
// PC-9821 PCI�o�X

// �����Configuration Mechanism #1�Ή��Ńo�X�ԍ�0�̂݁Bbios1a.c�ɃG�~�����[�V����PCI BIOS������܂��B

#include	"compiler.h"

#if defined(SUPPORT_PC9821)

#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"

#if defined(SUPPORT_PCI)

#include	"dosio.h"
#include	"pci/cbusbridge.h"
#include	"pci/98graphbridge.h"
#include	"wab/cirrus_vga_extern.h"

#define GETCFGREG_B(reg, ofs)			(*((UINT8*)((reg) + (ofs))))
#define GETCFGREG_W(reg, ofs)			(*((UINT16*)((reg) + (ofs))))
#define GETCFGREG_D(reg, ofs)			(*((UINT32*)((reg) + (ofs))))

#define SETCFGREG_B(reg, ofs, value)	(GETCFGREG_B(reg, ofs) = value)
#define SETCFGREG_W(reg, ofs, value)	(GETCFGREG_W(reg, ofs) = value)
#define SETCFGREG_D(reg, ofs, value)	(GETCFGREG_D(reg, ofs) = value)

#define SETCFGREG_B_MASK(reg, ofs, value, mask)	(GETCFGREG_B(reg, ofs) = (GETCFGREG_B(reg, ofs) & mask) | (value & ~mask))
#define SETCFGREG_W_MASK(reg, ofs, value, mask)	(GETCFGREG_W(reg, ofs) = (GETCFGREG_W(reg, ofs) & mask) | (value & ~mask))
#define SETCFGREG_D_MASK(reg, ofs, value, mask)	(GETCFGREG_D(reg, ofs) = (GETCFGREG_D(reg, ofs) & mask) | (value & ~mask))

UINT8 PCI_INTLINE2IRQTBL[] = {
//  INTA INTB INTC INTD
	0,   1,   2,   3, // slot #0
	1,   2,   3,   0, // slot #1 
	2,   3,   0,   1, // slot #2 
	3,   2,   1,   0, // slot #3
};

// ����͂Ȃ񂾂낤�H
static void setRAM_D000(UINT8 dat){
	//UINT8 dat = *((UINT8*)(pcidev.devices[0].cfgreg8 + 0x64));
	UINT32	work;
	work = CPU_RAM_D000 & 0x03ff;
	if (dat & 0x10) {
		work |= 0x0400;
	}
	if (dat & 0x20) {
		work |= 0x0800;
	}
	if (dat & 0x80) {
		work |= 0xf000;
	}
	CPU_RAM_D000 = (UINT16)work;
}


UINT8* IOOUTCALL pci_getirqtbl(UINT port, REG8 dat) {
	return &(pcidev.devices[pcidev_cbusbridge_deviceid].cfgreg8[0x60]); // C-Bus Bridge���
}
UINT8 IOOUTCALL pci_getslotnumber(UINT32 devNumber){
	return devNumber & 0x3;
}
UINT8 IOOUTCALL pci_getirq(UINT32 devNumber){
	int intpin = (pcidev.devices[devNumber].header.interruptpin - 1);
	if(intpin < 0 || intpin > 3){
		return 0;
	}else{
		return pcidev.devices[pcidev_cbusbridge_deviceid].cfgreg8[0x60 + PCI_INTLINE2IRQTBL[intpin + 4*pci_getslotnumber(devNumber)]]; // C-Bus Bridge���
	}
}
UINT8 IOOUTCALL pci_getirq2(UINT8 intpin, UINT8 slot){
	if(intpin > 3){
		return 0;
	}else{
		return pcidev.devices[pcidev_cbusbridge_deviceid].cfgreg8[0x60 + PCI_INTLINE2IRQTBL[intpin + 4*slot]]; // C-Bus Bridge���
	}
}

void pcidev_pcmc_cfgreg_w(UINT32 devNumber, UINT8 funcNumber, UINT8 cfgregOffset, UINT8 sizeinbytes, UINT32 value){

}

