#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"bios/bios.h"
#include	"bios/biosmem.h"
#include	"gdc_sub.h"
#include	"lio.h"
#include	"vram.h"


typedef struct {
	UINT8	mode;
	UINT8	sw;
	UINT8	act;
	UINT8	disp;
} GSCREEN;

typedef struct {
	UINT8	x1[2];
	UINT8	y1[2];
	UINT8	x2[2];
	UINT8	y2[2];
	UINT8	vdraw_bg;
	UINT8	vdraw_ln;
} GVIEW;

typedef struct {
	UINT8	dummy;
	UINT8	bgcolor;
	UINT8	bdcolor;
	UINT8	fgcolor;
	UINT8	palmode;
} GCOLOR1;

typedef struct {
	UINT8	pal;
	UINT8	color1;
	UINT8	color2;
} GCOLOR2;

static void gview_vectl(const _GLIO *lio, int x1, int y1, int x2, int y2, UINT8 pal) {

	UINT32	csrw;
	GDCVECT	vect;

	csrw = (y1 * 40) + (x1 >> 4) + ((x1 & 0xf) << 20);
	if (lio->draw.flag & LIODRAW_UPPER) {
		csrw += 16000 >> 1;
	}
	gdcsub_setvectl(&vect, x1, y1, x2, y2);
	if (!(lio->draw.flag & LIODRAW_MONO)) {
		gdcsub_vectl(csrw + 0x4000, &vect, 0xffff,
						(REG8)((pal & 1)?GDCOPE_SET:GDCOPE_CLEAR));
		gdcsub_vectl(csrw + 0x8000, &vect, 0xffff,
						(REG8)((pal & 2)?GDCOPE_SET:GDCOPE_CLEAR));
		gdcsub_vectl(csrw + 0xc000, &vect, 0xffff,
						(REG8)((pal & 4)?GDCOPE_SET:GDCOPE_CLEAR));
		if (lio->draw.flag & LIODRAW_4BPP) {
			gdcsub_vectl(csrw, &vect, 0xffff,
						(REG8)((pal & 8)?GDCOPE_SET:GDCOPE_CLEAR));
		}
	}
	else {
		csrw += ((lio->draw.flag + 1) & LIODRAW_PMASK) << 12;
		gdcsub_vectl(csrw, &vect, 0xffff,
						(REG8)((pal)?GDCOPE_SET:GDCOPE_CLEAR));
	}
}

static void gview_box(const _GLIO *lio, UINT8 pal) {

	int		y;

	y = lio->draw.y1;
	while(y <= lio->draw.y2) {
		gview_vectl(lio, lio->draw.x1, y, lio->draw.x2, y, pal);
		y++;
	}
}

static void gview_frame(const _GLIO *lio, UINT8 pal) {

	gview_vectl(lio, lio->draw.x1, lio->draw.y1,
				lio->draw.x2, lio->draw.y1, pal);
	gview_vectl(lio, lio->draw.x1, lio->draw.y2,
				lio->draw.x2, lio->draw.y2, pal);
	gview_vectl(lio, lio->draw.x1, lio->draw.y1,
				lio->draw.x1, lio->draw.y2, pal);
	gview_vectl(lio, lio->draw.x2, lio->draw.y1,
				lio->draw.x2, lio->draw.y2, pal);
}

static void lio_reset_graphics(void) {

#if defined(SUPPORT_PC9821) && defined(SUPPORT_CRT31KHZ)
	gdc_analogext(FALSE);				/* clears GDCANALOG_256 and VOPBIT_VGA */
	gdc.analog &= ~(1 << GDCANALOG_256E);
	gdcs.grphdisp |= GDCSCRN_EXT | GDCSCRN_ALLDRAW2;
#endif
}

// ---- INIT

REG8 lio_ginit(GLIO lio) {

	UINT	i;

	vramop.operate &= ~(1 << VOPBIT_ACCESS);
	MEMM_VRAM(vramop.operate);
	lio_reset_graphics();
	bios0x18_42(0x80);
	bios0x18_40();
	iocore_out8(0x006a, 0);
	gdc_paletteinit();

	ZeroMemory(&lio->work, sizeof(lio->work));
//	lio->work.scrnmode = 0;
//	lio->work.pos = 0;
	lio->work.plane = 1;
//	lio->work.bgcolor = 0;
	lio->work.fgcolor = 7;
	for (i=0; i<8; i++) {
		lio->work.color[i] = (UINT8)i;
	}
//	STOREINTELWORD(lio->work.viewx1, 0);
//	STOREINTELWORD(lio->work.viewy1, 0);
	STOREINTELWORD(lio->work.viewx2, 639);
	STOREINTELWORD(lio->work.viewy2, 399);
	lio->palmode = 0;
	MEMR_WRITES(CPU_DS, 0x0620, &lio->work, sizeof(lio->work));
	MEMR_WRITE8(CPU_DS, 0x0a08, lio->palmode);
	return(LIO_SUCCESS);
}


