#define IOCTL_NP2_GENERIC \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_CLOCK_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_CLOCK_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_NP2_GENERIC�̏ꍇ�̌`��
ULONG NP2 system port command text length
UCHAR[] NP2 system port command text
ULONG Parameter data length (max. 4byte)
UCHAR[] Parameter data
ULONG Read buffer length
UCHAR[] Read bufer
DeviceIoControl�ɓn���o�b�t�@�͓��o�͓������̂ɂ��邱�Ɓi�����ꏊ�ɏ������݁j
�܂��A�o�b�t�@�T�C�Y�͏�L�T�C�Y�̍��v����������n�����Ɓi�����Ă����Ȃ��Ă��s�j
������߂�l���s�v�ȏꍇ�͒�����0��ݒ肷�邱��
*/

typedef struct _IOPORT_NP2_CLOCK_DATA {
    ULONG  clockMul;
} IOPORT_NP2_CLOCK_DATA, *PIOPORT_NP2_CLOCK_DATA;