#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "devdef.h"
#include "npsysprt.h"

int main(int argc, char const *argv[]) 
{
	int i;
    DWORD returned;
    DWORD cmdLength;
    IOPORT_NP2_SIMPLE_DATA data = {0};
    HANDLE hDevice;

	// �����̏���
	if(argc <= 1)
	{
        printf("Neko Project II �V�X�e���|�[�g �A�N�Z�X�c�[��\n");
        printf("usage: %s <�R�}���h> [�p�����[�^�lbyte1] [byte2] [byte3] [byte4] \n", argv[0]);
        return 1;
	}
	cmdLength = strlen(argv[1]);
	if(cmdLength > 16)
	{
        printf("Error: �R�}���h��16�����܂łł��B\n");
		return 1;
	}
	data.paramLength = argc - 2;
	if(data.paramLength > 4)
	{
        printf("Error: �p�����[�^�l��4byte�܂łł��B\n");
		return 1;
	}

	// �p�����[�^�Z�b�g
	memcpy(data.command, argv[1], cmdLength);
	for(i=0;i<data.paramLength;i++)
	{
		data.param.b[i] = atoi(argv[2 + i]);
	}

	// �f�o�C�X�I�[�v��
    hDevice = CreateFile("\\\\.\\NP2SystemPort", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) 
    {
        printf("Error: �h���C�o�ɐڑ��ł��܂���B\n");
        return 1;
    }

	// �R�}���h���M
    if(DeviceIoControl(hDevice, IOCTL_NP2_SIMPLE, &data, sizeof(IOPORT_NP2_SIMPLE_DATA), &data, sizeof(IOPORT_NP2_SIMPLE_DATA), &returned, NULL))
    {
    	char readData[17] = {0}; // NULL������t���邽�߂̉��o�b�t�@
		memcpy(readData, data.readBuffer, 16);
        printf("%s\n", readData);
    }
    else
    {
        printf("Error: �|�[�g�A�N�Z�X�Ɏ��s���܂����B\n");
    	CloseHandle(hDevice);
        return 1;
    }
    CloseHandle(hDevice);
    
    return 0;
}