// ---- SCREEN

REG8 lio_gscreen(GLIO lio) {

	GSCREEN	dat;
	UINT	colorbit;
	UINT8	oldscrnmode;
	UINT8	scrnmode;
	UINT8	mono;
	UINT8	act;
	UINT8	pos;
	UINT8	disp;
	UINT8	plane;
	UINT8	planemax;
	UINT8	mode;
	UINT8	upperbit;
	UINT8	planemask;

	if (lio->palmode != 2) {
		colorbit = 3;
	}
	else {
		colorbit = 4;
	}
	upperbit = (UINT8)(1 << colorbit);
	planemask = (UINT8)(upperbit - 1);

	MEMR_READS(CPU_DS, CPU_BX, &dat, sizeof(dat));

	oldscrnmode = lio->work.scrnmode;
	scrnmode = dat.mode;
	if (scrnmode == 0xff) {
		scrnmode = oldscrnmode;
	}
	if (scrnmode >= 4) {
		goto gscreen_err5;
	}

	// –³Œø‚È’l‚ÍR‚é
	if ((dat.sw != 0xff) && (dat.sw >= 4)) {
		goto gscreen_err5;
	}

	pos = lio->work.pos;
	act = lio->work.access;
	if (dat.act == 0xff) {
		if (scrnmode != oldscrnmode) {
			pos = 0;
			act = 0;
		}
	}
	else {
		act = dat.act;
		switch(scrnmode) {
			case 0:
				pos = act & 1;
				act >>= 1;
				break;

			case 1:
				pos = act % (colorbit * 2);
				act = act / (colorbit * 2);
				break;

			case 2:
				pos = act % colorbit;
				act = act / colorbit;
				break;

			case 3:
			default:
				pos = 0;
				break;
		}
		if (act >= 2) {
			goto gscreen_err5;
		}
	}

	plane = lio->work.plane;
	disp = lio->work.disp;
	if (dat.disp == 0xff) {
		if (scrnmode != oldscrnmode) {
			plane = 1;
			disp = 0;
		}
	}
	else {
		disp = dat.disp;
		plane = disp & ((2 << colorbit) - 1);
		disp >>= (colorbit + 1);
		if (disp >= 2) {
			goto gscreen_err5;
		}
		planemax = 1;
		mono = (UINT8)((scrnmode + 1) >> 1) & 1;
		if (mono) {
			planemax <<= colorbit;
		}
		if (!(scrnmode & 2)) {
			planemax <<= 1;
		}
		if ((plane > planemax) && (plane != upperbit)) {
			goto gscreen_err5;
		}
	}

	if (dat.sw != 0xff) {
		if (!(dat.sw & 2)) {
			bios0x18_40();
		}
		else {
			bios0x18_41();
		}
	}

	lio->work.scrnmode = scrnmode;
	lio->work.pos = pos;
	lio->work.access = act;
	lio->work.plane = plane;
	lio->work.disp = disp;

	STOREINTELWORD(lio->work.viewx1, 0);
	STOREINTELWORD(lio->work.viewy1, 0);
	STOREINTELWORD(lio->work.viewx2, 639);
	STOREINTELWORD(lio->work.viewy2, (scrnmode & 2) ? 399 : 199);

	if ((plane == 0) || (plane == upperbit)) {
		mode = 0x00;
	}
	else {
		switch(scrnmode) {
			case 0:
				mode = (plane == 2) ? 0x40 : 0x80;
				break;

			case 1:
				mode = (plane & upperbit) ? 0x60 : 0xa0;
				break;

			case 2:
				mode = (plane & planemask) ? 0xe0 : 0x00;
				break;

			case 3:
			default:
				mode = (plane & 1) ? 0xc0 : 0x00;
				break;
		}
	}
	if (((scrnmode + 1) >> 1) & 1) {
		mode |= 0x20;
	}
	mode |= disp << 4;
	lio_reset_graphics();
	bios0x18_42(mode);
	iocore_out8(0x00a6, lio->work.access);
	MEMR_WRITES(CPU_DS, 0x0620, &lio->work, sizeof(lio->work));
	return(LIO_SUCCESS);

gscreen_err5:
	TRACEOUT(("screen error! %d %d %d %d",
								dat.mode, dat.sw, dat.act, dat.disp));
	return(LIO_ILLEGALFUNC);
}


