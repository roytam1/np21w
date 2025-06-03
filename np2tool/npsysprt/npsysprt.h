#define IOCTL_NP2_GENERIC \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_SIMPLE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_CLOCK_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_CLOCK_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_MOUSEPOS_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS)

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

// IOCTL_NP2_SIMPLE�̏ꍇ�̌`��
typedef struct _IOPORT_NP2_SIMPLE_DATA {
    UCHAR command[16];
    ULONG paramLength;
    union {
    	UCHAR  b[4];
    	USHORT w[2];
    	ULONG  d;
    } param;
    UCHAR readBuffer[16];
} IOPORT_NP2_SIMPLE_DATA, *PIOPORT_NP2_SIMPLE_DATA;

// IOCTL_NP2_CLOCK_WRITE, IOCTL_NP2_CLOCK_READ�̏ꍇ�̌`��
typedef struct _IOPORT_NP2_CLOCK_DATA {
    ULONG  clockMul;
} IOPORT_NP2_CLOCK_DATA, *PIOPORT_NP2_CLOCK_DATA;

// IOCTL_NP2_MOUSEPOS_READ�̏ꍇ�̌`��
typedef struct _IOPORT_NP2_MOUSEPOS_DATA {
    USHORT  absPosX; // X���W ��ʑS�̂�0�`65535�Ƀ}�b�s���O
    USHORT  absPosY; // Y���W ��ʑS�̂�0�`65535�Ƀ}�b�s���O
} IOPORT_NP2_MOUSEPOS_DATA, *PIOPORT_NP2_MOUSEPOS_DATA;
