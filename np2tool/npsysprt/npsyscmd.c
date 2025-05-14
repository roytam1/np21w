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
    DWORD dataLength = 3 * sizeof(ULONG); // データサイズ格納領域分は先に入れておく
    DWORD commandLen = 0;
    DWORD paramLen = 0;
    char *dataReadBuffer = NULL;
    HANDLE hDevice;

	// 引数の処理
	if(argc <= 1)
	{
        printf("Neko Project II システムポート アクセスツール\n");
        printf("usage: %s <コマンド> [出力値byte1] [byte2] [byte3] [byte4] \n", argv[0]);
        return 1;
	}
	commandLen = strlen(argv[1]) + 1;
	dataLength += commandLen;
	paramLen = argc - 2;
	if(paramLen > 4)
	{
        printf("Error: 出力値は4byteまでです。\n");
		return 1;
	}
	dataLength += paramLen;
	dataLength += BUFFER_LENGTH; // 出力バッファサイズは適当な固定長で

	// バッファを確保
	data = (char*)malloc(dataLength);
	memset(data, 0, dataLength);
	if(!data)
	{
        printf("Error: バッファメモリを確保できません。\n");
		return 1;
	}
	
	// パラメータセット
	*(ULONG*)data = commandLen;
	memcpy(data + 4, argv[1], commandLen);
	*(ULONG*)(data + 4 + commandLen) = paramLen;
	for(i=0;i<paramLen;i++)
	{
		data[4 + commandLen + 4 + i] = atoi(argv[2 + i]);
	}
	*(ULONG*)(data + 4 + commandLen + 4 + paramLen) = BUFFER_LENGTH;
	dataReadBuffer = data + 4 + commandLen + 4 + paramLen + 4;

	// デバイスオープン
    hDevice = CreateFile("\\\\.\\NP2SystemPort", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) 
    {
        printf("Error: ドライバに接続できません。\n");
        free(data);
        return 1;
    }

	// コマンド送信
    if(DeviceIoControl(hDevice, IOCTL_NP2_GENERIC, data, dataLength, data, dataLength, &returned, NULL))
    {
        printf("%s\n", dataReadBuffer);
    }
    else
    {
        printf("Error: ポートアクセスに失敗しました。\n");
    	CloseHandle(hDevice);
        free(data);
        return 1;
    }
    CloseHandle(hDevice);
    free(data);
    return 0;
}