static void IOOUTCALL pci_o0cf8(UINT port, REG8 dat) {

	pcidev.reg_cse = dat & 0xfe;
}
static void IOOUTCALL pci_o0cf9(UINT port, REG8 dat) {
	
	pcidev.reg_trc = dat;
}
static void IOOUTCALL pci_o0cfa(UINT port, REG8 dat) {
	
	pcidev.reg_fwd = dat;
}
static void IOOUTCALL pci_o0cfb(UINT port, REG8 dat) {
	
	pcidev.reg_cms = dat;
}

static REG8 IOINPCALL pci_i0cf8(UINT port) {

	return pcidev.reg_cse;
}
static REG8 IOINPCALL pci_i0cf9(UINT port) {
	
	return pcidev.reg_trc;
}
static REG8 IOINPCALL pci_i0cfa(UINT port) {
	
	return pcidev.reg_fwd;
}
static REG8 IOINPCALL pci_i0cfb(UINT port) {
	
	return pcidev.reg_cms;
}

void IOOUTCALL pcidev_w8_0xcfc(UINT port, UINT8 value) {
	
	if (pcidev.reg32_caddr & 0x80000000) {
		//if((pcidev.reg32_caddr & 0x00ff0000) == 0){ 
			// Configuration Mechanism #1 Type0,1
			UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
			UINT8 idselSelect = (pcidev.reg32_caddr >> 11) & 0x1f;
			UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
			UINT8 cfgregOffset = ((pcidev.reg32_caddr >> 0) & 0xff) + (port-0xcfc);
			if(!pcidev.enable && idselSelect!=0) return;
			if(pcidev.devices[idselSelect].enable/* && funcNumber==0*/){
				UINT32 mask = GETCFGREG_B(pcidev.devices[idselSelect].cfgreg8rom, cfgregOffset);
				SETCFGREG_B_MASK(pcidev.devices[idselSelect].cfgreg8, cfgregOffset, value, mask);
				if(pcidev.devices[idselSelect].regwfn){
					(*pcidev.devices[idselSelect].regwfn)(idselSelect, funcNumber, cfgregOffset, 1, value);
				}
			}
			if(idselSelect==0 && cfgregOffset==0x64) setRAM_D000(value);
		//}else{ 
		//	// Configuration Mechanism #1 Type1
		//	UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
		//	UINT8 devNumber = (pcidev.reg32_caddr >> 11) & 0x1f;
		//	UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
		//	UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
		//}
	}
}
void IOOUTCALL pcidev_w16_0xcfc(UINT port, UINT16 value) {
	
	if (pcidev.reg32_caddr & 0x80000000) {
		//if((pcidev.reg32_caddr & 0x00ff0000) == 0){ 
			// Configuration Mechanism #1 Type0,1
			UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
			UINT8 idselSelect = (pcidev.reg32_caddr >> 11) & 0x1f;
			UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
			UINT8 cfgregOffset = ((pcidev.reg32_caddr >> 0) & 0xff) + (port-0xcfc);
			if(!pcidev.enable && idselSelect!=0) return;
			if(pcidev.devices[idselSelect].enable/* && funcNumber==0*/){
				UINT32 mask = GETCFGREG_W(pcidev.devices[idselSelect].cfgreg8rom, cfgregOffset);
				SETCFGREG_W_MASK(pcidev.devices[idselSelect].cfgreg8, cfgregOffset, value, mask);
				if(pcidev.devices[idselSelect].regwfn){
					(*pcidev.devices[idselSelect].regwfn)(idselSelect, funcNumber, cfgregOffset, 2, value);
				}
			}
			if(idselSelect==0 && cfgregOffset==0x64){
				//setRAM_D000((value >> 0) & 0xff);
				setRAM_D000((value >> 8) & 0xff);
			}
		//}else{ 
		//	// Configuration Mechanism #1 Type1
		//	UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
		//	UINT8 devNumber = (pcidev.reg32_caddr >> 11) & 0x1f;
		//	UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
		//	UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
		//}
	}
}
void IOOUTCALL pcidev_w32(UINT port, UINT32 value) {
	
	if(port==0xcf8){
		pcidev.reg32_caddr = value;
	}else{
		if (pcidev.reg32_caddr & 0x80000000) {
			//if((pcidev.reg32_caddr & 0x00ff0000) == 0){ 
				// Configuration Mechanism #1 Type0,1
				UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
				UINT8 idselSelect = (pcidev.reg32_caddr >> 11) & 0x1f;
				UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
				UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
				if(!pcidev.enable && idselSelect!=0) return;
				if(pcidev.devices[idselSelect].enable/* && funcNumber==0*/){
					UINT32 mask = GETCFGREG_D(pcidev.devices[idselSelect].cfgreg8rom, cfgregOffset);
					SETCFGREG_D_MASK(pcidev.devices[idselSelect].cfgreg8, cfgregOffset, value, mask);
					if(pcidev.devices[idselSelect].regwfn){
						(*pcidev.devices[idselSelect].regwfn)(idselSelect, funcNumber, cfgregOffset, 4, value);
					}
				}
				if(idselSelect==0 && cfgregOffset==0x64){
					//setRAM_D000((value >> 0) & 0xff);
					//setRAM_D000((value >> 8) & 0xff);
					//setRAM_D000((value >> 16) & 0xff);
					setRAM_D000((value >> 24) & 0xff);
				}
			//}else{ 
			//	// Configuration Mechanism #1 Type1
			//	UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
			//	UINT8 devNumber = (pcidev.reg32_caddr >> 11) & 0x1f;
			//	UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
			//	UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
			//}
		}
	}
}

