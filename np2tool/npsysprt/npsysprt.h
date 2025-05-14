#define IOCTL_NP2_GENERIC \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_CLOCK_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NP2_CLOCK_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* IOCTL_NP2_GENERICの場合の形式
ULONG NP2 system port command text length
UCHAR[] NP2 system port command text
ULONG Parameter data length (max. 4byte)
UCHAR[] Parameter data
ULONG Read buffer length
UCHAR[] Read bufer
DeviceIoControlに渡すバッファは入出力同じものにすること（同じ場所に書き込み）
また、バッファサイズは上記サイズの合計をきっちり渡すこと（多くても少なくても不可）
引数や戻り値が不要な場合は長さに0を設定すること
*/

typedef struct _IOPORT_NP2_CLOCK_DATA {
    ULONG  clockMul;
} IOPORT_NP2_CLOCK_DATA, *PIOPORT_NP2_CLOCK_DATA;