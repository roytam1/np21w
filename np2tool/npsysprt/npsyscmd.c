#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "devdef.h"
#include "npsysprt.h"

#define BUFFER_LENGTH	256

int main(int argc, char const *argv[]) 
{
	int i;
    DWORD returned;
    char *data = NULL;
    DWORD dataLength = 3 * sizeof(ULONG); // �f�[�^�T�C�Y�i�[�̈敪�͐�ɓ���Ă���
    DWORD commandLen = 0;
    DWORD paramLen = 0;
    char *dataReadBuffer = NULL;
    HANDLE hDevice;

	// �����̏���
	if(argc <= 1)
	{
        printf("Neko Project II �V�X�e���|�[�g �A�N�Z�X�c�[��\n");
        printf("usage: %s <�R�}���h> [�o�͒lbyte1] [byte2] [byte3] [byte4] \n", argv[0]);
        return 1;
	}
	commandLen = strlen(argv[1]) + 1;
	dataLength += commandLen;
	paramLen = argc - 2;
	if(paramLen > 4)
	{
        printf("Error: �o�͒l��4byte�܂łł��B\n");
		return 1;
	}
	dataLength += paramLen;
	dataLength += BUFFER_LENGTH; // �o�̓o�b�t�@�T�C�Y�͓K���ȌŒ蒷��

	// �o�b�t�@���m��
	data = (char*)malloc(dataLength);
	memset(data, 0, dataLength);
	if(!data)
	{
        printf("Error: �o�b�t�@���������m�ۂł��܂���B\n");
		return 1;
	}
	
	// �p�����[�^�Z�b�g
	*(ULONG*)data = commandLen;
	memcpy(data + 4, argv[1], commandLen);
	*(ULONG*)(data + 4 + commandLen) = paramLen;
	for(i=0;i<paramLen;i++)
	{
		data[4 + commandLen + 4 + i] = atoi(argv[2 + i]);
	}
	*(ULONG*)(data + 4 + commandLen + 4 + paramLen) = BUFFER_LENGTH;
	dataReadBuffer = data + 4 + commandLen + 4 + paramLen + 4;

	// �f�o�C�X�I�[�v��
    hDevice = CreateFile("\\\\.\\NP2SystemPort", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) 
    {
        printf("Error: �h���C�o�ɐڑ��ł��܂���B\n");
        free(data);
        return 1;
    }

	// �R�}���h���M
    if(DeviceIoControl(hDevice, IOCTL_NP2_GENERIC, data, dataLength, data, dataLength, &returned, NULL))
    {
        printf("%s\n", dataReadBuffer);
    }
    else
    {
        printf("Error: �|�[�g�A�N�Z�X�Ɏ��s���܂����B\n");
    	CloseHandle(hDevice);
        free(data);
        return 1;
    }
    CloseHandle(hDevice);
    free(data);
    return 0;
}