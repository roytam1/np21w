#include <ntddk.h>
#include "miniifs.h"
#include "minisop.h"
#include "irplock.h"

// NonPagedPool�ւ̃R�s�[�ł͂Ȃ��y�[�W���b�N���g��
#define USE_FAST_IRPSTACKLOCK

#define HOSTDRVNT_IO_ADDR	0x7EC
#define HOSTDRVNT_IO_CMD	0x7EE

// ���̃f�o�C�X�h���C�o�̃f�o�C�X��
#define DEVICE_NAME     L"\\Device\\HOSTDRV"
#define DOS_DEVICE_NAME L"\\DosDevices\\HOSTDRV"
#define DOS_DRIVE_NAME  L"\\DosDevices\\Z:"

#define PENDING_IRP_MAX	256

#define HOSTDRVNTOPTIONS_NONE				0x0
#define HOSTDRVNTOPTIONS_REMOVABLEDEVICE	0x1
#define HOSTDRVNTOPTIONS_USEREALCAPACITY	0x2
#define HOSTDRVNTOPTIONS_USECHECKNOTIFY		0x4
#define HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE		0x8
#define HOSTDRVNTOPTIONS_DISKDEVICE			0x10

#define HOSTDRVNT_VERSION		4

// �G�~�����[�^�Ƃ̒ʐM�p
typedef struct tagHOSTDRV_INFO {
    PIO_STACK_LOCATION stack; // IoGetCurrentIrpStackLocation(Irp)�Ŏ擾�����f�[�^�ւ̃A�h���X
    PIO_STATUS_BLOCK status; // Irp->IoStatus�ւ̃A�h���X
    PVOID systemBuffer; // �Q�X�gOS���G�~�����[�^�ւ̃o�b�t�@
    ULONG deviceFlags; // irpSp->DeviceObject->Flags�̒l
    PVOID outBuffer; // �G�~�����[�^���Q�X�gOS�ւ̃o�b�t�@
    PVOID sectionObjectPointer; // irpSp->FileObject->SectionObjectPointer�ւ̃A�h���X
    ULONG version; // �G�~�����[�^�ʐM�o�[�W�������
    ULONG pendingListCount; // �ҋ@�pIRP�̃��X�g�̗v�f��
    PIRP  *pendingIrpList; // �ҋ@�pIRP�̃��X�g�ւ̃A�h���X
    ULONG *pendingAliveList; // �ҋ@�p�����t���O�i�L���t�@�C���I�u�W�F�N�g�C���f�b�N�X�j�̃��X�g�ւ̃A�h���X
    union{
    	LONG pendingIndex; // STATUS_PENDING�̂Ƃ��A�ǂ̃C���f�b�N�X�ɑҋ@�pIRP��ǉ����邩��\���i�L�����Z�b�g�j
    	LONG pendingCompleteCount; // STATUS_PENDING�ȊO�̎��A�ҋ@�����������̂̐���\���i�L�����Z�b�g�j
    } pending;
    ULONG hostdrvNTOptions; // HOSTDRV for NT�I�v�V����
} HOSTDRV_INFO, *PHOSTDRV_INFO;
typedef struct tagHOSTDRV_NOTIFYINFO {
    ULONG version; // �G�~�����[�^�ʐM�o�[�W�������
    ULONG pendingListCount; // �ҋ@�pIRP�̃��X�g�̗v�f��
    PIRP  *pendingIrpList; // �ҋ@�pIRP�̃��X�g�ւ̃A�h���X
    ULONG *pendingAliveList; // �ҋ@�p�����t���O�i�L���t�@�C���I�u�W�F�N�g�C���f�b�N�X�j�̃��X�g�ւ̃A�h���X
    union{
    	LONG pendingCompleteCount; // �ҋ@�����������̂̐���\���i�L�����Z�b�g�j
    } pending;
} HOSTDRV_NOTIFYINFO, *PHOSTDRV_NOTIFYINFO;

// irpSp->FileObject->FsContext�֊i�[������
// �Q�l���FirpSp->FileObject->FsContext�֓K����ID������̂�NG�B�F�X�����Ȃ��Ȃ�B
// �K��ExAllocatePoolWithTag(NonPagedPool, �`�Ŋ��蓖�Ă��������ł���K�v������B
// [Undocumented] �\���̃T�C�Y�͏��Ȃ��Ƃ�64byte�Ȃ��ƑʖځH
// �Ȃ���OS�����ߑł��͈̔͊O�Q�Ƃ���IRQL_NOT_LESS_OR_EQUAL���p���B�ǂ��܂ł���Έ��S���͕s���B
// FSRTL_COMMON_FCB_HEADER���\���̂̍ŏ��Ɋ܂߂Ȃ���΂Ȃ�Ȃ��Ƃ����������K�{�̂悤�ɂ��v���܂��B
typedef struct tagHOSTDRV_FSCONTEXT {
	FSRTL_COMMON_FCB_HEADER header; // OS���g���̂ŐG���Ă͂����Ȃ��B32bit���ł�40byte
    ULONG fileIndex; // �G�~�����[�^�{�̂��Ǘ�����t�@�C��ID
    ULONG reserved[5]; // �\��
} HOSTDRV_FSCONTEXT, *PHOSTDRV_FSCONTEXT;

