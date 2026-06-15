/**
 * @file	ga1280a.c
 * @brief	Implementation of the I-O DATA GA-1280A
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_GA1280A)

#include	"pccore.h"
#include	"wab.h"
#include	"statsave.h"
#include	"dosio.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"soundmng.h"

#if defined(SUPPORT_IA32_HAXM)
#include "i386hax/haxfunc.h"
#include "i386hax/haxcore.h"
#endif

#include	"ga1280adef.h"
#include	"ga1280a.h"

#if 0
#undef	TRACEOUT
static void trace_fmt_ex(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUT(s)	trace_fmt_ex s
#endif

GA1280A		ga1280a;

static UINT8 GA1280A_IDSTREAM[] = "DATA DEVICE I.O ";

static void IOOUTCALL ga1280a_ob(UINT port, REG8 dat)
{
	ga1280a.reg.b[port >> 8][port & 3] = dat;
}
static REG8 IOINPCALL ga1280a_ib(UINT port)
{
	switch (port) {
	case 0x1dd9:
	{
		ga1280a.reg.b[port >> 8][port & 3] = (ga1280a.reg.b[port >> 8][port & 3] + 1) % (sizeof(GA1280A_IDSTREAM) - 1);
		return GA1280A_IDSTREAM[ga1280a.reg.b[port >> 8][port & 3]];
	}
	}
	return ga1280a.reg.b[port >> 8][port & 3];
}
void IOOUTCALL ga1280a_ow(UINT port, UINT16 dat)
{
	ga1280a.reg.w[port >> 8][(port >> 1) & 1] = dat;
}
UINT16 IOINPCALL ga1280a_iw(UINT port)
{
	return ga1280a.reg.w[port >> 8][(port >> 1) & 1];
}

// PC-98êÍóp MMIOèàóùóp
int MEMCALL ga1280a_memp_read8(UINT32 address, REG8 *lpRetValue)
{
	return 0;
}
int MEMCALL ga1280a_memp_read16(UINT32 address, REG16 *lpRetValue)
{
	return 0;
}
int MEMCALL ga1280a_memp_read32(UINT32 address, UINT32 *lpRetValue)
{
	return 0;
}
int MEMCALL ga1280a_memp_write8(UINT32 address, REG8 value)
{
	return 0;
}
int MEMCALL ga1280a_memp_write16(UINT32 address, REG16 value)
{
	return 0;
}
int MEMCALL ga1280a_memp_write32(UINT32 address, UINT32 value)
{
	return 0;
}

int ga1280a_drawGraphic(void) 
{
	UINT32 updated;
	UINT32 paletteUpdated;
	HDC hdc = np2wabwnd.hDCBuf;

	updated = ga1280a.updated;
	paletteUpdated = ga1280a.paletteUpdated;
	ga1280a.updated = 0;
	ga1280a.paletteUpdated = 0;

	if (!updated && !paletteUpdated) return 0;

	np2wab.realWidth = ga1280a.width;
	np2wab.realHeight = ga1280a.height;

	return 1;
}

void ga1280a_reset(const NP2CFG* pConfig)
{
	memset(&ga1280a, 0, sizeof(ga1280a));
	ga1280a.enabled = 1;
	ga1280a.width = 640;
	ga1280a.height = 480;
}
void ga1280a_bind(void)
{
	if (ga1280a.enabled) {
		for(int i=0;i<0x20;i++){
			for(int j=0xd8;j<=0xdb;j++){
				iocore_attachout(j | (i << 8), ga1280a_ob);
				iocore_attachinp(j | (i << 8), ga1280a_ib);
			}
		}
	}
}
void ga1280a_unbind(void)
{
	for(int i=0;i<0x20;i++){
		for(int j=0xd8;j<=0xdb;j++){
			iocore_detachout(j | (i << 8));
			iocore_detachinp(j | (i << 8));
		}
	}
}

void ga1280a_shutdown()
{
	
}

#endif