#include <ntddk.h>

// このデバイスドライバのデバイス名
#define DEVICE_NAME     L"\\Device\\HOSTDRV"
#define DOS_DEVICE_NAME L"\\DosDevices\\HOSTDRV"

// エミュレータとの通信用
typedef struct tagHOSTDRV_INFO {
    PIO_STACK_LOCATION stack; // IoGetCurrentIrpStackLocation(Irp)で取得されるデータへのアドレス
    PIO_STATUS_BLOCK status; // Irp->IoStatusへのアドレス
    PVOID systemBuffer; // ゲストOS→エミュレータへのバッファ
    ULONG deviceFlags; // irpSp->DeviceObject->Flagsの値
    PVOID outBuffer; // エミュレータ→ゲストOSへのバッファ
    PVOID sectionObjectPointer; // irpSp->FileObject->SectionObjectPointerへのアドレス
    ULONG version; // エミュレータ通信バージョン情報
} HOSTDRV_INFO, *PHOSTDRV_INFO;

// irpSp->FileObject->FsContextへ格納する情報
// 参考情報：irpSp->FileObject->FsContextへ適当なIDを入れるのはNG。色々動かなくなる。
// 必ずExAllocatePoolWithTag(NonPagedPool, ～で割り当てたメモリである必要がある。
typedef struct tagHOSTDRV_FSCONTEXT {
    ULONG fileIndex; // エミュレータ本体が管理するファイルID
    ULONG reserved1; // 予約
    ULONG reserved2; // 予約
    ULONG reserved3; // 予約
} HOSTDRV_FSCONTEXT, *PHOSTDRV_FSCONTEXT;

// 高速I/Oの処理可否を返す関数。使わないので常時FALSEを返す。
BOOLEAN HostdrvFastIoCheckIfPossible (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    )
{
    return FALSE;
}

// グローバルに管理する変数群
static FAST_MUTEX g_Mutex; // I/Oの排他ロック用

// 関数定義
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject);

// レジストリキーをチェックしてリムーバブルデバイス扱いにするかを取得
BOOLEAN HostdrvIsRemovableDevice(IN PUNICODE_STRING RegistryPath) {
	OBJECT_ATTRIBUTES attributes;
	HANDLE hKey;
	NTSTATUS status;
	UNICODE_STRING valueName;
	ULONG resultLength;
	PKEY_VALUE_PARTIAL_INFORMATION pInfo;

	InitializeObjectAttributes(&attributes, RegistryPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
	
	status = ZwOpenKey(&hKey, KEY_READ, &attributes);
	if (!NT_SUCCESS(status)) {
	    return FALSE;
	}
	
	RtlInitUnicodeString(&valueName, L"IsRemovableDevice");
	status = ZwQueryValueKey(hKey, &valueName, KeyValuePartialInformation, NULL, 0, &resultLength);
	if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
	    return FALSE;
	}
	
	pInfo = ExAllocatePoolWithTag(PagedPool, resultLength, 'prmT');
	if (!pInfo) {
	    ZwClose(hKey);
	    return FALSE;
	}

	status = ZwQueryValueKey(hKey, &valueName, KeyValuePartialInformation, pInfo, resultLength, &resultLength);
	if (NT_SUCCESS(status) && pInfo->Type == REG_DWORD) {
	    if(*(ULONG *)pInfo->Data == 1){
			ExFreePool(pInfo);
			ZwClose(hKey);
	    	return TRUE;
		}
	}
	ExFreePool(pInfo);
	ZwClose(hKey);
	
	return FALSE;
}

// デバイスドライバのエントリポイント
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
    UNICODE_STRING deviceNameUnicodeString, dosDeviceNameUnicodeString;
    PDEVICE_OBJECT deviceObject = NULL;
    BOOLEAN isRemovableDevice = FALSE;
    DEVICE_TYPE deviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
    NTSTATUS status;
    int i;
    
    // hostdrv for NT対応かを簡易チェック
    if(READ_PORT_UCHAR((PUCHAR)0x7EC) != 98 || READ_PORT_UCHAR((PUCHAR)0x7EE) != 21){
        return STATUS_NO_SUCH_DEVICE;
	}

    // 排他ロック初期化　初期化のみで破棄処理はいらない
    ExInitializeFastMutex(&g_Mutex);
    
    // リムーバブルデバイスの振りをするモード
    isRemovableDevice = HostdrvIsRemovableDevice(RegistryPath);
    if(isRemovableDevice){
    	deviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
    }
    
    // デバイスを作成　ローカルディスクの振りをするならFILE_DEVICE_DISK_FILE_SYSTEM
    RtlInitUnicodeString(&deviceNameUnicodeString, DEVICE_NAME);
    status = IoCreateDevice(DriverObject, 0, &deviceNameUnicodeString,
                            deviceType, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // デバイスのDOS名を登録　\\.\HOSTDRVのようにアクセスできる
    RtlInitUnicodeString(&dosDeviceNameUnicodeString, DOS_DEVICE_NAME);
    status = IoCreateSymbolicLink(&dosDeviceNameUnicodeString, &deviceNameUnicodeString);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    // デバイスのIRP処理関数を登録　要素番号が各IRP_MJ_～の番号に対応。
    // 全部エミュレータ本体に投げるので全部に同じ関数を割り当て
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = HostdrvDispatch;
    }

    // ドライバの終了処理を登録
    DriverObject->DriverUnload = HostdrvUnload;
    
    if(isRemovableDevice){
	    // ごみ箱を使わせないためにリムーバブルディスクの振りをする
	    deviceObject->Characteristics |= FILE_REMOVABLE_MEDIA;
    }else{
	    // ごみ箱を使わせないためにネットワークデバイスの振りをする
	    deviceObject->Characteristics |= FILE_REMOTE_DEVICE;
    }
    
    // その他追加フラグを設定
    deviceObject->Flags |= DO_BUFFERED_IO; // データ受け渡しでSystemBufferを基本とする？指定しても他方式で来るときは来る気がする。
    deviceObject->Flags |= DO_LOW_PRIORITY_FILESYSTEM; // 低優先度で処理

    KdPrint(("Hostdrv: Loaded successfully\n"));
    return STATUS_SUCCESS;
}

VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING dosDeviceNameUnicodeString;
    
    // DOS名を登録解除
    RtlInitUnicodeString(&dosDeviceNameUnicodeString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&dosDeviceNameUnicodeString);
    
    // デバイスを削除
    IoDeleteDevice(DriverObject->DeviceObject);
    
    KdPrint(("Hostdrv: Unloaded\n"));
}

NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    HOSTDRV_INFO hostdrvInfo;
    HOSTDRV_INFO *lpHostdrvInfo;
    ULONG hostdrvInfoAddr;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    // IRP_MJ_CREATEの時、必要なメモリを仮割り当て
    if(irpSp->MajorFunction == IRP_MJ_CREATE) {
        // FileObjectがNULLなのは異常なので弾く
        if(irpSp->FileObject == NULL){
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        }
        // FsContextとSectionObjectPointerにExAllocatePoolWithTag(NonPagedPool,～でメモリ割り当て
        // NULLのままだったり違う方法で割り当てると正常に動かないので注意
        irpSp->FileObject->FsContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(HOSTDRV_FSCONTEXT), "HSFC");
        RtlZeroMemory(irpSp->FileObject->FsContext, sizeof(HOSTDRV_FSCONTEXT));
        irpSp->FileObject->SectionObjectPointer = ExAllocatePoolWithTag(NonPagedPool, sizeof(SECTION_OBJECT_POINTERS), "HSOP");
        RtlZeroMemory(irpSp->FileObject->SectionObjectPointer, sizeof(SECTION_OBJECT_POINTERS));
    }
    
    // デフォルトのステータス設定
    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    Irp->IoStatus.Information = 0;
    
    // エミュレータ側に渡すデータ設定
    lpHostdrvInfo = &hostdrvInfo;
    lpHostdrvInfo->stack = irpSp;
    lpHostdrvInfo->status = &(Irp->IoStatus);
    lpHostdrvInfo->systemBuffer = Irp->AssociatedIrp.SystemBuffer;
    lpHostdrvInfo->deviceFlags = irpSp->DeviceObject->Flags;
    if (Irp->AssociatedIrp.SystemBuffer != NULL) {
        // システムバッファ経由での間接的な転送
        lpHostdrvInfo->outBuffer = Irp->AssociatedIrp.SystemBuffer;
    } else if (Irp->MdlAddress != NULL) {
        // MdlAddressを使った直接的な転送
        lpHostdrvInfo->outBuffer = MmGetSystemAddressForMdl(Irp->MdlAddress); // 古いOS対応用
        //lpHostdrvInfo->outBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority); // 最近のOSならこれも可能（より安全？）
    } else {
        // ユーザー指定バッファへの転送
        lpHostdrvInfo->outBuffer = Irp->UserBuffer;
    }
    if (irpSp->FileObject) {
        lpHostdrvInfo->sectionObjectPointer = irpSp->FileObject->SectionObjectPointer;
    } else {
        lpHostdrvInfo->sectionObjectPointer = NULL;
    }
    lpHostdrvInfo->version = 1;
    
    // 構造体アドレスを書き込んでエミュレータで処理させる（ハイパーバイザーコール）
    // エミュレータ側でステータスやバッファなどの値がセットされる
    ExAcquireFastMutex(&g_Mutex);
    hostdrvInfoAddr = (ULONG)lpHostdrvInfo;
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)(hostdrvInfoAddr));
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)(hostdrvInfoAddr >> 8));
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)(hostdrvInfoAddr >> 16));
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)(hostdrvInfoAddr >> 24));
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'H');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'D');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'R');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'9');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'8');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'0');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'1');
    ExReleaseFastMutex(&g_Mutex);
    
    // ファイルオープン・クローズなどで割り当てたメモリの処理
    if(Irp->IoStatus.Status == STATUS_SUCCESS){
        // ファイルを閉じたので破棄
        if(irpSp->MajorFunction == IRP_MJ_CLOSE) {
            if(irpSp->FileObject->SectionObjectPointer){
                ExFreePool(irpSp->FileObject->SectionObjectPointer);
                irpSp->FileObject->SectionObjectPointer = NULL;
                ExFreePool(irpSp->FileObject->FsContext);
                irpSp->FileObject->FsContext = NULL;
            }
        }
    }else{
        // 上手く行かなかったら破棄
        if(irpSp->MajorFunction == IRP_MJ_CREATE) {
            if(irpSp->FileObject->SectionObjectPointer){
                ExFreePool(irpSp->FileObject->SectionObjectPointer);
                irpSp->FileObject->SectionObjectPointer = NULL;
            }
            if(irpSp->FileObject->FsContext){
                ExFreePool(irpSp->FileObject->FsContext);
                irpSp->FileObject->FsContext = NULL;
            }
        }
    }
    
    //if(Irp->IoStatus.Status == STATUS_PENDING){
    //    IoMarkIrpPending(Irp);
    //}else{
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    //}
    return Irp->IoStatus.Status;
}