// �O���[�o���ɊǗ�����ϐ��Q
static FAST_MUTEX g_Mutex; // I/O�̔r�����b�N�p
static PIRP g_pendingIrpList[PENDING_IRP_MAX] = {0}; // I/O�ҋ@�pIRP�̃��X�g
static ULONG g_pendingAliveList[PENDING_IRP_MAX] = {0}; // I/O�ҋ@�p�����t���O�̃��X�g
static ULONG g_hostdrvNTOptions = HOSTDRVNTOPTIONS_NONE; // HOSTDRV for NT�I�v�V����
static ULONG g_checkNotifyInterval = 10; // �z�X�g�̃t�@�C���V�X�e���ύX�ʒm�����b�Ԋu�Ń`�F�b�N���邩
static KTIMER g_checkNotifyTimer = {0}; // �z�X�g�̃t�@�C���V�X�e���ύX�ʒm���Ď�����^�C�}�[
static KDPC g_checkNotifyTimerDpc = {0}; // �z�X�g�̃t�@�C���V�X�e���ύX�ʒm���Ď�����^�C�}�[DPC
static WORK_QUEUE_ITEM g_RescheduleTimerWorkItem = {0}; // �z�X�g�̃t�@�C���V�X�e���ύX�ʒm���Ď�����^�C�}�[�ċN���p
static int g_checkNotifyTimerEnabled = 0; // �z�X�g�̃t�@�C���V�X�e���ύX�ʒm���Ď�����^�C�}�[���J�n���
static ULONG g_pendingCounter = 0; // �t�@�C���V�X�e���Ď���STATUS_PENDING��Ԃ̂��̂̐�
static WCHAR g_autoMountDriveLetter = 0; // �����}�E���g���̃h���C�u���^�[�����B0�̏ꍇ��Z���珇�Ɏg����ꏊ��T���Ċ��蓖�āB

// �֐���`
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject);
VOID HostdrvCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID HostdrvTimerDpcRoutine(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2);
VOID HostdrvRescheduleTimer(IN PVOID Context);

// ���W�X�g����DWORD�l��ǂ�
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
// ���W�X�g���̃h���C�u����������킷REG_SZ��ǂ�
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
	if (NT_SUCCESS(status) && pInfo->Type == REG_SZ && pInfo->DataLength == 2 * sizeof(WCHAR)) { // NULL�����܂ރo�C�g��
    	WCHAR retValue = ((WCHAR *)(pInfo->Data))[0];
		ExFreePool(pInfo);
    	return retValue;
	}
	ExFreePool(pInfo);
	
	return 0;
}

