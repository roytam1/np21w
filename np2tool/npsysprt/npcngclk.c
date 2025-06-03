#include <windows.h>
#include <stdio.h>
#include "devdef.h"
#include "npsysprt.h"

int main(int argc, char const *argv[]) 
{
    IOPORT_NP2_CLOCK_DATA data;
    HANDLE hDevice;
    DWORD returned;
    int clockMul = 0;
    
    printf("Neko Project II ���ICPU�N���b�N�ύX�c�[��\n");
	if(argc > 1)
	{
		clockMul = atoi(argv[1]);
		if(clockMul <= 0 || 255 < clockMul)
		{
	    	printf("�N���b�N�{�� %d �͎w��\�͈͊O�ł��B\n", clockMul);
	        return 1;
		}
	}
	else
	{
        printf("usage: %s <CPU�N���b�N�{��(1�`255)>\n", argv[0]);
	}

    hDevice = CreateFile("\\\\.\\NP2SystemPort", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) 
    {
        printf("�h���C�o�ɐڑ��ł��܂���B\n");
        return 1;
    }

	if(argc > 1)
	{
	    DeviceIoControl(hDevice, IOCTL_NP2_CLOCK_READ, &data, sizeof(data), &data, sizeof(data), &returned, NULL);
	    printf("���̃N���b�N�{�� = %d\n", data.clockMul);

		data.clockMul = clockMul;
	    DeviceIoControl(hDevice, IOCTL_NP2_CLOCK_WRITE, &data, sizeof(data), NULL, 0, &returned, NULL);
	    printf("�N���b�N�{����%d�ɐݒ肵�܂����B\n", data.clockMul);

	    DeviceIoControl(hDevice, IOCTL_NP2_CLOCK_READ, &data, sizeof(data), &data, sizeof(data), &returned, NULL);
	    printf("���݂̃N���b�N�{�� = %d\n", data.clockMul);
	}
	else
	{
	    DeviceIoControl(hDevice, IOCTL_NP2_CLOCK_READ, &data, sizeof(data), &data, sizeof(data), &returned, NULL);
	    printf("���݂̃N���b�N�{�� = %d\n", data.clockMul);
	}

    CloseHandle(hDevice);
    
    return 0;
}