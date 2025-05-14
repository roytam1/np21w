#include <ntddk.h>
#include "npsysprt.h"

#define NP2_PARAM_PORT	0x7ED
#define NP2_CMD_PORT	0x7EF

#define NP2_CMD_MAXLEN	128
#define NP2_READ_MAXLEN	16384

#define NP2_DEVNAME        L"\\Device\\NP2SystemPort"
#define NP2_SYMNAME        L"\\DosDevices\\NP2SystemPort"

#define NP2_COMMAND_NP2CHECK        "NP2"
#define NP2_COMMAND_MULTIPLE        "multiple"
#define NP2_COMMAND_CHANGECLOCKMUL    "changeclockmul"

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT, PIRP);
VOID     DriverUnload(PDRIVER_OBJECT);
NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP);

BOOLEAN SendNP2Check(void);
VOID SendNP2ChangeClock(ULONG newClockMul);
ULONG SendNP2GetClock(void);
NTSTATUS SendNP2SystemPortData(CHAR *lpCommand, ULONG commandLen, UCHAR *lpParamBuffer, ULONG paramBufferLen, UCHAR *lpReadBuffer, ULONG *lpRreadBufferLen);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING devName;
    UNICODE_STRING symLink;
    PDEVICE_OBJECT DeviceObject;
    
    if(!SendNP2Check())
    {
        return STATUS_NO_SUCH_DEVICE;
    }
    
    RtlInitUnicodeString(&devName, NP2_DEVNAME);
    RtlInitUnicodeString(&symLink, NP2_SYMNAME);

    UNREFERENCED_PARAMETER(RegistryPath);

    IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
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

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_NP2_GENERIC:
        {
            ULONG readPos = 0;
            ULONG bufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            UCHAR *sysBuf = Irp->AssociatedIrp.SystemBuffer;
            ULONG commandLen, paramLen, readBufferLen;
            UCHAR *command, *param, *readBuffer;
            
            // バッファ位置を読み取る　異常ならエラーにする
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
            
            // ポートアクセス実行
            status = SendNP2SystemPortData((CHAR*)command, commandLen, param, paramLen, readBuffer, &readBufferLen);
            if(status != STATUS_SUCCESS)
            {
            	break;
            }
            
            // 戻すデータのサイズ
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
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++) // クロック倍率文字列が10桁はあり得ない。駄目そうなら読めなくても抜ける。
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if (tmp != commandText[i]) return FALSE;
    }
    return TRUE;
}
VOID SendNP2ChangeClock(ULONG clockMul)
{
    int i;
    char commandText[] = NP2_COMMAND_CHANGECLOCKMUL;
    WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, (UCHAR)clockMul);
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
}
ULONG SendNP2GetClock()
{
    int i;
    char tmp;
    char commandText[] = NP2_COMMAND_MULTIPLE;
    ULONG clockMul = 0;
    
    WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, (UCHAR)0);
    for(i=0;i<sizeof(commandText)/sizeof(commandText[0])-1;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)commandText[i]);
    }
    for(i=0;i<10;i++) // クロック倍率文字列が10桁はあり得ない。駄目そうなら読めなくても抜ける。
    {
        tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
        if (tmp == 0xff || tmp == 0 || (tmp - '0') >= 10) break;
        clockMul *= 10;
        clockMul += (tmp - '0') % 10;
    }
    
    return clockMul;
}
NTSTATUS SendNP2SystemPortData(CHAR *lpCommand, ULONG commandLen, UCHAR *lpParamBuffer, ULONG paramBufferLen, UCHAR *lpReadBuffer, ULONG *lpRreadBufferLen)
{
    int i;
    
    // コマンド名がNULLまたは空白は無効
    if(!lpCommand || lpCommand[0]=='\0')
    {
        return STATUS_INVALID_PARAMETER;
    }
    
    // 負の値や4バイト以上のパラメータは無効
    if(paramBufferLen < 0 || paramBufferLen > 4) 
    {
        return STATUS_INVALID_PARAMETER;
    }
    
    // パラメータバッファ長さを指定しているのにNULLポインタならエラー
    if(paramBufferLen > 0 && !lpParamBuffer) return STATUS_INVALID_PARAMETER;
    
    // 読み取りバッファ長さを指定しているのにNULLポインタならエラー
    if(lpRreadBufferLen && *lpRreadBufferLen > 0 && !lpReadBuffer) return STATUS_INVALID_PARAMETER;
    
    // パラメータがあれば送る
    for(i=0;i<paramBufferLen;i++)
    {
        WRITE_PORT_UCHAR((PUCHAR)NP2_PARAM_PORT, (UCHAR)lpParamBuffer[i]);
    }
    
    // コマンドを送る　NULL文字まで送るが、文字数が多すぎるコマンドは異常とする
    if(commandLen == 0) commandLen = NP2_CMD_MAXLEN;
    for(i=0;lpCommand[i]!='\0';i++)
    {
        if(i==commandLen) break;
        WRITE_PORT_UCHAR((PUCHAR)NP2_CMD_PORT, (UCHAR)lpCommand[i]);
    }
    
    // NULL文字までデータ読み取り
    if(lpRreadBufferLen)
    {
        char tmp;
        if(*lpRreadBufferLen == 0){
            // データの長さの調査
            ULONG portReadLen = 0;
            for(i=0;i<NP2_READ_MAXLEN;i++)
            {
                tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
                portReadLen++;
                if(tmp == '\0') break;
            }
            *lpRreadBufferLen = portReadLen;
        }
        else
        {
            // データ読み取り 格納領域が足りない場合は読める範囲で読む
            int len = *lpRreadBufferLen;
            for(i=0;i<len;i++)
            {
                tmp = READ_PORT_UCHAR((PUCHAR)NP2_CMD_PORT);
                lpReadBuffer[i] = tmp;
                if(tmp == '\0') break;
            }
            lpReadBuffer[len - 1] = '\0'; // 末尾は必ずNULL文字を保証する
        }
    }
    
    return STATUS_SUCCESS;
}