// �I�v�V�����̃��W�X�g���L�[���`�F�b�N
VOID HostdrvCheckOptions(IN PUNICODE_STRING RegistryPath) {
	OBJECT_ATTRIBUTES attributes;
	HANDLE hKey;
	NTSTATUS status;

	InitializeObjectAttributes(&attributes, RegistryPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
	
	status = ZwOpenKey(&hKey, KEY_READ, &attributes);
	if (!NT_SUCCESS(status)) {
	    return;
	}
	
	if (HostdrvReadDWORDReg(hKey, L"IsDiskDevice")){
		g_hostdrvNTOptions |= HOSTDRVNTOPTIONS_DISKDEVICE;
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
		g_checkNotifyInterval = 5; // �f�t�H���g�i5�b�j�ɂ���
	}
	if (g_checkNotifyInterval > 60){
		g_checkNotifyInterval = 60; // �ő��60�b�ɂ���
	}
	if (HostdrvReadDWORDReg(hKey, L"AutoMount")){
		g_hostdrvNTOptions |= HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE;
	}
	g_autoMountDriveLetter = HostdrvReadDriveLetterReg(hKey, L"AutoMountDriveLetter");
	if('a' <= g_autoMountDriveLetter && g_autoMountDriveLetter <= 'z'){
		// �啶���Ƃ���
		g_autoMountDriveLetter = g_autoMountDriveLetter - 'a' + 'A';
	}
	if(!('A' <= g_autoMountDriveLetter && g_autoMountDriveLetter <= 'Z')){
		// �����Ȃ̂Ŏ������蓖�ĂƂ���
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

NTSTATUS ReserveIoPortRange(PDRIVER_OBJECT DriverObject)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pResourceDescriptor;
    PCM_PARTIAL_RESOURCE_LIST pPartialResourceList;
    PCM_FULL_RESOURCE_DESCRIPTOR pFullResourceDescriptor;
    PCM_RESOURCE_LIST pResourceList;
    ULONG listSize;
    UNICODE_STRING className;

    BOOLEAN conflictDetected = FALSE;
    NTSTATUS status;
    
    listSize = sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
    pResourceList = ExAllocatePoolWithTag(PagedPool, listSize, 'resl');
    if(!pResourceList){
    	return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(pResourceList, listSize);
    pResourceList->Count = 1;

	pFullResourceDescriptor = &(pResourceList->List[0]);
    pFullResourceDescriptor->InterfaceType = Internal;
    pFullResourceDescriptor->BusNumber = 0;
    
    pPartialResourceList = &(pFullResourceDescriptor->PartialResourceList);
    pPartialResourceList->Version = 1;
    pPartialResourceList->Revision = 1;
    pPartialResourceList->Count = 2;
    
    pResourceDescriptor = &(pPartialResourceList->PartialDescriptors[0]);
    pResourceDescriptor->Type = CmResourceTypePort;
    pResourceDescriptor->ShareDisposition = CmResourceShareDriverExclusive;
    pResourceDescriptor->Flags = CM_RESOURCE_PORT_IO | CM_RESOURCE_PORT_16_BIT_DECODE;
    pResourceDescriptor->u.Port.Start.QuadPart = HOSTDRVNT_IO_ADDR;
    pResourceDescriptor->u.Port.Length = 1;
    
    pResourceDescriptor = &(pPartialResourceList->PartialDescriptors[1]);
    pResourceDescriptor->Type = CmResourceTypePort;
    pResourceDescriptor->ShareDisposition = CmResourceShareDriverExclusive;
    pResourceDescriptor->Flags = CM_RESOURCE_PORT_IO | CM_RESOURCE_PORT_16_BIT_DECODE;
    pResourceDescriptor->u.Port.Start.QuadPart = HOSTDRVNT_IO_CMD;
    pResourceDescriptor->u.Port.Length = 1;
    
    // ���\�[�X�g�p�̕�
	RtlInitUnicodeString(&className, L"LegacyDriver");
    status = IoReportResourceUsage(
        &className,           // DriverClassName
        DriverObject,         // OwningDriverObject
        pResourceList,        // ResourceList
        listSize,             // ResourceListSize
        NULL,                 // PhysicalDeviceObject
        NULL,                 // ConflictList
        0,                    // ConflictCount
        FALSE,                // ArbiterRequest
        &conflictDetected     // ConflictDetected
    );
    
	ExFreePool(pResourceList);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (conflictDetected) {
        return STATUS_CONFLICTING_ADDRESSES;
    }

    return STATUS_SUCCESS;
}

// �f�o�C�X�h���C�o�̃G���g���|�C���g
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
    UNICODE_STRING deviceNameUnicodeString, dosDeviceNameUnicodeString;
    PDEVICE_OBJECT deviceObject = NULL;
    DEVICE_TYPE deviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
    ULONG deviceCharacteristics = FILE_DEVICE_IS_MOUNTED;
    NTSTATUS status;
    int i;
    
    // I/O�|�[�g���g���邩���m�F
    status = ReserveIoPortRange(DriverObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // hostdrv for NT�Ή������ȈՃ`�F�b�N
    if(READ_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR) != 98 || READ_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD) != 21){
        return STATUS_NO_SUCH_DEVICE;
	}
	
    // hostdrv for NT�����Z�b�g
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)0);
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'H');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'D');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'R');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'9');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'8');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'0');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'1');

    // �r�����b�N�������@�������݂̂Ŕj�������͂���Ȃ�
    ExInitializeFastMutex(&g_Mutex);
    
    // �I�v�V�����`�F�b�N
    HostdrvCheckOptions(RegistryPath);
    
    // �f�o�C�X�^�C�v�Ȃǂ̐ݒ�
    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_REMOVABLEDEVICE){
    	// �����[�o�u���f�o�C�X�̐U������郂�[�h
    	deviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
    	deviceCharacteristics |= FILE_REMOVABLE_MEDIA;
    }else if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_DISKDEVICE){
    	// ���[�J���f�B�X�N�̐U������郂�[�h
    	deviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
    }else{
    	// �l�b�g���[�N�t�@�C���V�X�e���̐U������郂�[�h
    	deviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
    	deviceCharacteristics |= FILE_REMOTE_DEVICE;
    }
    
    // �f�o�C�X���쐬�@���[�J���f�B�X�N�̐U�������Ȃ�FILE_DEVICE_DISK_FILE_SYSTEM
    RtlInitUnicodeString(&deviceNameUnicodeString, DEVICE_NAME);
    status = IoCreateDevice(DriverObject, 0, &deviceNameUnicodeString,
                            deviceType, deviceCharacteristics, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // �f�o�C�X��DOS����o�^�@\\.\HOSTDRV�̂悤�ɃA�N�Z�X�ł���
    RtlInitUnicodeString(&dosDeviceNameUnicodeString, DOS_DEVICE_NAME);
    status = IoCreateSymbolicLink(&dosDeviceNameUnicodeString, &deviceNameUnicodeString);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }
    
    // �����h���C�u�������蓖�Ă̏ꍇ�A���蓖��
    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE){
		UNICODE_STRING dosDriveNameUnicodeString;
    	WCHAR dosDriveName[] = DOS_DRIVE_NAME;
		RtlInitUnicodeString(&dosDriveNameUnicodeString, dosDriveName);
    	if(g_autoMountDriveLetter== 0){
    		// �����Ŏg���镶����T��
    		for(g_autoMountDriveLetter='Z';g_autoMountDriveLetter>='A';g_autoMountDriveLetter--){
    			dosDriveNameUnicodeString.Buffer[12] = g_autoMountDriveLetter;
	    		status = IoCreateSymbolicLink(&dosDriveNameUnicodeString, &deviceNameUnicodeString);
    			if (NT_SUCCESS(status)) {
    				break;
    			}
    		}
    		if(g_autoMountDriveLetter < 'A'){
    			// �󂫂��Ȃ��̂Ŋ��蓖�Ăł��Ȃ�����
    			g_autoMountDriveLetter = 0;
    		}
    	}else{
    		// �Œ蕶���w��@�g���Ȃ��Ă��G���[�ɂ͂��Ȃ�
    		dosDriveNameUnicodeString.Buffer[12] = g_autoMountDriveLetter;
	    	status = IoCreateSymbolicLink(&dosDriveNameUnicodeString, &deviceNameUnicodeString);
		    if (!NT_SUCCESS(status)) {
		    	// ���蓖�Ď��s
		        g_autoMountDriveLetter = 0;
		    }
    	}
    }

    // �f�o�C�X��IRP�����֐���o�^�@�v�f�ԍ����eIRP_MJ_�`�̔ԍ��ɑΉ��B
    // �S���G�~�����[�^�{�̂ɓ�����̂őS���ɓ����֐������蓖��
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = HostdrvDispatch;
    }

    // �h���C�o�̏I��������o�^
    DriverObject->DriverUnload = HostdrvUnload;
    
    // �L���b�V���������ł��������}�b�v�g�t�@�C�����g����悤�ɂ���WORKAROUND�L���b�V����������
    MiniSOP_InitializeCache(DriverObject);
    
    // ���̑��ǉ��t���O��ݒ�
    deviceObject->Flags |= DO_BUFFERED_IO; // �f�[�^�󂯓n����SystemBuffer����{�Ƃ���H�w�肵�Ă��������ŗ���Ƃ��͗���C������B
    
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
    
    // �ҋ@����IRP��S���L�����Z������
    for (i = 0; i < PENDING_IRP_MAX; i++) {
        if (g_pendingIrpList[i] != NULL) {
    		KIRQL oldIrql;
        	PIRP Irp = g_pendingIrpList[i];
		    g_pendingIrpList[i] = NULL;
		    g_pendingAliveList[i] = 0;
        	IoAcquireCancelSpinLock(&oldIrql);
        	if(Irp->CancelRoutine){
				Irp->CancelRoutine = NULL;
    			IoReleaseCancelSpinLock(Irp->CancelIrql);
			    Irp->IoStatus.Status = STATUS_CANCELLED;
			    Irp->IoStatus.Information = 0;
	        	IoCompleteRequest(Irp, IO_NO_INCREMENT); // �L�����Z������
        	}else{
    			IoReleaseCancelSpinLock(Irp->CancelIrql); // ���̂��L�����Z���ς݁@�ʏ�͂Ȃ��͂�
        	}
        }
    }
	g_pendingCounter = 0;
    
    // �r���̈�J�n
    ExAcquireFastMutex(&g_Mutex);
    
    HostdrvStopTimer();
    
    // SOP���X�g���
    MiniSOP_ReleaseSOPList();
    
    // �r���̈�I��
    ExReleaseFastMutex(&g_Mutex);
    
    // �����h���C�u�������蓖�Ă̏ꍇ�A����
    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_AUTOMOUNTDRIVE){
    	if(g_autoMountDriveLetter != 0){
			UNICODE_STRING dosDriveNameUnicodeString;
	    	WCHAR dosDriveName[] = DOS_DRIVE_NAME;
			RtlInitUnicodeString(&dosDriveNameUnicodeString, dosDriveName);
			dosDriveNameUnicodeString.Buffer[12] = g_autoMountDriveLetter;
    		IoDeleteSymbolicLink(&dosDriveNameUnicodeString);
		}
    }
    
    // DOS����o�^����
    RtlInitUnicodeString(&dosDeviceNameUnicodeString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&dosDeviceNameUnicodeString);
    
    // �f�o�C�X���폜
    IoDeleteDevice(DriverObject->DeviceObject);
    
    KdPrint(("Hostdrv: Unloaded\n"));
}

