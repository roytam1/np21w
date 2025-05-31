/*
 *	minisop.h
 */
 
// ���#include <ntddk.h>���K�v�B

// ������`���Ă����ƃL���b�V���N���A�p��WORKAROUND�֐����g�p�\
#define USE_CACHEHACK

// SOP���X�g���������B���g�̃���������������B�h���C�o�̃A�����[�h���ɌĂяo���B
BOOLEAN MiniSOP_ReleaseSOPList();

// �w�肵���t�@�C�����ɑΉ�����SOP���X�g�̃C���f�b�N�X��Ԃ��B
// ���X�g�ɖ����ꍇ�͐V�K���蓖�Ă���B���\�[�X�s���Ŋ��蓖�Ăł��Ȃ��ꍇ��-1��Ԃ��B
// �Ăԓx�ɎQ�ƃJ�E���g�����̂ŁA�s�v�ɂȂ�����K��������HostdrvReleaseSOPByIndex�܂���HostdrvReleaseSOP�����邱�ƁB
LONG MiniSOP_GetSOPIndex(UNICODE_STRING path);

// �w�肵��SOP���X�g�̃C���f�b�N�X�ɑΉ�����SECTION_OBJECT_POINTERS�ւ̃|�C���^��Ԃ��B
// �����ȃC���f�b�N�X�ł����NULL���Ԃ�
PSECTION_OBJECT_POINTERS MiniSOP_GetSOP(LONG i);

// �w�肵��SOP�̎Q�ƃJ�E���g�����炷�@0�ɂȂ������������B
VOID MiniSOP_ReleaseSOP(PSECTION_OBJECT_POINTERS lpSOP);
// �w�肵���C���f�b�N�X��SOP�̎Q�ƃJ�E���g�����炷�@0�ɂȂ������������B
VOID MiniSOP_ReleaseSOPByIndex(LONG i);


// HACK: ���܂��@�\�@�L���b�V���������ł��������}�b�v�g�t�@�C�����g����悤�ɂ���B
#ifdef USE_CACHEHACK

// �������̒��r���[�L���b�V���̂��߂̏������BDriverEntry�ŌĂԂ��ƁB
// DriverObject: DriverEntry�̈����Ŏ󂯎����DriverObject
VOID MiniSOP_InitializeCache(PDRIVER_OBJECT DriverObject);
// IRP_MJ_CREATE����������x�ɌĂԁB�L���b�V���������ăf�B�X�N����̍�READ�������B
// irpSp: IRP�X�^�b�N�|�C���^
VOID MiniSOP_HandleMjCreateCache(PIO_STACK_LOCATION irpSp);
// IRP_MJ_CLEANUP����������x�ɌĂԁB�L���b�V���̓��e�������I�Ƀf�B�X�N��WRITE������B
// irpSp: IRP�X�^�b�N�|�C���^
VOID MiniSOP_HandleMjCleanupCache(PIO_STACK_LOCATION irpSp);
// IRP_MJ_SET_INFORMATION�Ŏ��ۂ̃t�@�C���T�C�Y��ύX���� �O�� �ĂԁB�L���b�V���̃t�@�C�������ύX��ʒm�B
// ���ۂ̏����́A�w�肳�ꂽ�t�@�C�������ȏ�ň�UWRITE����遨SetInformation�Ő؂�̂Ă�@�Ƃ�������ɂȂ�
// irpSp: IRP�X�^�b�N�|�C���^
VOID MiniSOP_HandlePreMjSetInformationCache(PIO_STACK_LOCATION irpSp);

#endif  /* USE_CACHEHACK */
