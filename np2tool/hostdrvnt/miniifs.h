// �ŏ��\���t�@�C���V�X�e���h���C�o�𓮂������߂̕K�v�ŏ����̊֐���\���̂̒�`
// �ȉ��̐����Ȓ�`��ntifs.h�ɑ��݁B�����Ă���Ȃ炻���include���Ă��悢

typedef struct _FSRTL_COMMON_FCB_HEADER {
  CSHORT        NodeTypeCode;
  CSHORT        NodeByteSize;
  UCHAR         Flags;
  UCHAR         IsFastIoPossible;
  UCHAR         Flags2;
  UCHAR         Reserved : 4;
  UCHAR         Version : 4;
  PVOID         Resource;
  PVOID         PagingIoResource;
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER FileSize;
  LARGE_INTEGER ValidDataLength;
} FSRTL_COMMON_FCB_HEADER;

typedef enum _MMFLUSH_TYPE {
    MmFlushForDelete,
    MmFlushForWrite
} MMFLUSH_TYPE;

typedef
BOOLEAN (*PACQUIRE_FOR_LAZY_WRITE) (
             IN PVOID Context,
             IN BOOLEAN Wait
             );
typedef
VOID (*PRELEASE_FROM_LAZY_WRITE) (
             IN PVOID Context
             );
typedef
BOOLEAN (*PACQUIRE_FOR_READ_AHEAD) (
             IN PVOID Context,
             IN BOOLEAN Wait
             );
typedef
VOID (*PRELEASE_FROM_READ_AHEAD) (
             IN PVOID Context
             );

typedef struct _CACHE_MANAGER_CALLBACKS {
    PACQUIRE_FOR_LAZY_WRITE AcquireForLazyWrite;
    PRELEASE_FROM_LAZY_WRITE ReleaseFromLazyWrite;
    PACQUIRE_FOR_READ_AHEAD AcquireForReadAhead;
    PRELEASE_FROM_READ_AHEAD ReleaseFromReadAhead;
} CACHE_MANAGER_CALLBACKS, *PCACHE_MANAGER_CALLBACKS;

typedef struct _CC_FILE_SIZES {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER FileSize;
    LARGE_INTEGER ValidDataLength;
} CC_FILE_SIZES, *PCC_FILE_SIZES;

NTKERNELAPI
VOID
CcFlushCache (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    OUT PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    OUT PIO_STATUS_BLOCK IoStatus
);

NTKERNELAPI
VOID
CcSetFileSizes (
    IN PFILE_OBJECT   FileObject,
    IN PCC_FILE_SIZES FileSizes
);

NTKERNELAPI
VOID
CcPurgeCacheSection (
	IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
	IN PLARGE_INTEGER           FileOffset,
	IN ULONG                    Length,
	ULONG                    Flags
);
