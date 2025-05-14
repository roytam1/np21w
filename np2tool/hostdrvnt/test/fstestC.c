#include <windows.h>
#include <stdio.h>

void PrintFileSize(DWORD high, DWORD low) {
    ULONGLONG size = ((ULONGLONG)high << 32) | low;
    printf("�T�C�Y: %llu �o�C�g\n", size);
}

int main(int argc, char *argv[]) {
    const char *searchPath = (argc > 1) ? argv[1] : "Z:\\*.txt";
    DWORD err;

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("�t�@�C���񋓂Ɏ��s���܂����B�G���[�R�[�h: %lu\n", GetLastError());
        return 1;
    }

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            printf("[�f�B���N�g��] %s\n", findData.cFileName);
        } else {
            printf("[�t�@�C��] %s\n", findData.cFileName);
            PrintFileSize(findData.nFileSizeHigh, findData.nFileSizeLow);
        }
    } while (FindNextFileA(hFind, &findData));

    err = GetLastError();
    if (err != ERROR_NO_MORE_FILES) {
        printf("FindNextFile �ŃG���[���������܂���: %lu\n", err);
    }

    FindClose(hFind);
    return 0;
}