#include <ntddk.h>

// このデバイスドライバのデバイス名
#define DEVICE_NAME     L"\\Device\\HOSTDRV"
#define DOS_DEVICE_NAME L"\\DosDevices\\HOSTDRV"
#define DOS_DRIVE_NAME  L"\\DosDevices\\Z:"

#define PENDING_IRP_MAX	256

#define HOSTDRVNTOPTIONS_NONE				0x0
#define HOSTDRVNTOPTIONS_REMOVABLEDEVICE	0x1
#define HOSTDRVNTOPTIONS_USEREALCAPACITY	0x2
#define HOSTDRVNTOPTIONS_USECHECKNOTIFY		0x4
#define HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE		0x8

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
typedef struct tagHOSTDRV_NOTIFYINFO {
    ULONG version; // エミュレータ通信バージョン情報
    ULONG pendingListCount; // 待機用IRPのリストの要素数
    PIRP  *pendingIrpList; // 待機用IRPのリストへのアドレス
    ULONG *pendingAliveList; // 待機用生存フラグ（猫側ファイルオブジェクトインデックス）のリストへのアドレス
    union{
    	LONG pendingCompleteCount; // 待機完了したものの数を表す（猫側がセット）
    } pending;
} HOSTDRV_NOTIFYINFO, *PHOSTDRV_NOTIFYINFO;

// irpSp->FileObject->FsContextへ格納する情報
// 参考情報：irpSp->FileObject->FsContextへ適当なIDを入れるのはNG。色々動かなくなる。
// 必ずExAllocatePoolWithTag(NonPagedPool, ～で割り当てたメモリである必要がある。
// [Undocumented] 構造体サイズは少なくとも64byteないと駄目？
// ないとOSが決め打ちの範囲外参照してIRQL_NOT_LESS_OR_EQUALが頻発。どこまであれば安全かは不明。
// FSRTL_COMMON_FCB_HEADERを構造体の最初に含めなければならないという条件が必須のようにも思えます。
typedef struct tagHOSTDRV_FSCONTEXT {
    ULONG fileIndex; // エミュレータ本体が管理するファイルID
    ULONG reserved[15]; // 予約
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
static ULONG g_checkNotifyInterval = 10; // ホストのファイルシステム変更通知を何秒間隔でチェックするか
static KTIMER g_checkNotifyTimer = {0}; // ホストのファイルシステム変更通知を監視するタイマー
static KDPC g_checkNotifyTimerDpc = {0}; // ホストのファイルシステム変更通知を監視するタイマーDPC
static WORK_QUEUE_ITEM g_RescheduleTimerWorkItem = {0}; // ホストのファイルシステム変更通知を監視するタイマー再起動用
static int g_checkNotifyTimerEnabled = 0; // ホストのファイルシステム変更通知を監視するタイマーが開始状態
static ULONG g_pendingCounter = 0; // ファイルシステム監視でSTATUS_PENDING状態のものの数
static WCHAR g_autoMountDriveLetter = 0; // 自動マウント時のドライブレター文字。0の場合はZから順に使える場所を探して割り当て。

// 関数定義
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject);
VOID HostdrvCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID HostdrvTimerDpcRoutine(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2);
VOID HostdrvRescheduleTimer(IN PVOID Context);