VOID HostdrvCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    int i;
    
    // �ҋ@�L�����Z���v���o�^����
    //IoSetCancelRoutine(Irp, NULL); // ��OS�œ����Ȃ����{���͂����炪����
    Irp->CancelRoutine = NULL;
    
    // �L�����Z���̃��b�N������
    IoReleaseCancelSpinLock(Irp->CancelIrql);
    
    // �r���̈�J�n
    ExAcquireFastMutex(&g_Mutex);
    
	// �w�肳�ꂽIRP��o�^����
    for (i = 0; i < PENDING_IRP_MAX; i++) {
    	if(g_pendingIrpList[i] == Irp){
    		g_pendingIrpList[i] = NULL;
    		g_pendingAliveList[i] = 0;
			if(g_pendingCounter > 0) g_pendingCounter--;
		    break;
    	}
    }
    
	if(g_pendingCounter == 0){
		// �ҋ@�����Ȃ���Γ������K�v�Ȃ�
		HostdrvStopTimer();
	}
    
    // �r���̈�I��
    ExReleaseFastMutex(&g_Mutex);
    
    // �L�����Z�����s
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
}

NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    HOSTDRV_INFO hostdrvInfo;
    HOSTDRV_INFO *lpHostdrvInfo;
    ULONG hostdrvInfoAddr;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