UINT8 IOOUTCALL pcidev_r8_0xcfc(UINT port) {
	
	//if((pcidev.reg32_caddr & 0x00ff0000) == 0){ 
		// Configuration Mechanism #1 Type0,1
		UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
		UINT8 idselSelect = (pcidev.reg32_caddr >> 11) & 0x1f;
		UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
		UINT8 cfgregOffset = ((pcidev.reg32_caddr >> 0) & 0xff) + (port-0xcfc);
		if(!pcidev.enable && idselSelect!=0) return 0xff;
		if(pcidev.devices[idselSelect].enable/* && funcNumber==0*/){
			return GETCFGREG_B(pcidev.devices[idselSelect].cfgreg8, cfgregOffset);
		}
	//}else{ 
	//	// Configuration Mechanism #1 Type1
	//	UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
	//	UINT8 devNumber = (pcidev.reg32_caddr >> 11) & 0x1f;
	//	UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
	//	UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
	//}
	return 0xff;
}
UINT16 IOOUTCALL pcidev_r16_0xcfc(UINT port) {
	
	//if((pcidev.reg32_caddr & 0x00ff0000) == 0){ 
		// Configuration Mechanism #1 Type0,1
		UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
		UINT8 idselSelect = (pcidev.reg32_caddr >> 11) & 0x1f;
		UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
		UINT8 cfgregOffset = ((pcidev.reg32_caddr >> 0) & 0xff) + (port-0xcfc);
		if(!pcidev.enable && idselSelect!=0) return 0xffff;
		if(pcidev.devices[idselSelect].enable/* && funcNumber==0*/){
			return GETCFGREG_W(pcidev.devices[idselSelect].cfgreg8, cfgregOffset);
		}
	//}else{ 
	//	// Configuration Mechanism #1 Type1
	//	UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
	//	UINT8 devNumber = (pcidev.reg32_caddr >> 11) & 0x1f;
	//	UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
	//	UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
	//}
	return 0xffff;
}
UINT32 IOOUTCALL pcidev_r32(UINT port) {
	
	if(port==0xcf8){
		return pcidev.reg32_caddr;
	}else{
		//if((pcidev.reg32_caddr & 0x00ff0000) == 0){ 
			// Configuration Mechanism #1 Type0,1
			UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
			UINT8 idselSelect = (pcidev.reg32_caddr >> 11) & 0x1f;
			UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
			UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
			if(!pcidev.enable && idselSelect!=0) return 0xffffffff;
			if(pcidev.devices[idselSelect].enable/* && funcNumber==0*/){
				return GETCFGREG_D(pcidev.devices[idselSelect].cfgreg8, cfgregOffset);
			}
		//}else{ 
		//	// Configuration Mechanism #1 Type1
		//	UINT8 busNumber = (pcidev.reg32_caddr >> 16) & 0xff;
		//	UINT8 devNumber = (pcidev.reg32_caddr >> 11) & 0x1f;
		//	UINT8 funcNumber = (pcidev.reg32_caddr >> 8) & 0x7;
		//	UINT8 cfgregOffset = (pcidev.reg32_caddr >> 0) & 0xff;
		//}
	}
	return 0xffffffff;
}

