#include <ntddk.h>

// ���̃f�o�C�X�h���C�o�̃f�o�C�X��
#define DEVICE_NAME     L"\\Device\\HOSTDRV"
#define DOS_DEVICE_NAME L"\\DosDevices\\HOSTDRV"

// �G�~�����[�^�Ƃ̒ʐM�p
typedef struct tagHOSTDRV_INFO {
    PIO_STACK_LOCATION stack; // IoGetCurrentIrpStackLocation(Irp)�Ŏ擾�����f�[�^�ւ̃A�h���X
    PIO_STATUS_BLOCK status; // Irp->IoStatus�ւ̃A�h���X
    PVOID systemBuffer; // �Q�X�gOS���G�~�����[�^�ւ̃o�b�t�@
    ULONG deviceFlags; // irpSp->DeviceObject->Flags�̒l
    PVOID outBuffer; // �G�~�����[�^���Q�X�gOS�ւ̃o�b�t�@
    PVOID sectionObjectPointer; // irpSp->FileObject->SectionObjectPointer�ւ̃A�h���X
    ULONG version; // �G�~�����[�^�ʐM�o�[�W�������
} HOSTDRV_INFO, *PHOSTDRV_INFO;

// irpSp->FileObject->FsContext�֊i�[������
// �Q�l���FirpSp->FileObject->FsContext�֓K����ID������̂�NG�B�F�X�����Ȃ��Ȃ�B
// �K��ExAllocatePoolWithTag(NonPagedPool, �`�Ŋ��蓖�Ă��������ł���K�v������B
typedef struct tagHOSTDRV_FSCONTEXT {
    ULONG fileIndex; // �G�~�����[�^�{�̂��Ǘ�����t�@�C��ID
    ULONG reserved1; // �\��
    ULONG reserved2; // �\��
    ULONG reserved3; // �\��
} HOSTDRV_FSCONTEXT, *PHOSTDRV_FSCONTEXT;

// ����I/O�̏����ۂ�Ԃ��֐��B�g��Ȃ��̂ŏ펞FALSE��Ԃ��B
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

// �O���[�o���ɊǗ�����ϐ��Q
static FAST_MUTEX g_Mutex; // I/O�̔r�����b�N�p

// �֐���`
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject);

// ���W�X�g���L�[���`�F�b�N���ă����[�o�u���f�o�C�X�����ɂ��邩���擾
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

// �f�o�C�X�h���C�o�̃G���g���|�C���g
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
    UNICODE_STRING deviceNameUnicodeString, dosDeviceNameUnicodeString;
    PDEVICE_OBJECT deviceObject = NULL;
    BOOLEAN isRemovableDevice = FALSE;
    DEVICE_TYPE deviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
    NTSTATUS status;
    int i;
    
    // hostdrv for NT�Ή������ȈՃ`�F�b�N
    if(READ_PORT_UCHAR((PUCHAR)0x7EC) != 98 || READ_PORT_UCHAR((PUCHAR)0x7EE) != 21){
        return STATUS_NO_SUCH_DEVICE;
	}

    // �r�����b�N�������@�������݂̂Ŕj�������͂���Ȃ�
    ExInitializeFastMutex(&g_Mutex);
    
    // �����[�o�u���f�o�C�X�̐U������郂�[�h
    isRemovableDevice = HostdrvIsRemovableDevice(RegistryPath);
    if(isRemovableDevice){
    	deviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
    }
    
    // �f�o�C�X���쐬�@���[�J���f�B�X�N�̐U�������Ȃ�FILE_DEVICE_DISK_FILE_SYSTEM
    RtlInitUnicodeString(&deviceNameUnicodeString, DEVICE_NAME);
    status = IoCreateDevice(DriverObject, 0, &deviceNameUnicodeString,
                            deviceType, 0, FALSE, &deviceObject);
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

    // �f�o�C�X��IRP�����֐���o�^�@�v�f�ԍ����eIRP_MJ_�`�̔ԍ��ɑΉ��B
    // �S���G�~�����[�^�{�̂ɓ�����̂őS���ɓ����֐������蓖��
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = HostdrvDispatch;
    }

    // �h���C�o�̏I��������o�^
    DriverObject->DriverUnload = HostdrvUnload;
    
    if(isRemovableDevice){
	    // ���ݔ����g�킹�Ȃ����߂Ƀ����[�o�u���f�B�X�N�̐U�������
	    deviceObject->Characteristics |= FILE_REMOVABLE_MEDIA;
    }else{
	    // ���ݔ����g�킹�Ȃ����߂Ƀl�b�g���[�N�f�o�C�X�̐U�������
	    deviceObject->Characteristics |= FILE_REMOTE_DEVICE;
    }
    
    // ���̑��ǉ��t���O��ݒ�
    deviceObject->Flags |= DO_BUFFERED_IO; // �f�[�^�󂯓n����SystemBuffer����{�Ƃ���H�w�肵�Ă��������ŗ���Ƃ��͗���C������B
    deviceObject->Flags |= DO_LOW_PRIORITY_FILESYSTEM; // ��D��x�ŏ���

    KdPrint(("Hostdrv: Loaded successfully\n"));
    return STATUS_SUCCESS;
}

