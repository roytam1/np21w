#include <windows.h>
#include <stdio.h>

void PrintFileTime(FILETIME ft) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    printf("%04d/%02d/%02d %02d:%02d:%02d\n",
           stLocal.wYear, stLocal.wMonth, stLocal.wDay,
           stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
}

int main(int argc, char *argv[]) {
    const char *filePath;
    LARGE_INTEGER fileSize;
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    
    filePath = (argc > 1) ? argv[1] : "Z:\\test.txt";

    if (!GetFileAttributesExA(filePath, GetFileExInfoStandard, &fileInfo)) {
        printf("�t�@�C�����̎擾�Ɏ��s���܂����B�G���[�R�[�h: %lu\n", GetLastError());
        return 1;
    }

    fileSize.HighPart = fileInfo.nFileSizeHigh;
    fileSize.LowPart = fileInfo.nFileSizeLow;

    printf("�t�@�C��: %s\n", filePath);
    printf("�T�C�Y: %lld �o�C�g\n", fileSize.QuadPart);

    printf("�쐬����: ");
    PrintFileTime(fileInfo.ftCreationTime);

    printf("�ŏI�A�N�Z�X����: ");
    PrintFileTime(fileInfo.ftLastAccessTime);

    printf("�ŏI�������ݓ���: ");
    PrintFileTime(fileInfo.ftLastWriteTime);

    return 0;
}