void pcidev_reset(const NP2CFG *pConfig) {

	int i;
	int devid = 0;
	OEMCHAR	path[MAX_PATH];
	FILEH	fh;
	OEMCHAR tmpbiosname[16];
	
	ZeroMemory(&pcidev, sizeof(pcidev));

	pcidev.enable = np2cfg.usepci;

	pcidev.membankd8 = 0xFE; // IDE bank 
	
	_tcscpy(tmpbiosname, OEMTEXT("pci.rom"));
	getbiospath(path, tmpbiosname, NELEMENTS(path));
	fh = file_open_rb(path);
	if (fh == FILEH_INVALID) {
		_tcscpy(tmpbiosname, OEMTEXT("bank0.bin"));
		getbiospath(path, tmpbiosname, NELEMENTS(path));
		fh = file_open_rb(path);
	}
	if (fh != FILEH_INVALID) {
		// PCI BIOS
		if (file_read(fh, pcidev.biosrom, 0x8000) == 0x8000) {
			TRACEOUT(("load pci.rom"));
			_tcscpy(pcidev.biosname, tmpbiosname);
		}else{
			TRACEOUT(("use simulate pci.rom"));
		}
		file_close(fh);
	}else{
		TRACEOUT(("use simulate pci.rom"));
	}

	pcidev.reg_cse = 0;
	pcidev.reg_trc = 0;
	pcidev.reg_fwd = 0;
	pcidev.reg_cms = 0x80; // Configuration Mechanism #1 Enable
	
	memset(pcidev.devices, 0xff, sizeof(_PCIDEVICE)*PCI_DEVICES_MAX);

	for(i=0;i<PCI_DEVICES_MAX;i++){
		pcidev.devices[i].enable = 0;
		pcidev.devices[i].regwfn = NULL;
		pcidev.devices[i].slot = 0;
	}

	if(pcidev.enable){
		// i82430LX
		ZeroMemory(pcidev.devices+devid, sizeof(_PCIDEVICE));
		pcidev.devices[devid].enable = 1;
		pcidev.devices[devid].skipirqtbl = 1;
		pcidev.devices[devid].regwfn = &pcidev_pcmc_cfgreg_w;
		pcidev.devices[devid].header.vendorID = 0x8086;
		pcidev.devices[devid].header.deviceID = 0x04A3;//0x1237;//0x04A3;
		pcidev.devices[devid].header.command = 0x0006;//0x0106;//0x0006;
		pcidev.devices[devid].header.status = 0x0200;//0x2280;//0x0400;
		pcidev.devices[devid].header.revisionID = 0x02;//0x03;
		pcidev.devices[devid].header.classcode[0] = 0x00; // ���W�X�^���x���v���O���~���O�C���^�t�F�[�X
		pcidev.devices[devid].header.classcode[1] = 0x00; // �T�u�N���X�R�[�h
		pcidev.devices[devid].header.classcode[2] = 0x06; // �x�[�X�N���X�R�[�h
		pcidev.devices[devid].header.cachelinesize = 0;
		pcidev.devices[devid].header.latencytimer = 0x00;
		pcidev.devices[devid].header.headertype = 0;
		pcidev.devices[devid].header.BIST = 0x00;
		pcidev.devices[devid].header.interruptline = 0x00;
		pcidev.devices[devid].header.interruptpin = 0x01;
		pcidev.devices[devid].header.subsysID = 0x00;
		pcidev.devices[devid].header.subsysventorID = 0x00;
		pcidev.devices[devid].cfgreg8[0x50] = 0x00; // �z�X�gCPU�I��(HCS)
		pcidev.devices[devid].cfgreg8[0x51] = 0x01; // �f�^�[�{���g������(DFC)
		pcidev.devices[devid].cfgreg8[0x52] = 0x01; // 2���L���b�V������(SCC)
		pcidev.devices[devid].cfgreg8[0x53] = 0x00; // �z�X�g�ǎ��^�����݃o�b�t�@����(HBC)
		pcidev.devices[devid].cfgreg8[0x54] = 0x00; // PCI�ǎ��^�����݃o�b�t�@����(PBC)
		pcidev.devices[devid].cfgreg8[0x55] = 0x00; // 2���L���b�V������g�����W�X�^(SCCE)
		pcidev.devices[devid].cfgreg8[0x57] = 0x01; // DRAM����
		pcidev.devices[devid].cfgreg8[0x58] = 0x00; // DRAM�^�C�~���O(DT)
		pcidev.devices[devid].cfgreg8[0x59] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x5a] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x5b] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x5c] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x5d] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x5e] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x5f] = 0x00; // �v���O�����\�����}�b�v(PAM)
		pcidev.devices[devid].cfgreg8[0x60] = 0x10; // DRAM���[���E���W�X�^(DRB) ROW#0 
		pcidev.devices[devid].cfgreg8[0x61] = 0x20; // ROW#0,#1
		pcidev.devices[devid].cfgreg8[0x62] = 0x20; // ROW#0�`2
		pcidev.devices[devid].cfgreg8[0x63] = 0x20; // ROW#0�`3
		pcidev.devices[devid].cfgreg8[0x64] = 0x20; // ????? pcidev.devices[0].cfgreg8[0x63]; // ROW#0�`4
		pcidev.devices[devid].cfgreg8[0x65] = 0x20; // ????? pcidev.devices[0].cfgreg8[0x65]; // ROW#0�`5
		pcidev.devices[devid].cfgreg8[0x70] = 0x00; // �G���[�R�}���h(ERRCMD)
		pcidev.devices[devid].cfgreg8[0x71] = 0x00; // �G���[�X�e�[�^�X(ERRSTS)
		pcidev.devices[devid].cfgreg8[0x72] = 0x00; // SMRAM��Ԑ���(SMRS)
		SETCFGREG_W(pcidev.devices[devid].cfgreg8, 0x78, 0x0000); // ��������ԃM���b�v(MSG)
		SETCFGREG_D(pcidev.devices[devid].cfgreg8, 0x7C, 0x00000000); // �t���[���o�b�t�@�̈�(FBR)

		// ROM�̈�ݒ�
		pcidev.devices[devid].headerrom.vendorID = 0xffff;
		pcidev.devices[devid].headerrom.deviceID = 0xffff;
		pcidev.devices[devid].headerrom.status = 0xffff;
		pcidev.devices[devid].headerrom.revisionID = 0xff;
		pcidev.devices[devid].headerrom.classcode[0] = 0xff;
		pcidev.devices[devid].headerrom.classcode[1] = 0xff;
		pcidev.devices[devid].headerrom.classcode[2] = 0xff;
		pcidev.devices[devid].headerrom.baseaddrregs[0] = 0xffffffff;
		pcidev.devices[devid].headerrom.baseaddrregs[1] = 0xffffffff;
		pcidev.devices[devid].headerrom.baseaddrregs[2] = 0xffffffff;
		pcidev.devices[devid].headerrom.baseaddrregs[3] = 0xffffffff;
		pcidev.devices[devid].headerrom.baseaddrregs[4] = 0xffffffff;
		pcidev.devices[devid].headerrom.baseaddrregs[5] = 0xffffffff;
	
		// PCI PC-9821�W���f�o�C�Xreset
		pcidev_cbusbridge_reset(pConfig);
		pcidev_98graphbridge_reset(pConfig);
		
		TRACEOUT(("PCI: Peripheral Component Interconnect Enabled"));
	}

	(void)pConfig;
}

