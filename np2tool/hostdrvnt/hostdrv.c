#include <ntddk.h>

// このデバイスドライバのデバイス名
#define DEVICE_NAME     L"\\Device\\HOSTDRV"
#define DOS_DEVICE_NAME L"\\DosDevices\\HOSTDRV"

#define PENDING_IRP_MAX	256

#define HOSTDRVNTOPTIONS_NONE				0x0
#define HOSTDRVNTOPTIONS_REMOVABLEDEVICE	0x1
#define HOSTDRVNTOPTIONS_USEREALCAPACITY	0x2

// エミュレータとの通信用
typedef struct tagHOSTDRV_INFO {
    PIO_STACK_LOCATION stack; // IoGetCurrentIrpStackLocation(Irp)で取得されるデータへのアドレス
    PIO_STATUS_BLOCK status; // Irp->IoStatusへのアドレス
    PVOID systemBuffer; // ゲストOS→エミュレータへのバッファ
    ULONG deviceFlags; // irpSp->DeviceObject->Flagsの値
    PVOID outBuffer; // エミュレータ→ゲストOSへのバッファ
    PVOID sectionObjectPointer; // irpSp->FileObject->SectionObjectPointerへのアドレス
    ULONG version; // エミュレータ通信バージョン情報
    ULONG pendingListCount; // 待機用IRPのリストの要素数
    PIRP  *pendingIrpList; // 待機用IRPのリストへのアドレス
    ULONG *pendingAliveList; // 待機用生存フラグ（猫側ファイルオブジェクトインデックス）のリストへのアドレス
    union{
    	LONG pendingIndex; // STATUS_PENDINGのとき、どのインデックスに待機用IRPを追加するかを表す（猫側がセット）
    	LONG pendingCompleteCount; // STATUS_PENDING以外の時、待機完了したものの数を表す（猫側がセット）
    } pending;
    ULONG hostdrvNTOptions; // HOSTDRV for NTオプション
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
static PIRP g_pendingIrpList[PENDING_IRP_MAX] = {0}; // I/O待機用IRPのリスト
static ULONG g_pendingAliveList[PENDING_IRP_MAX] = {0}; // I/O待機用生存フラグのリスト
static ULONG g_hostdrvNTOptions = HOSTDRVNTOPTIONS_NONE; // HOSTDRV for NTオプション

// 関数定義
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject);
VOID HostdrvCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// レジストリのDWORD値を読む
ULONG HostdrvReadDWORDReg(HANDLE hKey, WCHAR *valueName) {
	NTSTATUS status;
	UNICODE_STRING valueNameUnicode;
	ULONG resultLength;
	PKEY_VALUE_PARTIAL_INFORMATION pInfo;
	
	RtlInitUnicodeString(&valueNameUnicode, valueName);
	status = ZwQueryValueKey(hKey, &valueNameUnicode, KeyValuePartialInformation, NULL, 0, &resultLength);
	if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
	    ZwClose(hKey);
	    return 0;
	}
	pInfo = ExAllocatePoolWithTag(PagedPool, resultLength, 'prmT');
	if (!pInfo) {
	    ZwClose(hKey);
	    return 0;
	}
	status = ZwQueryValueKey(hKey, &valueNameUnicode, KeyValuePartialInformation, pInfo, resultLength, &resultLength);
	if (NT_SUCCESS(status) && pInfo->Type == REG_DWORD) {
	    if(*(ULONG *)pInfo->Data == 1){
			ExFreePool(pInfo);
			ZwClose(hKey);
	    	return *(ULONG *)pInfo->Data;
		}
	}
	ExFreePool(pInfo);
	
	return 0;
}

// オプションのレジストリキーをチェック
VOID HostdrvCheckOptions(IN PUNICODE_STRING RegistryPath) {
	OBJECT_ATTRIBUTES attributes;
	HANDLE hKey;
	NTSTATUS status;

	InitializeObjectAttributes(&attributes, RegistryPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
	
	status = ZwOpenKey(&hKey, KEY_READ, &attributes);
	if (!NT_SUCCESS(status)) {
	    return;
	}
	
	if (HostdrvReadDWORDReg(hKey, L"IsRemovableDevice")){
		g_hostdrvNTOptions |= HOSTDRVNTOPTIONS_REMOVABLEDEVICE;
	}
	if (HostdrvReadDWORDReg(hKey, L"UseRealCapacity")){
		g_hostdrvNTOptions |= HOSTDRVNTOPTIONS_USEREALCAPACITY;
	}
	
	ZwClose(hKey);
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
	
    // hostdrv for NTをリセット
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)0x7EC, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'H');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'D');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'R');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'9');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'8');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'0');
    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'1');

    // 排他ロック初期化　初期化のみで破棄処理はいらない
    ExInitializeFastMutex(&g_Mutex);
    
    // オプションチェック
    HostdrvCheckOptions(RegistryPath);
    
    // リムーバブルデバイスの振りをするモード
    isRemovableDevice = (g_hostdrvNTOptions & HOSTDRVNTOPTIONS_REMOVABLEDEVICE);
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
    int i;
    
    // 待機中のIRPを全部キャンセルする
    for (i = 0; i < PENDING_IRP_MAX; i++) {
        if (g_pendingIrpList[i] != NULL) {
        	PIRP Irp = g_pendingIrpList[i];
		    g_pendingIrpList[i] = NULL;
		    g_pendingAliveList[i] = 0;
		    Irp->IoStatus.Status = STATUS_CANCELLED;
		    Irp->IoStatus.Information = 0;
		    IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }
    }
    
    // DOS名を登録解除
    RtlInitUnicodeString(&dosDeviceNameUnicodeString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&dosDeviceNameUnicodeString);
    
    // デバイスを削除
    IoDeleteDevice(DriverObject->DeviceObject);
    
    KdPrint(("Hostdrv: Unloaded\n"));
}

