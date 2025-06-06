/*
 *	minisop.c
 *	�t�@�C���P�ʂ�FileObject->SectionObjectPointer(SOP)���ȈՎ������܂��B
 *	�^����ꂽ�p�X����v���Ă�����̂𓯂��t�@�C���ƌ��􂵂āA��v���Ă���ꍇ�͓���SOP��Ԃ��܂��B
 *
 *	���t�@�C���P�ʂŋ��ʂɂ���v���͔������Ă������Memory-Mapped File�iEXE�̎��s���ɂ��g�p�j�ŉe�����܂��B
 *	����ȊO�ł�IRP_MJ_CREATE�P�ʂ�SOP�������Ă����Ȃ������܂��B
 *	
 *	HACK: ���܂��@�\�@minisop.h��#define USE_CACHEHACK����ƁA
 *	�L���b�V���������ł��������}�b�v�g�t�@�C�����g����WORKAROUND�֐����g����B
 */

#include <ntddk.h>
#include "minisop.h"

#ifdef USE_CACHEHACK
#include "miniifs.h"
#endif  /* USE_CACHEHACK */

#define PENDING_SOP_MAX	65535

static VOID MiniSOP_ReleaseAllSOP(void);
static BOOLEAN MiniSOP_ExpandSOPList(void);

// SECTION_OBJECT_POINTERS�ێ��p
typedef struct tagMINISOP_SOP {
	UNICODE_STRING path; // �Ώۃt�@�C���̃p�X�@SOP�̓t�@�C�����Ɋ��蓖�Ă�̂ŏd���͂Ȃ�
    PSECTION_OBJECT_POINTERS pSOP; // ���蓖�Ă�SECTION_OBJECT_POINTERS�ւ̃|�C���^
    ULONG refCount; // �Q�ƃJ�E���g 0�ɂȂ�����path�������pSOP���������
} MINISOP_SOP, *PMINISOP_SOP;

// SOP�̓t�@�C���E�f�B���N�g���P�ʂŊǗ�����K�v����
static ULONG g_pendingSOPListCount = 0;
static PMINISOP_SOP g_pendingSOPList = NULL;

// �Q�ƃJ�E���g�𑝂₵��SOP���擾�@�Ȃ��ꍇ�͐V�K�쐬�@���Ȃ����-1
static BOOLEAN MiniSOP_ExpandSOPList(){
	PMINISOP_SOP oldBuffer = g_pendingSOPList;
	PMINISOP_SOP newBuffer = NULL;
	ULONG oldCount = g_pendingSOPListCount;
	ULONG newCount;
	if(g_pendingSOPListCount == PENDING_SOP_MAX){
		return FALSE; // �m�ۂł��Ȃ�
	}
	newCount = oldCount + 8; // ���܂�p�ɂ��Ƒ�ςȂ̂łƂ肠����8�t�@�C�����g��
	if(newCount > PENDING_SOP_MAX){
		newCount = PENDING_SOP_MAX;
	}
    newBuffer = ExAllocatePool(NonPagedPool, sizeof(MINISOP_SOP) * newCount);
    if(newBuffer == NULL){
		return FALSE; // �m�ۂł��Ȃ�
    }
	RtlZeroMemory(newBuffer, sizeof(MINISOP_SOP) * newCount);
    if(g_pendingSOPList != NULL){
    	// ���݂̓��e���R�s�[
		RtlCopyMemory(newBuffer, oldBuffer, sizeof(MINISOP_SOP) * oldCount);
    }
    g_pendingSOPList = newBuffer;
    g_pendingSOPListCount = newCount;
    if(oldBuffer != NULL){
    	ExFreePool(oldBuffer);
    }
	return TRUE; // OK
}

// SOP���X�g���������B���g�̃���������������B�h���C�o�̃A�����[�h���ɌĂяo���B
BOOLEAN MiniSOP_ReleaseSOPList(){
	PMINISOP_SOP oldBuffer = g_pendingSOPList;
	MiniSOP_ReleaseAllSOP(); // SOP��S�ă����[�X
	g_pendingSOPListCount = 0;
	g_pendingSOPList = NULL;
    if(oldBuffer != NULL){
    	ExFreePool(oldBuffer);
    }
	return TRUE; // OK
}