static void IOOUTCALL pci_o063c(UINT port, REG8 dat) {

	switch(dat & 0x3) {
		case 0x01:
			if((pcidev.membankd8 & 0x3) != 0x01){
				memcpy(pcidev.biosromtmp, mem + 0x0d8000, 0x8000);
				memcpy(mem + 0x0d8000, pcidev.biosrom, 0x8000);
			}
			break;

		case 0x10:
		case 0x11:
		case 0x00:
		default:
			if((pcidev.membankd8 & 0x3) == 0x01){
				memcpy(pcidev.biosrom, mem + 0x0d8000, 0x8000);
				memcpy(mem + 0x0d8000, pcidev.biosromtmp, 0x8000);
			}
			break;
	}
	pcidev.membankd8 = dat;
	(void)port;
}
static REG8 IOINPCALL pci_i063c(UINT port) {

	return pcidev.membankd8;
}

static void IOOUTCALL pci_o18f0(UINT port, REG8 dat) {

	pcidev.unkreg_bank1 = dat;
	pcidev.unkreg_bank2 = 0;
	(void)port;
}
static REG8 IOINPCALL pci_i18f0(UINT port) {

	return pcidev.unkreg_bank1;
}
static void IOOUTCALL pci_o18f2(UINT port, REG8 dat) {
	
    pcidev.unkreg[(pcidev.unkreg_bank2++) & 3][pcidev.unkreg_bank1] = dat;
	(void)port;
}
static REG8 IOINPCALL pci_i18f2(UINT port) {

	return pcidev.unkreg[(pcidev.unkreg_bank2++) & 3][pcidev.unkreg_bank1];
}
static UINT8 pnp_addr = 0;
static UINT8 pnp_data[0x100] = {0};
static void IOOUTCALL pnp_o259(UINT port, REG8 dat) {
	
    pnp_addr = dat;
	(void)port;
}
static REG8 IOINPCALL pnp_i259(UINT port) {

	return pnp_addr;
}
static void IOOUTCALL pnp_oA59(UINT port, REG8 dat) {
	
    pnp_data[pnp_addr] = dat;
	if(pnp_addr==0)
		mem[0x5B7] = pnp_data[pnp_addr] >> 2;
	(void)port;
}
static REG8 IOINPCALL pnp_iA59(UINT port) {
	
	if(pnp_addr==0)
		pnp_data[pnp_addr] = (mem[0x5B7] << 2) | 0x3;
	return pnp_data[pnp_addr];
}