#ifdef USE_FAST_IRPSTACKLOCK
	IRPSTACKLOCK_INFO irpLockInfo;
#else
    PIO_STACK_LOCATION irpSpNPP = NULL;
    PIO_STACK_LOCATION irpSpNPPBefore = NULL;
#endif
    BOOLEAN pending = FALSE;
	ULONG completeIrpCount = 0; // I/O�ҋ@�ō��񊮗��������̂̐�
	PIRP *completeIrpList = NULL; // I/O�ҋ@�ō��񊮗�����IRP�̃��X�g
	ULONG sopIndex = -1;
	NTSTATUS status;
	int i;

    // IRP_MJ_CREATE�̎��A�K�v�ȃ������������蓖��
    if(irpSp->MajorFunction == IRP_MJ_CREATE) {
        // FileObject��NULL�Ȃُ͈̂�Ȃ̂Œe��
        if(irpSp->FileObject == NULL){
            Irp->IoStatus.Status = status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
        }
        // FsContext��SectionObjectPointer��ExAllocatePoolWithTag(NonPagedPool,�`�Ń��������蓖��
        // NULL�̂܂܂�������Ⴄ���@�Ŋ��蓖�Ă�Ɛ���ɓ����Ȃ��̂Œ���
        irpSp->FileObject->FsContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(HOSTDRV_FSCONTEXT), "HSFC");
        if(irpSp->FileObject->FsContext == NULL){
		    Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
		    Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
        }
        RtlZeroMemory(irpSp->FileObject->FsContext, sizeof(HOSTDRV_FSCONTEXT));
        
    	// SOP�擾
	    ExAcquireFastMutex(&g_Mutex);
    	sopIndex = MiniSOP_GetSOPIndex(irpSp->FileObject->FileName);
	    ExReleaseFastMutex(&g_Mutex);
	    
    	if(sopIndex == -1){
        	ExFreePool(irpSp->FileObject->FsContext);
		    Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
		    Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
		}
		irpSp->FileObject->SectionObjectPointer = MiniSOP_GetSOP(sopIndex);
    }
    
    // �f�t�H���g�̃X�e�[�^�X�ݒ�
    Irp->IoStatus.Status = status = STATUS_NOT_IMPLEMENTED;
    Irp->IoStatus.Information = 0;
    
    // �G�~�����[�^���ɓn���f�[�^�ݒ�
