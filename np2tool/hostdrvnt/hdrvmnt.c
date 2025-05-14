#include <windows.h>
#include <stdio.h>

#define HOSTDRV_PATH	"\\Device\\HOSTDRV"

int main(int argc, char const* argv[]) {
	int i;
	int remove = 0;
	int hasarg = 0;
	int isAllocated = 0;
	char driveLetter[] = " :";
	char driveLetterTmp[] = " :";
	char target[MAX_PATH] = {0};
	DWORD result;

    printf("Neko Project II �z�X�g���L�h���C�u for Windows NT\n");
	for(i=1;i<argc;i++){
		int slen = strlen(argv[i]);
		if(slen==1 || slen==2 && argv[i][1]==':'){
			char drive = argv[i][0];
			if('a' <= drive && drive <= 'z' || 'A' <= drive && drive <= 'Z'){
				hasarg = 1;
				driveLetter[0] = drive;
			}else{
				printf("�h���C�u�����w�肪�ُ�ł��B\n");
        		return 1;
			}
		}else if(stricmp(argv[i], "-r")==0 || stricmp(argv[i], "/r")==0){
			remove = 1;
			hasarg = 1;
		}else if(stricmp(argv[i], "-h")==0 || stricmp(argv[i], "/h")==0 || stricmp(argv[i], "-?")==0 || stricmp(argv[i], "/?")==0){
	        printf("usage: %s <�h���C�u����> [/r] \n", argv[0]);
	        printf("/r�������ꍇ�A���蓖�ĉ������܂��B\n");
	        return 0;
		}
	}
	
	// ���������f�o�C�X���g���邩�m�F
    if (GetFileAttributes("\\\\.\\HOSTDRV") == 4294967295) 
    {
    	DWORD err = GetLastError();
        printf("Error: �h���C�o�ɐڑ��ł��܂���B(code: %d)\n", err);
        return 1;
    }
	
	// ���蓖�čς݃`�F�b�N
	for(i='A';i<='Z';i++){
		driveLetterTmp[0] = i;
	    if (QueryDosDevice(driveLetterTmp, target, MAX_PATH)) {
	    	if(stricmp(target,HOSTDRV_PATH)==0){
	    		isAllocated = 1;
	    		break;
	    	}
	    }
	}
	
	if(hasarg && !remove){
		if(isAllocated){
		    printf("���݃h���C�u%s�Ɋ��蓖�Ă��Ă��܂��B\n", driveLetterTmp);
		    printf("���/r�I�v�V�����œo�^�������Ă��������B\n");
	        return 1;
		}
	    if (!DefineDosDeviceA(DDD_RAW_TARGET_PATH, driveLetter, HOSTDRV_PATH)) {
	        printf("���蓖�ĂɎ��s���܂����B\n");
	        return 1;
	    }
	    printf("HOSTDRV��%s�Ɋ��蓖�Ă܂����B\n", driveLetter);
	}else if(hasarg && remove){
		if(!isAllocated){
			printf("���݊��蓖�Ă��Ă��܂���B\n");
	        return 1;
		}
	    if (!DefineDosDeviceA(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION, driveLetterTmp, HOSTDRV_PATH)) {
	        printf("���蓖�ĉ����Ɏ��s���܂����B\n");
	        return 1;
	    }
	    printf("HOSTDRV��%s���犄�蓖�ĉ������܂����B\n", driveLetterTmp);
	}else if(argc < 1){
        printf("����������������܂���B\n", argv[0]);
        printf("/?�I�v�V�����Ŏg������\�����܂��B\n");
		return 1;
	}else{
		if(isAllocated){
		    printf("���݃h���C�u%s�Ɋ��蓖�Ă��Ă��܂��B\n", driveLetterTmp);
		}else{
			printf("���݊��蓖�Ă��Ă��܂���B\n");
		}
	}
    
	return 0;
}