VOID HostdrvCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIRP cancelIrp = NULL;
    int i;
    
    // キャンセルのロックを解除
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    // 排他領域開始
    ExAcquireFastMutex(&g_Mutex);
    
	// 指定されたIRPを登録解除
    for (i = 0; i < PENDING_IRP_MAX; i++) {
    	if(g_pendingIrpList[i] == Irp){
    		g_pendingIrpList[i] = NULL;
    		g_pendingAliveList[i] = 0;
		    break;
    	}
    }
    
    // 排他領域終了
    ExReleaseFastMutex(&g_Mutex);
    
    // キャンセル実行
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    HOSTDRV_INFO hostdrvInfo;
    HOSTDRV_INFO *lpHostdrvInfo;
    ULONG hostdrvInfoAddr;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    BOOLEAN pending = FALSE;
	ULONG completeIrpCount = 0; // I/O待機で今回完了したものの数
	PIRP *completeIrpList = NULL; // I/O待機で今回完了したものIRPのリスト
	int i;

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
        if(irpSp->FileObject->FsContext == NULL){
		    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		    Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        }
        RtlZeroMemory(irpSp->FileObject->FsContext, sizeof(HOSTDRV_FSCONTEXT));
        irpSp->FileObject->SectionObjectPointer = ExAllocatePoolWithTag(NonPagedPool, sizeof(SECTION_OBJECT_POINTERS), "HSOP");
        if(irpSp->FileObject->SectionObjectPointer == NULL){
        	ExFreePool(irpSp->FileObject->FsContext);
		    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		    Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        }
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
    lpHostdrvInfo->version = 3;
    lpHostdrvInfo->pendingListCount = PENDING_IRP_MAX;
    lpHostdrvInfo->pendingIrpList = g_pendingIrpList;
    lpHostdrvInfo->pendingAliveList = g_pendingAliveList;
    lpHostdrvInfo->pending.pendingIndex = -1;
    lpHostdrvInfo->hostdrvNTOptions = g_hostdrvNTOptions;
    
    // 排他領域開始
    ExAcquireFastMutex(&g_Mutex);
    
    // 構造体アドレスを書き込んでエミュレータで処理させる（ハイパーバイザーコール）
    // エミュレータ側でステータスやバッファなどの値がセットされる
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
    if(Irp->IoStatus.Status == STATUS_PENDING){
    	// 待機希望
    	if(lpHostdrvInfo->pending.pendingIndex < 0 || PENDING_IRP_MAX <= lpHostdrvInfo->pending.pendingIndex){
    		// 登録できる場所がない
		    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		    Irp->IoStatus.Information = 0;
    	}else{
    		// 指定された場所へ登録
    		g_pendingIrpList[lpHostdrvInfo->pending.pendingIndex] = Irp;
    		pending = TRUE;
    	}
    }else if(lpHostdrvInfo->pending.pendingCompleteCount > 0){
    	// 待機解除が存在する
    	completeIrpCount = lpHostdrvInfo->pending.pendingCompleteCount;
        completeIrpList = ExAllocatePoolWithTag(NonPagedPool, sizeof(PIRP) * completeIrpCount, "HSIP"); // XXX: Mutex内のメモリ割り当ては安全か不明
        if(completeIrpList){
        	int ci = 0;
		    for (i = 0; i < PENDING_IRP_MAX; i++) {
		        if (g_pendingAliveList[i] == 0 && g_pendingIrpList[i] != NULL) {
		        	completeIrpList[ci] = g_pendingIrpList[i];
				    g_pendingIrpList[i] = NULL;
				    ci++;
				    if (ci==completeIrpCount) break;
		        }
		    }
        }
    }
    
    // 排他領域終了
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
    
    // 待機解除が存在する場合、それらを完了させる
    if(completeIrpList){
	    for (i = 0; i < completeIrpCount; i++) {
        	PIRP Irp = completeIrpList[i];
	        IoCompleteRequest(Irp, IO_NO_INCREMENT); // 全部猫側でセットされているので完了を呼ぶだけでよい
	    }
    	ExFreePool(completeIrpList);
    	completeIrpList = NULL;
    }
    
    if(pending){
    	KIRQL oldIrql;
    	
    	// 待機中にセット
        IoMarkIrpPending(Irp);
        
        IoAcquireCancelSpinLock(&oldIrql);  // 保護開始
        
	    // 既にキャンセル済みならすぐに処理
		if (Irp->Cancel) {
    		IoReleaseCancelSpinLock(oldIrql);  // 保護解除
	        
    		g_pendingIrpList[lpHostdrvInfo->pending.pendingIndex] = NULL;
    		g_pendingAliveList[lpHostdrvInfo->pending.pendingIndex] = 0;

	        Irp->IoStatus.Status = STATUS_CANCELLED;
	        Irp->IoStatus.Information = 0;
	        IoCompleteRequest(Irp, IO_NO_INCREMENT);
	    }else{
		    // 待機キャンセル要求登録
		    IoSetCancelRoutine(Irp, HostdrvCancelRoutine);
		    
    		IoReleaseCancelSpinLock(oldIrql);  // 保護解除
	    }
    }else{
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    return Irp->IoStatus.Status;
}
