
#if defined(SUPPORT_GPIB)

typedef struct {
	UINT8 enable;
	UINT8 irq; // ���荞��
	UINT8 mode; // �}�X�^=1, �X���[�u=0
	UINT8 gpibaddr; // GP-IB�A�h���X(0x0�`0x1F)
	UINT8 ifcflag; // IFC# 1=�A�N�e�B�u, 0=��A�N�e�B�u

} _GPIB, *GPIB;

#ifdef __cplusplus
extern "C" {
#endif

extern	_GPIB		gpib;

void gpibint(NEVENTITEM item);

void gpibio_reset(const NP2CFG *pConfig);
void gpibio_bind(void);

#ifdef __cplusplus
}
#endif

#endif

