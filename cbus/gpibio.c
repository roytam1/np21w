/**
 * @file	gpib.c
 * @brief	Implementation of PC-9801-06/19/29/29K/29N GP-IB(IEEE-488.1) Interface (uPD7210)
 */

// ���ӁF�܂������������ĂȂ��̂Ŏg���܂���iGP-IB�{�[�h������œ����@��������ĂȂ�������j

#include	"compiler.h"

#include	"dosio.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"bios/biosmem.h"
#include	"gpibio.h"
#include	"bios/bios.h"

#if defined(SUPPORT_GPIB)


	_GPIB		gpib;

UINT8 irq2idx(UINT8 irq){
	switch(irq){
	case 3:
		return 0x0;
	case 10:
		return 0x1;
	case 12:
		return 0x2;
	case 13:
		return 0x3;
	}
	return 0x0;
}
UINT8 idx2irq(UINT8 idx){
	switch(idx){
	case 0x0:
		return 3;
	case 0x1:
		return 10;
	case 0x2:
		return 12;
	case 0x3:
		return 13;
	}
	return 3;
}

// ----

// Byte Out 
static void IOOUTCALL gpib_o1(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Data In
static REG8 IOOUTCALL gpib_i1(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Interrupt Mask 1
static void IOOUTCALL gpib_o3(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Interrupt Status 1
static REG8 IOOUTCALL gpib_i3(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Interrupt Mask 2
static void IOOUTCALL gpib_o5(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Interrupt Status 2
static REG8 IOOUTCALL gpib_i5(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Serial Poll Mode
static void IOOUTCALL gpib_o7(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Serial Poll Status
static REG8 IOOUTCALL gpib_i7(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Address Mode
static void IOOUTCALL gpib_o9(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Address Status
static REG8 IOOUTCALL gpib_i9(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Auxiliary Mode
static void IOOUTCALL gpib_ob(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Command Pass Through
static REG8 IOOUTCALL gpib_ib(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Address 0/1
static void IOOUTCALL gpib_od(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Address 0
static REG8 IOOUTCALL gpib_id(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// End of String
static void IOOUTCALL gpib_of(UINT port, REG8 dat) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	(void)port;
}
// Address 1
static REG8 IOOUTCALL gpib_if(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h�Ɋۓ���
	return 0xff;
}

// Read Switch
static REG8 IOOUTCALL gpib_i99(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h����ݒ�����
	return (irq2idx(gpib.irq) << 6)|(gpib.mode << 5)|(gpib.gpibaddr);
}
// Read IFC
static REG8 IOOUTCALL gpib_i9b(UINT port) {
	// TODO: Windows�Ή��� uPD7210�݊� GP-IB�{�[�h����IFC�t���O������Ă���
	return (gpib.ifcflag ? 0x00: 0x80)|(gpib_i99(0x99) & ~0x80);
}

// �g��
static void IOOUTCALL gpib_o0(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_i0(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_o2(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_i2(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_o4(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_i4(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_o6(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_i6(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_o8(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_i8(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_oa(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_ia(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_oc(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_ic(UINT port) {
	return 0xff;
}

static void IOOUTCALL gpib_oe(UINT port, REG8 dat) {
	(void)port;
}
static REG8 IOOUTCALL gpib_ie(UINT port) {
	return 0xff;
}


static const IOOUT gpib_o[] = {
					gpib_o0, gpib_o1, gpib_o2, gpib_o3, gpib_o4, gpib_o5, gpib_o6, gpib_o7, gpib_o8, gpib_o9, gpib_oa, gpib_ob, gpib_oc, gpib_od, gpib_oe, gpib_of};

static const IOINP gpib_i[] = {
					gpib_i0, gpib_i1, gpib_i2, gpib_i3, gpib_i4, gpib_i5, gpib_i6, gpib_i7, gpib_i8, gpib_i9, gpib_ia, gpib_ib, gpib_ic, gpib_id, gpib_ie, gpib_if};

void gpibio_reset(const NP2CFG *pConfig) {
	
	OEMCHAR	path[MAX_PATH];
	FILEH	fh;
	OEMCHAR tmpbiosname[16];
	
	_tcscpy(tmpbiosname, OEMTEXT("gpib.rom"));
	getbiospath(path, tmpbiosname, NELEMENTS(path));
	fh = file_open_rb(path);

	// GP-IB BIOS �g��ROM(D4000h - D5FFFh) �L��?
	if((np2cfg.memsw[3] & 0x20) == 0){
		gpib.enable = 0;
		return;
	}
	gpib.enable = 1;
	gpib.irq = np2cfg.gpibirq;
	gpib.mode = np2cfg.gpibmode;
	gpib.gpibaddr = np2cfg.gpibaddr;
	gpib.exiobase = np2cfg.gpibexio;
	
	if (fh != FILEH_INVALID) {
		// GP-IB BIOS
		if (file_read(fh, mem + 0x0d4000, 0x2000) == 0x2000) {
			TRACEOUT(("load gpib.rom"));
		}else{
			//CopyMemory(mem + 0x0d4000, gpibbios, sizeof(gpibbios));
			//TRACEOUT(("use simulate gpib.rom"));
		}
		file_close(fh);
	}else{
		//CopyMemory(mem + 0x0d4000, gpibbios, sizeof(gpibbios));
		//TRACEOUT(("use simulate gpib.rom"));
	}
	
	(void)pConfig;
}

void gpibio_bind(void) {
	
	int i;

	// GP-IB �L��?
	if(!gpib.enable){
		return;
	}
	
	// �W��I/O�|�[�g�ݒ�
	for(i=0;i<16;i++){
		if(gpib_o[i]){
			iocore_attachout(0xC0 + i, gpib_o[i]);
		}
		if(gpib_i[i]){
			iocore_attachinp(0xC0 + i, gpib_i[i]);
		}
	}
	iocore_attachinp(0x99, gpib_i99);
	iocore_attachinp(0x9b, gpib_i9b);
	
	// �J�X�^��I/O�|�[�g�x�[�X�A�h���X
	if(gpib.exiobase != 0){
		for(i=0;i<16;i++){
			if(gpib_o[i]){
				iocore_attachout(gpib.exiobase + i, gpib_o[i]);
			}
			if(gpib_i[i]){
				iocore_attachinp(gpib.exiobase + i, gpib_i[i]);
			}
		}
	}

	//// AZI-4301P
	//for(i=0;i<16;i++){
	//	if(gpib_o[i]){
	//		iocore_attachout(0xD0 + i, gpib_o[i]);
	//	}
	//	if(gpib_i[i]){
	//		iocore_attachinp(0xD0 + i, gpib_i[i]);
	//	}
	//}

}
#endif