#ifdef USE_FAST_IRPSTACKLOCK
	// �y�[�W�A�E�g���Ȃ��悤�Ƀ��b�N
    irpLockInfo = LockIrpStack(irpSp);
    if(!irpLockInfo.isValid){
        Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
#else
	// �y�[�W�A�E�g���Ȃ��̈�փR�s�[
    irpSpNPP = CreateNonPagedPoolIrpStack(irpSp); // NonPaged�R�s�[�쐬
    if(irpSpNPP == NULL){
        Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
	}
	irpSpNPPBefore = irpSp;
	irpSp = irpSpNPP; // �|�C���^��������
#endif
    lpHostdrvInfo = &hostdrvInfo;
    lpHostdrvInfo->stack = irpSp;
    lpHostdrvInfo->status = &(Irp->IoStatus);
    lpHostdrvInfo->systemBuffer = Irp->AssociatedIrp.SystemBuffer;
    lpHostdrvInfo->deviceFlags = irpSp->DeviceObject->Flags;
    if (Irp->MdlAddress != NULL) {
        // MdlAddress���g�������ړI�ȓ]��
        lpHostdrvInfo->outBuffer = MmGetSystemAddressForMdl(Irp->MdlAddress); // �Â�OS�Ή��p
        //lpHostdrvInfo->outBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority); // �ŋ߂�OS�Ȃ炱����\�i�����S�H�j
        if(lpHostdrvInfo->systemBuffer == NULL){
        	lpHostdrvInfo->systemBuffer = lpHostdrvInfo->outBuffer;
        }
    } else if (Irp->AssociatedIrp.SystemBuffer != NULL) {
        // �V�X�e���o�b�t�@�o�R�ł̊ԐړI�ȓ]��
        lpHostdrvInfo->outBuffer = Irp->AssociatedIrp.SystemBuffer;
    } else {
        // ���[�U�[�w��o�b�t�@�ւ̓]��
        lpHostdrvInfo->outBuffer = Irp->UserBuffer;
    }
    if (irpSp->FileObject) {
        lpHostdrvInfo->sectionObjectPointer = irpSp->FileObject->SectionObjectPointer;
    } else {
        lpHostdrvInfo->sectionObjectPointer = NULL;
    }
    lpHostdrvInfo->version = HOSTDRVNT_VERSION;
    lpHostdrvInfo->pendingListCount = PENDING_IRP_MAX;
    lpHostdrvInfo->pendingIrpList = g_pendingIrpList;
    lpHostdrvInfo->pendingAliveList = g_pendingAliveList;
    lpHostdrvInfo->pending.pendingIndex = -1;
    lpHostdrvInfo->hostdrvNTOptions = g_hostdrvNTOptions;
    
    if(irpSp->MajorFunction == IRP_MJ_SET_INFORMATION) {
	    MiniSOP_HandlePreMjSetInformationCache(irpSp);
 	}
 	
    // �r���̈�J�n
    ExAcquireFastMutex(&g_Mutex);
    
    // �\���̃A�h���X����������ŃG�~�����[�^�ŏ���������i�n�C�p�[�o�C�U�[�R�[���j
    // �G�~�����[�^���ŃX�e�[�^�X��o�b�t�@�Ȃǂ̒l���Z�b�g�����
    hostdrvInfoAddr = (ULONG)lpHostdrvInfo;
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr));
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr >> 8));
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr >> 16));
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr >> 24));
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'H');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'D');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'R');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'9');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'8');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'0');
    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'1');
    if(Irp->IoStatus.Status == STATUS_PENDING){
    	// �ҋ@��]
    	if(lpHostdrvInfo->pending.pendingIndex < 0 || PENDING_IRP_MAX <= lpHostdrvInfo->pending.pendingIndex){
    		// �o�^�ł���ꏊ���Ȃ�
		    Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		    Irp->IoStatus.Information = 0;
    	}else{
    		// �o�^OK�@�����ł�g_pendingIrpList�ɑ�����Ȃ��i�����Ƒ��X���b�h�Ɋ�������Ă��܂����ꂪ����j
    		pending = TRUE;
    	}
    }else if(lpHostdrvInfo->pending.pendingCompleteCount > 0){
    	// �ҋ@���������݂���
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
			// �ҋ@�����Ȃ���Γ������K�v�Ȃ�
			HostdrvStopTimer();
		}
    }
    
    // �r���̈�I��
    ExReleaseFastMutex(&g_Mutex);
    
    
#ifdef USE_FAST_IRPSTACKLOCK
	// ���b�N����
    UnlockIrpStack(&irpLockInfo);