// レジストリのDWORD値を読む
ULONG HostdrvReadDWORDReg(HANDLE hKey, WCHAR *valueName) {
	NTSTATUS status;
	UNICODE_STRING valueNameUnicode;
	ULONG resultLength;
	PKEY_VALUE_PARTIAL_INFORMATION pInfo;
	
	RtlInitUnicodeString(&valueNameUnicode, valueName);
	status = ZwQueryValueKey(hKey, &valueNameUnicode, KeyValuePartialInformation, NULL, 0, &resultLength);
	if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
	    return 0;
	}
	pInfo = ExAllocatePoolWithTag(PagedPool, resultLength, 'prmT');
	if (!pInfo) {
	    return 0;
	}
	status = ZwQueryValueKey(hKey, &valueNameUnicode, KeyValuePartialInformation, pInfo, resultLength, &resultLength);
	if (NT_SUCCESS(status) && pInfo->Type == REG_DWORD) {
    	ULONG retValue = *(ULONG *)pInfo->Data;
		ExFreePool(pInfo);
    	return retValue;
	}
	ExFreePool(pInfo);
	
	return 0;
}
// レジストリのドライブ文字をあらわすREG_SZを読む
WCHAR HostdrvReadDriveLetterReg(HANDLE hKey, WCHAR *valueName) {
	NTSTATUS status;
	UNICODE_STRING valueNameUnicode;
	ULONG resultLength;
	PKEY_VALUE_PARTIAL_INFORMATION pInfo;
	
	RtlInitUnicodeString(&valueNameUnicode, valueName);
	status = ZwQueryValueKey(hKey, &valueNameUnicode, KeyValuePartialInformation, NULL, 0, &resultLength);
	if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
	    return 0;
	}
	pInfo = ExAllocatePoolWithTag(PagedPool, resultLength, 'prmT');
	if (!pInfo) {
	    return 0;
	}
	status = ZwQueryValueKey(hKey, &valueNameUnicode, KeyValuePartialInformation, pInfo, resultLength, &resultLength);
	if (NT_SUCCESS(status) && pInfo->Type == REG_SZ && pInfo->DataLength == 2 * sizeof(WCHAR)) { // NULL文字含むバイト数
    	WCHAR retValue = ((WCHAR *)(pInfo->Data))[0];
		ExFreePool(pInfo);
    	return retValue;
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
	if (HostdrvReadDWORDReg(hKey, L"UseCheckNotify")){
		g_hostdrvNTOptions |= HOSTDRVNTOPTIONS_USECHECKNOTIFY;
	}
	g_checkNotifyInterval = HostdrvReadDWORDReg(hKey, L"CheckNotifyInterval");
	if (g_checkNotifyInterval <= 0){
		g_checkNotifyInterval = 5; // デフォルト（5秒）にする
	}
	if (g_checkNotifyInterval > 60){
		g_checkNotifyInterval = 60; // 最大は60秒にする
	}
	if (HostdrvReadDWORDReg(hKey, L"AutoMount")){
		g_hostdrvNTOptions |= HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE;
	}
	g_autoMountDriveLetter = HostdrvReadDriveLetterReg(hKey, L"AutoMountDriveLetter");
	if('a' <= g_autoMountDriveLetter && g_autoMountDriveLetter <= 'z'){
		// 大文字とする
		g_autoMountDriveLetter = g_autoMountDriveLetter - 'a' + 'A';
	}
	if(!('A' <= g_autoMountDriveLetter && g_autoMountDriveLetter <= 'Z')){
		// 無効なので自動割り当てとする
		g_autoMountDriveLetter = 0;
	}
	
	ZwClose(hKey);
}

VOID HostdrvStartTimer()
{
	LARGE_INTEGER dueTime;
	
    if (g_checkNotifyTimerEnabled) {
        return;
    }
    
	g_checkNotifyTimerEnabled = 1;
	
	dueTime.QuadPart = (LONGLONG)(-(LONG)g_checkNotifyInterval * 1000 * 10000);
	KeSetTimer(&g_checkNotifyTimer, dueTime, &g_checkNotifyTimerDpc);
}