// ---- VIEW

REG8 lio_gview(GLIO lio) {

	GVIEW	dat;
	int		x1;
	int		y1;
	int		x2;
	int		y2;

	MEMR_READS(CPU_DS, CPU_BX, &dat, sizeof(dat));
	x1 = (SINT16)LOADINTELWORD(dat.x1);
	y1 = (SINT16)LOADINTELWORD(dat.y1);
	x2 = (SINT16)LOADINTELWORD(dat.x2);
	y2 = (SINT16)LOADINTELWORD(dat.y2);
	if ((x1 >= x2) || (y1 >= y2)) {
		return(LIO_ILLEGALFUNC);
	}
	lio_updatedraw(lio);
	if ((dat.vdraw_bg != 0xff) && (dat.vdraw_bg >= lio->draw.palmax)) {
		return(LIO_ILLEGALFUNC);
	}
	if ((dat.vdraw_ln != 0xff) && (dat.vdraw_ln >= lio->draw.palmax)) {
		return(LIO_ILLEGALFUNC);
	}
	STOREINTELWORD(lio->work.viewx1, (UINT16)x1);
	STOREINTELWORD(lio->work.viewy1, (UINT16)y1);
	STOREINTELWORD(lio->work.viewx2, (UINT16)x2);
	STOREINTELWORD(lio->work.viewy2, (UINT16)y2);
	MEMR_WRITES(CPU_DS, 0x0620, &lio->work, sizeof(lio->work));

	if ((dat.vdraw_bg != 0xff) || (dat.vdraw_ln != 0xff)) {
		lio_updatedraw(lio);
		if (dat.vdraw_bg != 0xff) {
			gview_box(lio, dat.vdraw_bg);
		}
		if (dat.vdraw_ln != 0xff) {
			gview_frame(lio, dat.vdraw_ln);
		}
	}
	return(LIO_SUCCESS);
}


// ---- COLOR1

REG8 lio_gcolor1(GLIO lio) {

	GCOLOR1	dat;

	MEMR_READS(CPU_DS, CPU_BX, &dat, sizeof(dat));
	if (dat.bgcolor != 0xff) {
		lio->work.bgcolor = dat.bgcolor;
	}
	if (dat.fgcolor != 0xff) {
		lio->work.fgcolor = dat.fgcolor;
	}
	if (dat.palmode != 0xff) {
		if (!(mem[MEMB_PRXCRT] & 1)) {				// 8color lio
			dat.palmode = 0;
		}
		else {
			if (!(mem[MEMB_PRXCRT] & 4)) {			// have e-plane?
				goto gcolor1_err5;
			}
			if (!dat.palmode) {
				iocore_out8(0x006a, 0);
			}
			else {
				iocore_out8(0x006a, 1);
			}
		}
		lio->palmode = dat.palmode;
	}
	MEMR_WRITES(CPU_DS, 0x0620, &lio->work, sizeof(lio->work));
	MEMR_WRITE8(CPU_DS, 0x0a08, lio->palmode);
	return(LIO_SUCCESS);

gcolor1_err5:
	return(LIO_ILLEGALFUNC);
}


// ---- COLOR2

REG8 lio_gcolor2(GLIO lio) {

	GCOLOR2	dat;

	MEMR_READS(CPU_DS, CPU_BX, &dat, sizeof(dat));
	if (dat.pal >= ((lio->palmode == 2)?16:8)) {
		goto gcolor2_err5;
	}
	if (!lio->palmode) {
		if ((lio->work.scrnmode == 1) || (lio->work.scrnmode == 2)) {
			dat.color1 = (dat.color1 & 1) ? 7 : 0;
			lio->work.color[dat.pal] = dat.color1;
			gdc_setdegitalpal(dat.pal, dat.color1);
		}
		else {
			dat.color1 &= 7;
			lio->work.color[dat.pal] = dat.color1;
			gdc_setdegitalpal(dat.pal, dat.color1);
		}
	}
	else {
		gdc_setanalogpal(dat.pal, offsetof(RGB32, p.b),
												(UINT8)(dat.color1 & 0x0f));
		gdc_setanalogpal(dat.pal, offsetof(RGB32, p.r),
												(UINT8)(dat.color1 >> 4));
		gdc_setanalogpal(dat.pal, offsetof(RGB32, p.g),
												(UINT8)(dat.color2 & 0x0f));
	}
	MEMR_WRITES(CPU_DS, 0x0620, &lio->work, sizeof(lio->work));
	return(LIO_SUCCESS);

gcolor2_err5:
	return(LIO_ILLEGALFUNC);
}