// �w�肵���t�@�C�����ɑΉ�����SOP���X�g�̃C���f�b�N�X��Ԃ��B
// ���X�g�ɖ����ꍇ�͐V�K���蓖�Ă���B���\�[�X�s���Ŋ��蓖�Ăł��Ȃ��ꍇ��-1��Ԃ��B
// �Ăԓx�ɎQ�ƃJ�E���g�����̂ŁA�s�v�ɂȂ�����K��������HostdrvReleaseSOPByIndex�܂���HostdrvReleaseSOP�����邱�ƁB
LONG MiniSOP_GetSOPIndex(UNICODE_STRING path){
	int i;
	for(i=0;i<g_pendingSOPListCount;i++){
		if(g_pendingSOPList[i].path.Buffer && RtlEqualUnicodeString(&path, &g_pendingSOPList[i].path, FALSE)){
			g_pendingSOPList[i].refCount++;
			return i; // ���ɂ���
		}
	}
	// �Ȃ������̂ŐV�K�쐬
	for(i=0;i<g_pendingSOPListCount;i++){
		if(!g_pendingSOPList[i].path.Buffer){
			ULONG allocSize = path.Length;
			g_pendingSOPList[i].path.Length = path.Length;
			if(allocSize == 0) allocSize = 1; // �K��1byte�͊m��
			g_pendingSOPList[i].path.MaximumLength = allocSize;
	        g_pendingSOPList[i].path.Buffer = ExAllocatePool(NonPagedPool, allocSize);
	        if(g_pendingSOPList[i].path.Buffer == NULL){
				return -1;
	        }
	        RtlCopyMemory(g_pendingSOPList[i].path.Buffer, path.Buffer, g_pendingSOPList[i].path.Length);
	        g_pendingSOPList[i].pSOP = ExAllocatePool(NonPagedPool, sizeof(SECTION_OBJECT_POINTERS));
	        if(g_pendingSOPList[i].pSOP == NULL){
	        	ExFreePool(g_pendingSOPList[i].path.Buffer);
	        	g_pendingSOPList[i].path.Buffer = NULL;
	        	g_pendingSOPList[i].path.Length = 0;
	        	g_pendingSOPList[i].path.MaximumLength = 0;
				return -1;
	        }
	        RtlZeroMemory(g_pendingSOPList[i].pSOP, sizeof(SECTION_OBJECT_POINTERS));
			g_pendingSOPList[i].refCount++;
	        return i;
		}
	}
	// �󂫂��Ȃ������̂Ŋg�������݂�
	if(MiniSOP_ExpandSOPList()){
		// �g���ł����烊�g���C
		return MiniSOP_GetSOPIndex(path);
	}
	return -1;
}

// �w�肵��SOP���X�g�̃C���f�b�N�X�ɑΉ�����SECTION_OBJECT_POINTERS�ւ̃|�C���^��Ԃ��B
// �����ȃC���f�b�N�X�ł����NULL���Ԃ�
PSECTION_OBJECT_POINTERS MiniSOP_GetSOP(LONG i){
	if(i < 0 || g_pendingSOPListCount <= i){
		return NULL; // ����
	}
	return g_pendingSOPList[i].pSOP;
}

// �w�肵���ԍ���SOP�̎Q�ƃJ�E���g�����炷�@0�ɂȂ�������
VOID MiniSOP_ReleaseSOPByIndex(LONG i){
	if(i < 0 || g_pendingSOPListCount <= i){
		return; // ����
	}
	if(g_pendingSOPList[i].refCount > 0){
		g_pendingSOPList[i].refCount--;
	}
	if(g_pendingSOPList[i].refCount==0){
		// �Q�Ɩ����Ȃ����̂ŏ���
    	ExFreePool(g_pendingSOPList[i].path.Buffer);
    	g_pendingSOPList[i].path.Buffer = NULL;
    	g_pendingSOPList[i].path.Length = 0;
    	ExFreePool(g_pendingSOPList[i].pSOP);
    	g_pendingSOPList[i].pSOP = NULL;
	}
}

// �w�肵��SOP�̎Q�ƃJ�E���g�����炷�@0�ɂȂ�������
VOID MiniSOP_ReleaseSOP(PSECTION_OBJECT_POINTERS lpSOP){
	int i;
	for(i=0;i<g_pendingSOPListCount;i++){
		if(g_pendingSOPList[i].pSOP == lpSOP){
			MiniSOP_ReleaseSOPByIndex(i);
			return;
		}
	}
}

// �S�Ă�SOP�����
static VOID MiniSOP_ReleaseAllSOP(){
	int i;
    // SOP���X�g����
    for (i = 0; i < g_pendingSOPListCount; i++) {
        if (g_pendingSOPList[i].pSOP != NULL) {
        	ExFreePool(g_pendingSOPList[i].path.Buffer);
        	g_pendingSOPList[i].path.Buffer = NULL;
        	g_pendingSOPList[i].path.Length = 0;
        	ExFreePool(g_pendingSOPList[i].pSOP);
        	g_pendingSOPList[i].pSOP = NULL;
        	g_pendingSOPList[i].refCount = 0;
        }
    }
}