#else
    // NonPaged�R�s�[�������߂�
    ReleaseNonPagedPoolIrpStack(irpSpNPP, irpSpNPPBefore);
	irpSp = irpSpNPPBefore; // �|�C���^�����߂�
    irpSpNPP = NULL;
#endif
    
    // �t�@�C���I�[�v���E�N���[�Y�ȂǂŊ��蓖�Ă��������̏���
    if(Irp->IoStatus.Status == STATUS_SUCCESS){
        // �t�@�C��������̂Ŕj��
        if(irpSp->MajorFunction == IRP_MJ_CLOSE) {
            if(irpSp->FileObject->SectionObjectPointer){
            	PSECTION_OBJECT_POINTERS sop;
            		
			    // SOP���
			    ExAcquireFastMutex(&g_Mutex);
    			sop = irpSp->FileObject->SectionObjectPointer;
                irpSp->FileObject->SectionObjectPointer = NULL;
                MiniSOP_ReleaseSOP(sop);
			    ExReleaseFastMutex(&g_Mutex);
            }
            if(irpSp->FileObject->FsContext){
                ExFreePool(irpSp->FileObject->FsContext);
                irpSp->FileObject->FsContext = NULL;
            }
        }else if(irpSp->MajorFunction == IRP_MJ_CREATE) {
			MiniSOP_HandleMjCreateCache(irpSp);
        }else if(irpSp->MajorFunction == IRP_MJ_CLEANUP) {
		    MiniSOP_HandleMjCleanupCache(irpSp);
        }
    }else{
        // ��肭�s���Ȃ�������j��
        if(irpSp->MajorFunction == IRP_MJ_CREATE) {
            if(sopIndex != -1){
			    // SOP���
			    ExAcquireFastMutex(&g_Mutex);
                irpSp->FileObject->SectionObjectPointer = NULL;
                MiniSOP_ReleaseSOPByIndex(sopIndex);
			    ExReleaseFastMutex(&g_Mutex);
            }
            if(irpSp->FileObject->FsContext){
                ExFreePool(irpSp->FileObject->FsContext);
                irpSp->FileObject->FsContext = NULL;
            }
        }
    }
    
    // �ҋ@���������݂���ꍇ�A����������������
    if(completeIrpList){
	    for (i = 0; i < completeIrpCount; i++) {
    		KIRQL oldIrql;
        	PIRP Irp = completeIrpList[i];
        	IoAcquireCancelSpinLock(&oldIrql);
        	if(Irp->CancelRoutine){
    			Irp->CancelRoutine = NULL;
    			IoReleaseCancelSpinLock(Irp->CancelIrql);
	        	IoCompleteRequest(Irp, IO_NO_INCREMENT); // �S���L���ŃZ�b�g����Ă���̂Ŋ������ĂԂ����ł悢
        	}else{
    			IoReleaseCancelSpinLock(Irp->CancelIrql); // ���̂��L�����Z���ς݁@�ʏ�͂Ȃ��͂�
        	}
	    }
    	ExFreePool(completeIrpList);
    	completeIrpList = NULL;
    }
    
    if(pending){
    	KIRQL oldIrql;
    	
    	// �ҋ@���ɃZ�b�g
        IoMarkIrpPending(Irp);
        
        IoAcquireCancelSpinLock(&oldIrql);  // �ی�J�n
        
	    // ���ɃL�����Z���ς݂Ȃ炷���ɏ���
		if (Irp->Cancel) {
    		IoReleaseCancelSpinLock(oldIrql);  // �ی����
	        
		    // �r���̈�J�n
		    ExAcquireFastMutex(&g_Mutex);
    
    		// ���X�g�ɓ����K�v�Ȃ��B��������
    		g_pendingIrpList[lpHostdrvInfo->pending.pendingIndex] = NULL;
    		g_pendingAliveList[lpHostdrvInfo->pending.pendingIndex] = 0;

		    // �r���̈�I��
		    ExReleaseFastMutex(&g_Mutex);
    
	        Irp->IoStatus.Status = status = STATUS_CANCELLED;
	        Irp->IoStatus.Information = 0;
	        IoCompleteRequest(Irp, IO_NO_INCREMENT);
	    }else{
		    // �ҋ@�L�����Z���v���o�^
		    //IoSetCancelRoutine(Irp, HostdrvCancelRoutine); // ��OS�œ����Ȃ����{���͂����炪����
    		Irp->CancelRoutine = HostdrvCancelRoutine;
		    
    		IoReleaseCancelSpinLock(oldIrql);  // �ی����
    		
		    // �r���̈�J�n
		    ExAcquireFastMutex(&g_Mutex);
		    
    		// ���ۂɓo�^
    		g_pendingIrpList[lpHostdrvInfo->pending.pendingIndex] = Irp;
			g_pendingCounter++;
			// �K�v�Ȃ�Ď��^�C�}�[�𓮂���
		    if(g_hostdrvNTOptions & HOSTDRVNTOPTIONS_USECHECKNOTIFY){
		    	HostdrvStartTimer();
			}
    
		    // �r���̈�I��
		    ExReleaseFastMutex(&g_Mutex);
		    
    		status = Irp->IoStatus.Status;
	    }
    }else{
    	status = Irp->IoStatus.Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    return status;
}

