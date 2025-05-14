#include <windows.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    const char *filePath = (argc > 1) ? argv[1] : "z:\\mmap.txt";
    const DWORD fileSize = 1024;  // �ŏ�1KB�m��
    HANDLE hFile;
    HANDLE hMapping;
    LPVOID pMap;
    char *newData;

    // �t�@�C�����J���܂��͍쐬
    hFile = CreateFileA(
        filePath,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("�t�@�C�����J���܂���ł����B�G���[: %lu\n", GetLastError());
        return 1;
    }

	getchar();
	
    // �������}�b�s���O�I�u�W�F�N�g�̍쐬
    hMapping = CreateFileMappingA(
        hFile,
        NULL,
        PAGE_READWRITE,
        0,
        fileSize,
        NULL
    );

    if (hMapping == NULL) {
        printf("CreateFileMapping ���s�B�G���[: %lu\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    // �������Ƀ}�b�v
    pMap = MapViewOfFile(
        hMapping,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        0
    );

    if (pMap == NULL) {
        printf("MapViewOfFile ���s�B�G���[: %lu\n", GetLastError());
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return 1;
    }

    // �f�[�^�\���E�ύX�i��: �擪�Ƀ��b�Z�[�W��}���j
    printf("���݂̓��e�i�擪100�o�C�g�j:\n");
    fwrite(pMap, 1, 100, stdout);
    printf("\n\n�f�[�^������������...\n");

    newData = "Hello from memory-mapped file!\n";
    memcpy(pMap, newData, strlen(newData));

    // ��n��
    UnmapViewOfFile(pMap);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    printf("�������܂����B\n");
    return 0;
}