VOID HostdrvStopTimer()
{
    if (!g_checkNotifyTimerEnabled) {
        return;
    }
    
	KeCancelTimer(&g_checkNotifyTimer);
	
	g_checkNotifyTimerEnabled = 0;
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
    
    // 自動ドライブ文字割り当ての場合、割り当て
    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE){
		UNICODE_STRING dosDriveNameUnicodeString;
    	WCHAR dosDriveName[] = DOS_DRIVE_NAME;
		RtlInitUnicodeString(&dosDriveNameUnicodeString, dosDriveName);
    	if(g_autoMountDriveLetter== 0){
    		// 自動で使える文字を探す
    		for(g_autoMountDriveLetter='Z';g_autoMountDriveLetter>='A';g_autoMountDriveLetter--){
    			dosDriveNameUnicodeString.Buffer[12] = g_autoMountDriveLetter;
	    		status = IoCreateSymbolicLink(&dosDriveNameUnicodeString, &deviceNameUnicodeString);
    			if (NT_SUCCESS(status)) {
    				break;
    			}
    		}
    		if(g_autoMountDriveLetter < 'A'){
    			// 空きがないので割り当てできなかった
    			g_autoMountDriveLetter = 0;
    		}
    	}else{
    		// 固定文字指定　使えなくてもエラーにはしない
    		dosDriveNameUnicodeString.Buffer[12] = g_autoMountDriveLetter;
	    	status = IoCreateSymbolicLink(&dosDriveNameUnicodeString, &deviceNameUnicodeString);
		    if (!NT_SUCCESS(status)) {
		    	// 割り当て失敗
		        g_autoMountDriveLetter = 0;
		    }
    	}
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
    
    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_USECHECKNOTIFY){
    	KeInitializeTimer(&g_checkNotifyTimer);
		KeInitializeDpc(&g_checkNotifyTimerDpc, HostdrvTimerDpcRoutine, NULL);
        ExInitializeWorkItem(&g_RescheduleTimerWorkItem, HostdrvRescheduleTimer, NULL);
	}

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
	g_pendingCounter = 0;
    
    HostdrvStopTimer();
    
    // 自動ドライブ文字割り当ての場合、解除
    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE){
    	if(g_autoMountDriveLetter != 0){
			UNICODE_STRING dosDriveNameUnicodeString;
	    	WCHAR dosDriveName[] = DOS_DRIVE_NAME;
			RtlInitUnicodeString(&dosDriveNameUnicodeString, dosDriveName);
			dosDriveNameUnicodeString.Buffer[12] = g_autoMountDriveLetter;
    		IoDeleteSymbolicLink(&dosDriveNameUnicodeString);
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
    int i;
    
    // 待機キャンセル要求登録解除
    //IoSetCancelRoutine(Irp, NULL);
    Irp->CancelRoutine = NULL;
    
    // キャンセルのロックを解除
    IoReleaseCancelSpinLock(Irp->CancelIrql);
    
    // 排他領域開始
    ExAcquireFastMutex(&g_Mutex);
    
	// 指定されたIRPを登録解除
    for (i = 0; i < PENDING_IRP_MAX; i++) {
    	if(g_pendingIrpList[i] == Irp){
    		g_pendingIrpList[i] = NULL;
    		g_pendingAliveList[i] = 0;
			if(g_pendingCounter > 0) g_pendingCounter--;
		    break;
    	}
    }
    
	if(g_pendingCounter == 0){
		// 待機中がなければ動かす必要なし
		HostdrvStopTimer();
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
	PIRP *completeIrpList = NULL; // I/O待機で今回完了したIRPのリスト
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
			g_pendingCounter++;
			// 監視タイマーを動かす
		    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_USECHECKNOTIFY){
		    	HostdrvStartTimer();
			}
    	}
    }else if(lpHostdrvInfo->pending.pendingCompleteCount > 0){
    	// 待機解除が存在する
    	completeIrpCount = lpHostdrvInfo->pending.pendingCompleteCount;
        completeIrpList = ExAllocatePoolWithTag(NonPagedPool, sizeof(PIRP) * completeIrpCount, "HSIP");
        if(completeIrpList){
        	int ci = 0;
		    for (i = 0; i < PENDING_IRP_MAX; i++) {
		        if (g_pendingAliveList[i] == 0 && g_pendingIrpList[i] != NULL) {
		        	completeIrpList[ci] = g_pendingIrpList[i];
				    g_pendingIrpList[i] = NULL;
				    if(g_pendingCounter > 0) g_pendingCounter--;
				    ci++;
				    if (ci==completeIrpCount) break;
		        }
		    }
        }
		if(g_pendingCounter == 0){
			// 待機中がなければ動かす必要なし
			HostdrvStopTimer();
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
	        
			if(g_pendingCounter > 0) g_pendingCounter--;
	    	if(g_pendingCounter == 0){
	    		// 待機中がなければ動かす必要なし
	    		HostdrvStopTimer();
	    	}
	    }else{
		    // 待機キャンセル要求登録
		    //IoSetCancelRoutine(Irp, HostdrvCancelRoutine);
    		Irp->CancelRoutine = HostdrvCancelRoutine;
		    
    		IoReleaseCancelSpinLock(oldIrql);  // 保護解除
	    }
    }else{
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    return Irp->IoStatus.Status;
}