void pcidev_bind(void) {
	
	UINT	i;

	//for (i=0x0cfc; i<0x0d00; i++) {
	//	iocore_attachout(i, pci_o04);
	//	iocore_attachinp(i, pci_i04);
	//}
	// PCI I/O�|�[�g���蓖��
	iocore_attachout(0xcf8, pci_o0cf8);
	iocore_attachout(0xcf9, pci_o0cf9);
	iocore_attachout(0xcfa, pci_o0cfa);
	iocore_attachout(0xcfb, pci_o0cfb);
	iocore_attachinp(0xcf8, pci_i0cf8);
	iocore_attachinp(0xcf9, pci_i0cf9);
	iocore_attachinp(0xcfa, pci_i0cfa);
	iocore_attachinp(0xcfb, pci_i0cfb);
	
	for (i=0; i<4; i++) {
		iocore_attachout(0xcfc+i, pcidev_w8_0xcfc);
		iocore_attachinp(0xcfc+i, pcidev_r8_0xcfc);
	}
	
	// �o���N�؂�ւ�
	iocore_attachout(0x63c, pci_o063c);
	iocore_attachinp(0x63c, pci_i063c);
	
	iocore_attachout(0x18f0, pci_o18f0);
	iocore_attachout(0x18f2, pci_o18f2);
	iocore_attachinp(0x18f0, pci_i18f0);
	iocore_attachinp(0x18f2, pci_i18f2);
	
	iocore_attachout(0x259, pnp_o259);
	iocore_attachout(0xA59, pnp_oA59);
	iocore_attachinp(0x259, pnp_i259);
	iocore_attachinp(0xA59, pnp_iA59);
	
	if(pcidev.enable){
		// PCI PC-9821�W���f�o�C�Xbind
		pcidev_cbusbridge_bind();
		pcidev_98graphbridge_bind();
	}
	
	pcidev.bios32entrypoint = 0xffff0000;
	pcidev_updateBIOS32data();
}