VOID MiniSOP_SendFlushSOP(){
	int i;
	IO_STATUS_BLOCK iosb = {0};
    for (i = 0; i < g_pendingSOPListCount; i++) {
        if (g_pendingSOPList[i].pSOP != NULL) {
			if(g_pendingSOPList[i].pSOP->DataSectionObject){
				CcFlushCache(g_pendingSOPList[i].pSOP, NULL, 0, &iosb);
				MmFlushImageSection(g_pendingSOPList[i].pSOP, MmFlushForDelete);
			}
        }
    }
}


// HACK: ���܂��@�\�@�L���b�V���������ł��������}�b�v�g�t�@�C�����g����悤�ɂ���B
#ifdef USE_CACHEHACK

// ����I/O�̏����ۂ�Ԃ��֐��B�g��Ȃ��̂ŏ펞FALSE��Ԃ��B
BOOLEAN HostdrvFastIoCheckIfPossible (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject)
{
    return FALSE;
}

static FAST_IO_DISPATCH g_FastIoDispatch; // �펞����I/O�s��Ԃ������̃_�~�[

// �������̒��r���[�L���b�V���̂��߂̏������BDriverEntry�ŌĂԂ��ƁB
// DriverObject: DriverEntry�̈����Ŏ󂯎����DriverObject
VOID MiniSOP_InitializeCache(PDRIVER_OBJECT DriverObject){
    // ��{�I�ɃL���b�V�����Ή��Ȃ̂Ń`�F�b�N�֐��ȊO��NULL��
    RtlZeroMemory( &g_FastIoDispatch, sizeof( FAST_IO_DISPATCH ));
    g_FastIoDispatch.SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);
    g_FastIoDispatch.FastIoCheckIfPossible = HostdrvFastIoCheckIfPossible;
    DriverObject->FastIoDispatch = &g_FastIoDispatch;
}
// IRP_MJ_CREATE����������x�ɌĂԁB�L���b�V���������ăf�B�X�N����̍�READ�������B
// irpSp: IRP�X�^�b�N�|�C���^
VOID MiniSOP_HandleMjCreateCache(PIO_STACK_LOCATION irpSp){
    CC_FILE_SIZES sizes = {0};
	PFILE_OBJECT pFileObject = irpSp->FileObject;
    
	if(!pFileObject || !pFileObject->SectionObjectPointer){
		return;
	}
	
	// �L���b�V���T�C�Y��0�ɂ��邱�Ƃŋ����I�Ƀf�B�X�N����ēǍ�������
	CcSetFileSizes(pFileObject, &sizes); 
}
// IRP_MJ_CLEANUP����������x�ɌĂԁB�L���b�V���̓��e�������I�Ƀf�B�X�N��WRITE������B
// irpSp: IRP�X�^�b�N�|�C���^
VOID MiniSOP_HandleMjCleanupCache(PIO_STACK_LOCATION irpSp){
	IO_STATUS_BLOCK iosb = {0};
	PFILE_OBJECT pFileObject = irpSp->FileObject;

	if(!pFileObject || !pFileObject->SectionObjectPointer || !pFileObject->SectionObjectPointer->DataSectionObject){
		return;
	}
	
	// �L���b�V���̓��e�������I�Ƀf�B�X�N�֏����߂�����
	CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &iosb);
	//MmFlushImageSection(pFileObject->SectionObjectPointer, MmFlushForDelete);
	//CcUninitializeCacheMap(pFileObject, NULL, NULL);
}
// IRP_MJ_SET_INFORMATION�Ŏ��ۂ̃t�@�C���T�C�Y��ύX���� �O�� �ĂԁB�L���b�V���̃t�@�C�������ύX��ʒm�B
// ���ۂ̏����́A�w�肳�ꂽ�t�@�C�������ȏ�ň�UWRITE����遨SetInformation�Ő؂�̂Ă�@�Ƃ�������ɂȂ�
// irpSp: IRP�X�^�b�N�|�C���^
VOID MiniSOP_HandlePreMjSetInformationCache(PIO_STACK_LOCATION irpSp){
    if(irpSp->Parameters.QueryFile.FileInformationClass == FileEndOfFileInformation || 
       irpSp->Parameters.QueryFile.FileInformationClass == FileAllocationInformation){
		PFILE_OBJECT pFileObject = irpSp->FileObject;
    	if(pFileObject && pFileObject->SectionObjectPointer){
			IO_STATUS_BLOCK iosb = {0};
            CC_FILE_SIZES sizes = {0};
			CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &iosb); // ���݂̃L���b�V�����f�B�X�N�֏����߂�
			CcSetFileSizes(pFileObject, &sizes); // �L���b�V�������Z�b�g
    	}
	}
}

#endif  /* USE_CACHEHACK */