VOID HostdrvTimerDpcRoutine(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    if (!g_checkNotifyTimerEnabled) {
        return;
    }
    
	if(g_pendingCounter > 0 && g_checkNotifyTimerEnabled) {
    	// WorkItemへ投げる IRQLを落とす
    	ExQueueWorkItem(&g_RescheduleTimerWorkItem, DelayedWorkQueue);
	}else{
		// 停止
		g_checkNotifyTimerEnabled = 0;
	}
}
VOID HostdrvRescheduleTimer(IN PVOID Context)
{
	PHOSTDRV_NOTIFYINFO lpHostdrvInfo;
	ULONG hostdrvInfoAddr;
	ULONG completeIrpCount = 0; // I/O待機で今回完了したものの数
	PIRP *completeIrpList = NULL; // I/O待機で今回完了したIRPのリスト
	int i;
   
    lpHostdrvInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(HOSTDRV_NOTIFYINFO), "HSIM");
    
	// 排他領域開始
    ExAcquireFastMutex(&g_Mutex);
    
    if(lpHostdrvInfo){
    	// エミュレータ側に渡すデータ設定
	    lpHostdrvInfo->version = 3;
	    lpHostdrvInfo->pendingListCount = PENDING_IRP_MAX;
	    lpHostdrvInfo->pendingIrpList = g_pendingIrpList;
	    lpHostdrvInfo->pendingAliveList = g_pendingAliveList;
	    lpHostdrvInfo->pending.pendingCompleteCount = -1;
	    
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
	    WRITE_PORT_UCHAR((PUCHAR)0x7EE, (UCHAR)'M');
	    if(lpHostdrvInfo->pending.pendingCompleteCount > 0){
	    	// 待機解除が存在する
	    	completeIrpCount = lpHostdrvInfo->pending.pendingCompleteCount;
	        completeIrpList = ExAllocatePoolWithTag(NonPagedPool, sizeof(PIRP) * completeIrpCount, "HSIP");
	        if(completeIrpList){
	        	int ci = 0;
			    for (i = 0; i < PENDING_IRP_MAX; i++) {
			        if (g_pendingAliveList[i] == 0 && g_pendingIrpList[i] != NULL) {
			        	completeIrpList[ci] = g_pendingIrpList[i];
					    g_pendingIrpList[i] = NULL;
					    if(g_pendingCounter > 0) g_pendingCounter--;
					    ci++;
					    if (ci==completeIrpCount) break;
			        }
			    }
	        }
	    }
    }

    // タイマー再設定
	if(g_pendingCounter > 0 && g_checkNotifyTimerEnabled) {
    	// 再設定
		LARGE_INTEGER dueTime = {0};
		dueTime.QuadPart = (LONGLONG)(-(LONG)g_checkNotifyInterval * 1000 * 10000);
		KeSetTimer(&g_checkNotifyTimer, dueTime, &g_checkNotifyTimerDpc);
	}else{
		// 停止
		g_checkNotifyTimerEnabled = 0;
	}
    
    // 排他領域終了
    ExReleaseFastMutex(&g_Mutex);
    
    if(lpHostdrvInfo){
    	ExFreePool(lpHostdrvInfo);
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
}
