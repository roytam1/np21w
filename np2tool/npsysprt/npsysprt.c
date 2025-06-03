#include <ntddk.h>
#include "npsysprt.h"

#define NP2_PARAM_PORT	0x7ED
#define NP2_CMD_PORT	0x7EF

#define NP2_CMD_MAXLEN	16 // np2�d�l��16byte�܂�
#define NP2_READ_MAXLEN	16 // np2�d�l��16byte�܂�

#define NP2_DEVNAME        L"\\Device\\NP2SystemPort"
#define NP2_SYMNAME        L"\\DosDevices\\NP2SystemPort"

#define NP2_COMMAND_NP2CHECK        "NP2"
#define NP2_COMMAND_MULTIPLE        "multiple"
#define NP2_COMMAND_CHANGECLOCKMUL  "changeclockmul"
#define NP2_COMMAND_GETMPOS         "getmpos"


KSPIN_LOCK g_IOSpinLock; // I/O�̔r�����b�N�p

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT, PIRP);
VOID     DriverUnload(PDRIVER_OBJECT);
NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP);

BOOLEAN SendNP2Check(void);
VOID SendNP2ChangeClock(ULONG newClockMul);
ULONG SendNP2GetClock(void);
ULONG SendNP2GetMousePos(PUSHORT x, PUSHORT y);
NTSTATUS SendNP2SystemPortData(CHAR *lpCommand, ULONG commandLen, UCHAR *lpParamBuffer, ULONG paramBufferLen, UCHAR *lpReadBuffer, ULONG *lpReadBufferLen);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING devName;
    UNICODE_STRING symLink;
    PDEVICE_OBJECT DeviceObject;
    
    // �r�����b�N�������@�������݂̂Ŕj�������͂���Ȃ�
    KeInitializeSpinLock(&g_IOSpinLock);
    
    if(!SendNP2Check())
    {
        return STATUS_NO_SUCH_DEVICE;
    }
    
    RtlInitUnicodeString(&devName, NP2_DEVNAME);
    RtlInitUnicodeString(&symLink, NP2_SYMNAME);

    UNREFERENCED_PARAMETER(RegistryPath);

    IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    IoCreateSymbolicLink(&symLink, &devName);

    DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}