VOID HostdrvTimerDpcRoutine(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    if (!g_checkNotifyTimerEnabled) {
        return;
    }
    
	if(g_pendingCounter > 0 && g_checkNotifyTimerEnabled) {
    	// WorkItem�֓����� IRQL�𗎂Ƃ�
    	ExQueueWorkItem(&g_RescheduleTimerWorkItem, DelayedWorkQueue);
	}else{
		// ��~
		g_checkNotifyTimerEnabled = 0;
	}
}
VOID HostdrvRescheduleTimer(IN PVOID Context)
{
	PHOSTDRV_NOTIFYINFO lpHostdrvInfo;
	ULONG hostdrvInfoAddr;
	ULONG completeIrpCount = 0; // I/O�ҋ@�ō��񊮗��������̂̐�
	PIRP *completeIrpList = NULL; // I/O�ҋ@�ō��񊮗�����IRP�̃��X�g
	int i;
   
    lpHostdrvInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(HOSTDRV_NOTIFYINFO), "HSIM");
    
	// �r���̈�J�n
    ExAcquireFastMutex(&g_Mutex);
    
    if(lpHostdrvInfo){
    	// �G�~�����[�^���ɓn���f�[�^�ݒ�
	    lpHostdrvInfo->version = HOSTDRVNT_VERSION;
	    lpHostdrvInfo->pendingListCount = PENDING_IRP_MAX;
	    lpHostdrvInfo->pendingIrpList = g_pendingIrpList;
	    lpHostdrvInfo->pendingAliveList = g_pendingAliveList;
	    lpHostdrvInfo->pending.pendingCompleteCount = -1;
	    
	    // �\���̃A�h���X����������ŃG�~�����[�^�ŏ���������i�n�C�p�[�o�C�U�[�R�[���j
	    // �G�~�����[�^���ŃX�e�[�^�X��o�b�t�@�Ȃǂ̒l���Z�b�g�����
	    hostdrvInfoAddr = (ULONG)lpHostdrvInfo;
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr));
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr >> 8));
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr >> 16));
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_ADDR, (UCHAR)(hostdrvInfoAddr >> 24));
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'H');
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'D');
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'R');
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'9');
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'8');
	    WRITE_PORT_UCHAR((PUCHAR)HOSTDRVNT_IO_CMD, (UCHAR)'M');
	    if(lpHostdrvInfo->pending.pendingCompleteCount > 0){
	    	// �ҋ@���������݂���
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

    // �^�C�}�[�Đݒ�
	if(g_pendingCounter > 0 && g_checkNotifyTimerEnabled) {
    	// �Đݒ�
		LARGE_INTEGER dueTime = {0};
		dueTime.QuadPart = (LONGLONG)(-(LONG)g_checkNotifyInterval * 1000 * 10000);
		KeSetTimer(&g_checkNotifyTimer, dueTime, &g_checkNotifyTimerDpc);
	}else{
		// ��~
		g_checkNotifyTimerEnabled = 0;
	}
    
    // �r���̈�I��
    ExReleaseFastMutex(&g_Mutex);
    
    if(lpHostdrvInfo){
    	ExFreePool(lpHostdrvInfo);
    }
    
    // �ҋ@���������݂���ꍇ�A����������������
    if(completeIrpList){
	    for (i = 0; i < completeIrpCount; i++) {
    		KIRQL oldIrql;
        	PIRP Irp = completeIrpList[i];
        	IoAcquireCancelSpinLock(&oldIrql);
        	if(Irp->CancelRoutine){
    			Irp->CancelRoutine = NULL;
    			IoReleaseCancelSpinLock(Irp->CancelIrql);
	        	IoCompleteRequest(Irp, IO_NO_INCREMENT); // �S���L���ŃZ�b�g����Ă���̂Ŋ������ĂԂ����ł悢
        	}else{
    			IoReleaseCancelSpinLock(Irp->CancelIrql); // ���̂��L�����Z���ς݁@�ʏ�͂Ȃ��͂�
        	}
	    }
    	ExFreePool(completeIrpList);
    	completeIrpList = NULL;
    }
}
