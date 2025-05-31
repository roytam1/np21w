/*
 *	irplock.h
 */
 
// ���#include <ntddk.h>���K�v�B

// IrpStack�̃��b�N���
typedef struct tagIRPSTACKLOCK_INFO {
	ULONG isValid;
    PMDL mdlIrpStack;
    PMDL mdlFileObject;
    PMDL mdlFileObjectFileNameBuffer;
    PMDL mdlQueryDirectoryFileName;
    PMDL mdlQueryDirectoryFileNameBuffer;
} IRPSTACKLOCK_INFO, *PIRPSTACKLOCK_INFO;

// �y�[�W���O����Ȃ�IrpStack���擾����BNULL�̏ꍇ���s
// src: �R�s�[��
// return -> �y�[�W���O����Ȃ�IrpStack
PIO_STACK_LOCATION CreateNonPagedPoolIrpStack(PIO_STACK_LOCATION src);

// �y�[�W���O����Ȃ�IrpStack��j������
// src: �y�[�W���O����Ȃ�IrpStack
// back: �����߂���IrpStack
VOID ReleaseNonPagedPoolIrpStack(PIO_STACK_LOCATION src, PIO_STACK_LOCATION back);

// �y�[�W���O����Ȃ�IrpStack���擾����BNULL�̏ꍇ���s
// src: ���b�N������rpStack
// return -> IRP���b�N���
IRPSTACKLOCK_INFO LockIrpStack(PIO_STACK_LOCATION src);

// �y�[�W���O����Ȃ�IrpStack��j������
// src: ���b�N���ꂽIrpStack
// lpInfo: IRP���b�N���ւ̃|�C���^
VOID UnlockIrpStack(PIRPSTACKLOCK_INFO lpInfo);