NTSTATUS CreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symLink = {0};
    RtlInitUnicodeString(&symLink, NP2_SYMNAME);

    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0; // ����0
    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_NP2_GENERIC:
        {
            ULONG readPos = 0;
            ULONG bufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            UCHAR *sysBuf = Irp->AssociatedIrp.SystemBuffer;
            ULONG commandLen, paramLen, readBufferLen;
            UCHAR *command, *param, *readBuffer;
            
            // �o�b�t�@�ʒu��ǂݎ��@�ُ�Ȃ�G���[�ɂ���
            if(bufferLen < 4) {
            	status = STATUS_BUFFER_TOO_SMALL;
            	break;
            }
            commandLen = *(ULONG*)(&sysBuf[0]);
            command = &sysBuf[4];
            readPos += 4 + commandLen;
            if(bufferLen < readPos + 4) {
            	status = STATUS_BUFFER_TOO_SMALL;
            	break;
            }
            paramLen = *(ULONG*)(&sysBuf[readPos]);
            param = &sysBuf[readPos + 4];
            readPos += 4 + paramLen;
            if(bufferLen < readPos + 4) {
            	status = STATUS_BUFFER_TOO_SMALL;
            	break;
            }
            readBufferLen = *(ULONG*)(&sysBuf[readPos]);
            readBuffer = &sysBuf[readPos + 4];
            readPos += 4 + readBufferLen;
            if(bufferLen != readPos) {
            	status = STATUS_BUFFER_TOO_SMALL;
            	break;
            }
            
            // �|�[�g�A�N�Z�X���s
            status = SendNP2SystemPortData((CHAR*)command, commandLen, param, paramLen, readBuffer, &readBufferLen);
            if(status != STATUS_SUCCESS)
            {
            	break;
            }
            
            // �߂��f�[�^�̃T�C�Y
            Irp->IoStatus.Information = bufferLen;
            
            break;
        }
        case IOCTL_NP2_SIMPLE:
        {
            ULONG bufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            PIOPORT_NP2_SIMPLE_DATA data = Irp->AssociatedIrp.SystemBuffer;
            ULONG commandLen, paramLen, readBufferLen;
            UCHAR *command, *param, *readBuffer;
            
            // �o�b�t�@�ʒu��ǂݎ��@�ُ�Ȃ�G���[�ɂ���
            if(bufferLen != sizeof(IOPORT_NP2_SIMPLE_DATA)) {
            	status = STATUS_BUFFER_TOO_SMALL;
            	break;
            }
            for(commandLen=0;commandLen<NP2_CMD_MAXLEN;commandLen++){ // �R�}���h������16�����܂�
            	if(data->command[commandLen] == '\0') break;
            }
            if(commandLen == 0) {
            	status = STATUS_INVALID_DEVICE_REQUEST;
            	break;
            }
            command = data->command;
            paramLen = data->paramLength;
            if(paramLen > 4) {
            	status = STATUS_INVALID_DEVICE_REQUEST;
            	break;
            }
            param = data->param.b;
            readBufferLen = NP2_READ_MAXLEN;
            readBuffer = data->readBuffer;
            
            // �|�[�g�A�N�Z�X���s
            status = SendNP2SystemPortData((CHAR*)command, commandLen, param, paramLen, readBuffer, &readBufferLen);
            if(status != STATUS_SUCCESS)
            {
            	break;
            }
            
            // �߂��f�[�^�̃T�C�Y
            Irp->IoStatus.Information = bufferLen;
            
            break;
        }
        case IOCTL_NP2_CLOCK_WRITE:
        {
            ULONG bufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            PIOPORT_NP2_CLOCK_DATA ioData = (PIOPORT_NP2_CLOCK_DATA)Irp->AssociatedIrp.SystemBuffer;
            
            if(bufferLen != sizeof(IOPORT_NP2_CLOCK_DATA)) {
            	status = STATUS_INVALID_DEVICE_REQUEST;
            	break;
            }
            
            SendNP2ChangeClock(ioData->clockMul);
            break;
        }
        case IOCTL_NP2_CLOCK_READ:
        {
            ULONG bufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            PIOPORT_NP2_CLOCK_DATA ioData = (PIOPORT_NP2_CLOCK_DATA)Irp->AssociatedIrp.SystemBuffer;
            
            if(bufferLen != sizeof(IOPORT_NP2_CLOCK_DATA)) {
            	status = STATUS_INVALID_DEVICE_REQUEST;
            	break;
            }
            
            ioData->clockMul = SendNP2GetClock();
            Irp->IoStatus.Information = sizeof(IOPORT_NP2_CLOCK_DATA);
            break;
        }
        case IOCTL_NP2_MOUSEPOS_READ:
        {
            ULONG bufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            PIOPORT_NP2_MOUSEPOS_DATA ioData = (PIOPORT_NP2_MOUSEPOS_DATA)Irp->AssociatedIrp.SystemBuffer;
            
            if(bufferLen != sizeof(IOPORT_NP2_MOUSEPOS_DATA)) {
            	status = STATUS_INVALID_DEVICE_REQUEST;
            	break;
            }
            
            if(!SendNP2GetMousePos(&ioData->absPosX, &ioData->absPosY)){
            	status = STATUS_INVALID_DEVICE_REQUEST;
            	break;
            }
            Irp->IoStatus.Information = sizeof(IOPORT_NP2_MOUSEPOS_DATA);
            break;
        }
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