VOID HostdrvUnload(IN PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING dosDeviceNameUnicodeString;
    
    // DOS����o�^����
    RtlInitUnicodeString(&dosDeviceNameUnicodeString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&dosDeviceNameUnicodeString);
    
    // �f�o�C�X���폜
    IoDeleteDevice(DriverObject->DeviceObject);
    
    KdPrint(("Hostdrv: Unloaded\n"));
}

NTSTATUS HostdrvDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp) {
    HOSTDRV_INFO hostdrvInfo;
    HOSTDRV_INFO *lpHostdrvInfo;
    ULONG hostdrvInfoAddr;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    // IRP_MJ_CREATE�̎��A�K�v�ȃ������������蓖��
    if(irpSp->MajorFunction == IRP_MJ_CREATE) {
        // FileObject��NULL�Ȃُ͈̂�Ȃ̂Œe��
        if(irpSp->FileObject == NULL){
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Irp->IoStatus.Status;
        }
        // FsContext��SectionObjectPointer��ExAllocatePoolWithTag(NonPagedPool,�`�Ń��������蓖��
        // NULL�̂܂܂�������Ⴄ���@�Ŋ��蓖�Ă�Ɛ���ɓ����Ȃ��̂Œ���
        irpSp->FileObject->FsContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(HOSTDRV_FSCONTEXT), "HSFC");
        RtlZeroMemory(irpSp->FileObject->FsContext, sizeof(HOSTDRV_FSCONTEXT));
        irpSp->FileObject->SectionObjectPointer = ExAllocatePoolWithTag(NonPagedPool, sizeof(SECTION_OBJECT_POINTERS), "HSOP");
        RtlZeroMemory(irpSp->FileObject->SectionObjectPointer, sizeof(SECTION_OBJECT_POINTERS));
    }
    
    // �f�t�H���g�̃X�e�[�^�X�ݒ�
    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    Irp->IoStatus.Information = 0;
    
    // �G�~�����[�^���ɓn���f�[�^�ݒ�
    lpHostdrvInfo = &hostdrvInfo;
    lpHostdrvInfo->stack = irpSp;
    lpHostdrvInfo->status = &(Irp->IoStatus);
    lpHostdrvInfo->systemBuffer = Irp->AssociatedIrp.SystemBuffer;
    lpHostdrvInfo->deviceFlags = irpSp->DeviceObject->Flags;
    if (Irp->AssociatedIrp.SystemBuffer != NULL) {
        // �V�X�e���o�b�t�@�o�R�ł̊ԐړI�ȓ]��
        lpHostdrvInfo->outBuffer = Irp->AssociatedIrp.SystemBuffer;
    } else if (Irp->MdlAddress != NULL) {
        // MdlAddress���g�������ړI�ȓ]��
        lpHostdrvInfo->outBuffer = MmGetSystemAddressForMdl(Irp->MdlAddress); // �Â�OS�Ή��p
        //lpHostdrvInfo->outBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority); // �ŋ߂�OS�Ȃ炱����\�i�����S�H�j
    } else {
        // ���[�U�[�w��o�b�t�@�ւ̓]��
        lpHostdrvInfo->outBuffer = Irp->UserBuffer;
    }
    if (irpSp->FileObject) {
        lpHostdrvInfo->sectionObjectPointer = irpSp->FileObject->SectionObjectPointer;
    } else {
        lpHostdrvInfo->sectionObjectPointer = NULL;
    }
    lpHostdrvInfo->version = 1;
    
    // �\���̃A�h���X����������ŃG�~�����[�^�ŏ���������i�n�C�p�[�o�C�U�[�R�[���j
    // �G�~�����[�^���ŃX�e�[�^�X��o�b�t�@�Ȃǂ̒l���Z�b�g�����
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
    
    // �t�@�C���I�[�v���E�N���[�Y�ȂǂŊ��蓖�Ă��������̏���
    if(Irp->IoStatus.Status == STATUS_SUCCESS){
        // �t�@�C��������̂Ŕj��
        if(irpSp->MajorFunction == IRP_MJ_CLOSE) {
            if(irpSp->FileObject->SectionObjectPointer){
                ExFreePool(irpSp->FileObject->SectionObjectPointer);
                irpSp->FileObject->SectionObjectPointer = NULL;
                ExFreePool(irpSp->FileObject->FsContext);
                irpSp->FileObject->FsContext = NULL;
            }
        }
    }else{
        // ��肭�s���Ȃ�������j��
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