void pcidev_updateBIOS32data(){
	UINT16 i, j;
	UINT8 checksum;
	UINT8 pcibios_BIOS32SD[16] = {0x5F, 0x33, 0x32, 0x5F, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x01, 0x0, 0x00, 0x00, 0x00, 0x00, 0x00};

	if(pcidev.bios32svcdir == 0) return;

	CopyMemory(pcibios_BIOS32SD + 4, &pcidev.bios32entrypoint, 4);
	ZeroMemory(pcidev.biosdata.data, sizeof(pcidev.biosdata.data));
	
	pcidev.biosdata.datacount = 0;
	pcidev.allirqbitmap = 0;

	for(i=1;i<PCI_DEVICES_MAX;i++){ // 0�͏���
		if(pcidev.devices[i].enable && !pcidev.devices[i].skipirqtbl){
			pcidev.biosdata.data[pcidev.biosdata.datacount].busnumber = 0;
			pcidev.biosdata.data[pcidev.biosdata.datacount].devicenumber = (i << 3);
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intA = (pcidev.devices[i].header.interruptpin == 1 ? pcidev.devices[i].header.interruptline : 0); // ���̃f�o�C�X��INT#A(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B0=�ڑ�����, 1=PIRQ#0, 2=PIRQ#1, 3=PIRQ#2, 4=PIRQ#3, 5�ȏ�:���̑��ڑ��i�����ԍ����m���ڑ��j
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intA = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#A�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�Bbit0=IRQ0, bit1=IRQ1, ... , bit15=IRQ15�ɑΉ� 
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intB = (pcidev.devices[i].header.interruptpin == 2 ? pcidev.devices[i].header.interruptline : 0); // ���̃f�o�C�X��INT#B(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intB = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#B�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�B
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intC = (pcidev.devices[i].header.interruptpin == 3 ? pcidev.devices[i].header.interruptline : 0); // ���̃f�o�C�X��INT#C(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intC = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#C�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�B
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intD = (pcidev.devices[i].header.interruptpin == 4 ? pcidev.devices[i].header.interruptline : 0); // ���̃f�o�C�X��INT#D(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intD = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#D�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�B
			pcidev.biosdata.data[pcidev.biosdata.datacount].slot = pcidev.devices[i].slot;
			pcidev.allirqbitmap |= 
				pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intA | 
				pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intB |
				pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intC |
				pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intD;
			pcidev.biosdata.datacount++;
		}else if(i==13 || i==14 || i==15){
			// PCI�X���b�g
			pcidev.biosdata.data[pcidev.biosdata.datacount].busnumber = 0;
			pcidev.biosdata.data[pcidev.biosdata.datacount].devicenumber = (i << 3);
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intA = ((i-13) & 0x3)+1; // ���̃f�o�C�X��INT#A(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B0=�ڑ�����, 1=PIRQ#0, 2=PIRQ#1, 3=PIRQ#2, 4=PIRQ#3, 5�ȏ�:���̑��ڑ��i�����ԍ����m���ڑ��j
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intA = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#A�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�Bbit0=IRQ0, bit1=IRQ1, ... , bit15=IRQ15�ɑΉ� 
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intB = ((i-13+1) & 0x3)+1; // ���̃f�o�C�X��INT#B(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intB = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#B�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�B
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intC = ((i-13+2) & 0x3)+1; // ���̃f�o�C�X��INT#C(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intC = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#C�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�B
			pcidev.biosdata.data[pcidev.biosdata.datacount].link4intD = ((i-13+3) & 0x3)+1; // ���̃f�o�C�X��INT#D(PCI�d�l��̓f�o�C�X���ɓƗ�)�̐ڑ���B
			pcidev.biosdata.data[pcidev.biosdata.datacount].irqmap4intD = (1<<12)|(1<<6)|(1<<5)|(1<<3); // ���̃f�o�C�X��INT#D�Ŏg�p�ł���IRQ(PIRQ�ł͂Ȃ�)�̃}�b�v�B
			pcidev.biosdata.data[pcidev.biosdata.datacount].slot = (i-13)+1;
			pcidev.biosdata.datacount++;
		}
	}

	//// BIOS32 �d�l�͓�
	//pcibios_BIOS32SD[9] = 1;
	//checksum = 0;
	//pcibios_BIOS32SD[10] = 0;
	//for(j=0;j<16;j++){
	//	checksum += pcibios_BIOS32SD[j];
	//}
	//pcibios_BIOS32SD[10] = (UINT8)(0x100 - checksum);
	//CopyMemory(mem + pcidev.bios32svcdir, pcibios_BIOS32SD, sizeof(pcibios_BIOS32SD));

	memset(pcidev.unkreg, 0, sizeof(pcidev.unkreg));
    pcidev.unkreg_bank1 = pcidev.unkreg_bank2 = 0;
}

