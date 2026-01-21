#include	"compiler.h"
#include	"commng.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"


	COMMNG	cm_prt;

	static REG8 lastData = 0xff;
	static REG8 i44Data = 0x00;
	
// ---- I/O

static void IOOUTCALL prt_o40(UINT port, REG8 dat) {

	COMMNG	prt;

	prt = cm_prt;
	if (prt == NULL) {
		prt = commng_create(COMCREATE_PRINTER, FALSE);
		cm_prt = prt;
	}
	prt->write(prt, (UINT8)dat);
	lastData = dat;
//	TRACEOUT(("prt - %.2x", dat));
	(void)port;
}
static void IOOUTCALL prt_o44(UINT port, REG8 dat) {

	i44Data = dat;
	(void)port;
}

static REG8 IOINPCALL prt_i40(UINT port) {

	(void)port;
	return(lastData);
}
static REG8 IOINPCALL prt_i42(UINT port) {

	REG8	ret;

	ret = 0x84;
	if (pccore.cpumode & CPUMODE_8MHZ) {
		ret |= 0x20;
	}
	if (pccore.dipsw[0] & 4) {
		ret |= 0x10;
	}
	if (pccore.dipsw[0] & 0x80) {
		ret |= 0x08;
	}
	if (!(pccore.model & PCMODEL_EPSON)) {
		if (CPU_TYPE & CPUTYPE_V30) {
			ret |= 0x02;
		}
	}
	else {
		if (pccore.dipsw[2] & 0x80) {
			ret |= 0x02;
		}
	}
	(void)port;
	return(ret);
}
static REG8 IOINPCALL prt_i44(UINT port) {

	(void)port;
	return(i44Data);
}


// ---- I/F

static const IOOUT prto40[4] = {
					prt_o40,	NULL, prt_o44,	 NULL};

static const IOINP prti40[4] = {
					prt_i40, prt_i42, prt_i44,	 NULL};

void printif_reset(const NP2CFG *pConfig) {

	commng_destroy(cm_prt);
	cm_prt = NULL;

	(void)pConfig;
}

void printif_bind(void) {

	iocore_attachsysoutex(0x0040, 0x0cf1, prto40, 4);
	iocore_attachsysinpex(0x0040, 0x0cf1, prti40, 4);
}

void printif_finalize(void) {

	commng_destroy(cm_prt);
	cm_prt = NULL;
}

// Finish current print job
void printif_finishjob(void) {

	if (cm_prt) {
		cm_prt->msg(cm_prt, COMMSG_REOPEN, NULL);
	}
}