BOOLEAN SendNP2Check()
{
    int i;
    char tmp;
    char commandText[] = NP2_COMMAND_NP2CHECK;
    KIRQL oldIrql;
    
    // �r���̈�J�n
	KeAcquireSpinLock(&g_IOSpinLock, &oldIrql);
	
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if (tmp != commandText[i]) return FALSE;
    }
    
    // �r���̈�I��
	KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
	
    return TRUE;
}
VOID SendNP2ChangeClock(ULONG clockMul)
{
    int i;
    char commandText[] = NP2_COMMAND_CHANGECLOCKMUL;
    KIRQL oldIrql;
    
    // �r���̈�J�n
	KeAcquireSpinLock(&g_IOSpinLock, &oldIrql);
    
    WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, (UCHAR)clockMul);
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
    
    // �r���̈�I��
	KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
}
ULONG SendNP2GetClock()
{
    int i;
    char tmp;
    char commandText[] = NP2_COMMAND_MULTIPLE;
    ULONG clockMul = 0;
    KIRQL oldIrql;
    
    // �r���̈�J�n
	KeAcquireSpinLock(&g_IOSpinLock, &oldIrql);
    
    WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, (UCHAR)0);
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
    for(i=0;i<10;i++) // �N���b�N�{��������10���͂��蓾�Ȃ��B�ʖڂ����Ȃ�ǂ߂Ȃ��Ă�������B
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if (tmp == 0xff || tmp == 0 || (tmp - '0') >= 10) break;
        if (tmp == 0xff || tmp == 0 || (tmp - '0') >= 10) break;
        clockMul *= 10;
        clockMul += (tmp - '0') % 10;
    }
    
    // �r���̈�I��
	KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
    
    return clockMul;
}
ULONG SendNP2GetMousePos(PUSHORT x, PUSHORT y)
{
    int i;
    char tmp;
    char commandText[] = NP2_COMMAND_GETMPOS;
    KIRQL oldIrql;
    
    if(!x || !y) return 0;
    
    // �r���̈�J�n
	KeAcquireSpinLock(&g_IOSpinLock, &oldIrql);
    
    // �Â��R�}���h���s���ʂ�����Α|��
    for(i=0;i<NP2_READ_MAXLEN;i++)
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if(tmp == '\0') break;
    }
    
    // �����g���p�p�����[�^ ��U��0
    WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, 0);
    
    // �R�}���h���M
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
    
    // �ǂݎ��
    *x = 0;
    for(i=0;i<NP2_READ_MAXLEN;i++)
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if (tmp == ',') break;
        if (tmp < '0' || '9' < tmp){
        	// ���l�ł͂Ȃ��̂ňُ�
			KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
        	return 0;
        }
        *x = (*x * 10) + (tmp - '0');
    }
    *y = 0;
    for(;i<NP2_READ_MAXLEN;i++)
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if (tmp == '\0') break;
        if (tmp < '0' || '9' < tmp){
        	// ���l�ł͂Ȃ��̂ňُ�
			KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
        	return 0;
        }
        *y = (*y * 10) + (tmp - '0');
    }
    
    // �r���̈�I��
	KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
    
    if(i==NP2_READ_MAXLEN) return 0;
    
    return 1;
}
NTSTATUS SendNP2SystemPortData(CHAR *lpCommand, ULONG commandLen, UCHAR *lpParamBuffer, ULONG paramBufferLen, UCHAR *lpReadBuffer, ULONG *lpReadBufferLen)
{
    int i;
    KIRQL oldIrql;
    
    // �R�}���h����NULL�܂��͋󔒂͖���
    if(!lpCommand || lpCommand[0]=='\0')
    {
        return STATUS_INVALID_PARAMETER;
    }
    
    // ���̒l��4�o�C�g�ȏ�̃p�����[�^�͖���
    if(paramBufferLen < 0 || paramBufferLen > 4) 
    {
        return STATUS_INVALID_PARAMETER;
    }
    
    // �p�����[�^�o�b�t�@�������w�肵�Ă���̂�NULL�|�C���^�Ȃ�G���[
    if(paramBufferLen > 0 && !lpParamBuffer) return STATUS_INVALID_PARAMETER;
    
    // �ǂݎ��o�b�t�@�������w�肵�Ă���̂�NULL�|�C���^�Ȃ�G���[
    if(lpReadBufferLen && *lpReadBufferLen > 0 && !lpReadBuffer) return STATUS_INVALID_PARAMETER;
    
    // �r���̈�J�n
	KeAcquireSpinLock(&g_IOSpinLock, &oldIrql);
    
    // �Â��R�}���h���s���ʂ�����Α|��
    for(i=0;i<NP2_READ_MAXLEN;i++)
    {
        char tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if(tmp == '\0') break;
    }
    
    // �p�����[�^������Α���
    for(i=0;i<paramBufferLen;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, (UCHAR)lpParamBuffer[i]);
    }
    
    // �R�}���h�𑗂�@NULL�����܂ő��邪�A����������������R�}���h�ُ͈�Ƃ���
    if(commandLen == 0) commandLen = NP2_CMD_MAXLEN;
    for(i=0;lpCommand[i]!='\0';i++)
    {
        if(i==commandLen) break;
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)lpCommand[i]);
    }
    
    // NULL�����܂Ńf�[�^�ǂݎ��
    if(lpReadBufferLen)
    {
        char tmp;
        if(*lpReadBufferLen == 0){
            // �f�[�^�̒����̒���
            ULONG portReadLen = 0;
            for(i=0;i<NP2_READ_MAXLEN;i++)
            {
                tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
                portReadLen++;
                if(tmp == '\0') break;
            }
            *lpReadBufferLen = portReadLen;
        }
        else
        {
            // �f�[�^�ǂݎ�� �i�[�̈悪����Ȃ��ꍇ�͓ǂ߂�͈͂œǂ�
            int len = *lpReadBufferLen;
            for(i=0;i<len;i++)
            {
                tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
                lpReadBuffer[i] = tmp;
                if(tmp == '\0') break;
            }
            lpReadBuffer[len - 1] = '\0'; // �����͕K��NULL������ۏ؂���
        }
    }
    
    // �r���̈�I��
	KeReleaseSpinLock(&g_IOSpinLock, oldIrql);
    
    return STATUS_SUCCESS;
}