#else

// �Ƃ肠���� config #1 type0�Œ�Łc

static void pcidevset10(UINT32 addr, REG8 dat) {

	UINT32	work;

	switch(addr) {
		case 0x000064 + 3:
			pcidev.membankd0 = dat;
			work = CPU_RAM_D000 & 0x03ff;
			if (dat & 0x10) {
				work |= 0x0400;
			}
			if (dat & 0x20) {
				work |= 0x0800;
			}
			if (dat & 0x80) {
				work |= 0xf000;
			}
			CPU_RAM_D000 = (UINT16)work;
			break;
	}
}

static REG8 pcidevget10(UINT32 addr) {

	switch(addr) {
		case 0x000064 + 3:
			return(pcidev.membankd0);
	}
	return(0xff);
}


// ----

static void IOOUTCALL pci_o04(UINT port, REG8 dat) {

	UINT32	addr;

	if (pcidev.base & 0x80000000) {
		addr = pcidev.base & 0x00fffffc;
		addr += port & 3;
		pcidevset10(addr, dat);
	}
}

static REG8 IOINPCALL pci_i04(UINT port) {

	UINT32	addr;

	if (pcidev.base & 0x80000000) {
		addr = pcidev.base & 0x00fffffc;
		addr += port & 3;
		return(pcidevget10(addr));
	}
	else {
		return(0xff);
	}
}

void IOOUTCALL pcidev_w32(UINT port, UINT32 value) {

	UINT32	addr;

	if (!(port & 4)) {
		pcidev.base = value;
	}
	else {
		if (pcidev.base & 0x80000000) {
			addr = pcidev.base & 0x00fffffc;
			pcidevset10(addr + 0, (UINT8)(value >> 0));
			pcidevset10(addr + 1, (UINT8)(value >> 8));
			pcidevset10(addr + 2, (UINT8)(value >> 16));
			pcidevset10(addr + 3, (UINT8)(value >> 24));
		}
	}
}

UINT32 IOOUTCALL pcidev_r32(UINT port) {

	UINT32	ret;
	UINT32	addr;

	ret = (UINT32)-1;
	if (!(port & 4)) {
		ret = pcidev.base;
	}
	else {
		if (pcidev.base & 0x80000000) {
			addr = pcidev.base & 0x00fffffc;
			ret = pcidevget10(addr + 0);
			ret |= (pcidevget10(addr + 1) << 8);
			ret |= (pcidevget10(addr + 2) << 16);
			ret |= (pcidevget10(addr + 3) << 24);
		}
	}
	return(ret);
}

void pcidev_reset(const NP2CFG *pConfig) {

	ZeroMemory(&pcidev, sizeof(pcidev));

	(void)pConfig;
}

void pcidev_bind(void) {

	UINT	i;

	for (i=0x0cfc; i<0x0d00; i++) {
		iocore_attachout(i, pci_o04);
		iocore_attachinp(i, pci_i04);
	}
}
#endif
#endif

