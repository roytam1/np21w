#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"gdc_sub.h"
#include	"lio.h"


typedef struct {
	UINT8	x1[2];
	UINT8	y1[2];
	UINT8	x2[2];
	UINT8	y2[2];
	UINT8	pal;
	UINT8	type;
	UINT8	sw;
	UINT8	style[2];
	UINT8	patleng;
	UINT8	off[2];
	UINT8	seg[2];
} GLINE;

typedef struct {
	int		x1;
	int		y1;
	int		x2;
	int		y2;
	UINT8	pal;
} LINEPT;

static UINT gettileplanes(const _GLIO *lio) {

	if (lio->draw.flag & LIODRAW_MONO) {
		return(1);
	}
	return((lio->draw.flag & LIODRAW_4BPP)?4:3);
}

static BOOL checktileleng(const _GLIO *lio, UINT leng) {

	UINT planes;

	if (leng == 0) {
		return(FALSE);
	}
	planes = gettileplanes(lio);
	return((leng >= planes) && ((leng % planes) == 0));
}



static int divfloor_int(int num, int den) {

	int		q;
	int		r;

	q = num / den;
	r = num % den;
	if ((r != 0) && ((r < 0) != (den < 0))) {
		q--;
	}
	return(q);
}

static UINT clipcode(const _GLIO *lio, int x, int y) {

	UINT	code;

	code = 0;
	if (x < lio->draw.x1) {
		code |= 1;
	}
	else if (x > lio->draw.x2) {
		code |= 2;
	}
	if (y < lio->draw.y1) {
		code |= 4;
	}
	else if (y > lio->draw.y2) {
		code |= 8;
	}
	return(code);
}

static BOOL clipline_directional_floor(const _GLIO *lio, LINEPT *lp) {

	int		x1;
	int		y1;
	int		x2;
	int		y2;
	UINT	code1;
	UINT	code2;
	UINT	code;
	int		dx;
	int		dy;
	int		i;

	x1 = lp->x1;
	y1 = lp->y1;
	x2 = lp->x2;
	y2 = lp->y2;
	for (i=0; i<16; i++) {
		code1 = clipcode(lio, x1, y1);
		code2 = clipcode(lio, x2, y2);
		if (!(code1 | code2)) {
			lp->x1 = x1;
			lp->y1 = y1;
			lp->x2 = x2;
			lp->y2 = y2;
			return(TRUE);
		}
		if (code1 & code2) {
			return(FALSE);
		}
		if (code1) {
			code = code1;
			dx = x2 - x1;
			dy = y2 - y1;
			if (code & 1) {
				if (!dx) {
					return(FALSE);
				}
				y1 += divfloor_int(dy * (lio->draw.x1 - x1), dx);
				x1 = lio->draw.x1;
			}
			else if (code & 2) {
				if (!dx) {
					return(FALSE);
				}
				y1 += divfloor_int(dy * (lio->draw.x2 - x1), dx);
				x1 = lio->draw.x2;
			}
			else if (code & 4) {
				if (!dy) {
					return(FALSE);
				}
				x1 += divfloor_int(dx * (lio->draw.y1 - y1), dy);
				y1 = lio->draw.y1;
			}
			else {
				if (!dy) {
					return(FALSE);
				}
				x1 += divfloor_int(dx * (lio->draw.y2 - y1), dy);
				y1 = lio->draw.y2;
			}
		}
		else {
			code = code2;
			dx = x1 - x2;
			dy = y1 - y2;
			if (code & 1) {
				if (!dx) {
					return(FALSE);
				}
				y2 += divfloor_int(dy * (lio->draw.x1 - x2), dx);
				x2 = lio->draw.x1;
			}
			else if (code & 2) {
				if (!dx) {
					return(FALSE);
				}
				y2 += divfloor_int(dy * (lio->draw.x2 - x2), dx);
				x2 = lio->draw.x2;
			}
			else if (code & 4) {
				if (!dy) {
					return(FALSE);
				}
				x2 += divfloor_int(dx * (lio->draw.y1 - y2), dy);
				y2 = lio->draw.y1;
			}
			else {
				if (!dy) {
					return(FALSE);
				}
				x2 += divfloor_int(dx * (lio->draw.y2 - y2), dy);
				y2 = lio->draw.y2;
			}
		}
	}
	return(FALSE);
}

static void gline(const _GLIO *lio, const LINEPT *lp, UINT16 pat) {

	int		x1;
	int		y1;
	int		x2;
	int		y2;
	int		swap;
	int		tmp;
	int		width;
	int		height;
	int		d1;
	int		d2;
	UINT32	csrw;
	GDCVECT	vect;

	x1 = lp->x1;
	y1 = lp->y1;
	x2 = lp->x2;
	y2 = lp->y2;

	// びゅーぽいんと
	swap = 0;
	if (x1 > x2) {
		LINEPT clp;
		clp = *lp;
		if (!clipline_directional_floor(lio, &clp)) {
			return;
		}
		x1 = clp.x1;
		y1 = clp.y1;
		x2 = clp.x2;
		y2 = clp.y2;
		goto gline_clipped;
	}
	if ((x1 > lio->draw.x2) || (x2 < lio->draw.x1)) {
		return;
	}
	width = x2 - x1;
	height = y2 - y1;
	d1 = lio->draw.x1 - x1;
	d2 = x2 - lio->draw.x2;
	if (d1 > 0) {
		x1 = lio->draw.x1;
		y1 += (((height * d1 * 2) / width) + 1) >> 1;
	}
	if (d2 > 0) {
		x2 = lio->draw.x2;
		y2 -= (((height * d2 * 2) / width) + 1) >> 1;
	}
	if (swap) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	swap = 0;
	if (y1 > y2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}
	if ((y1 > lio->draw.y2) || (y2 < lio->draw.y1)) {
		return;
	}
	width = x2 - x1;
	height = y2 - y1;
	d1 = lio->draw.y1 - y1;
	d2 = y2 - lio->draw.y2;
	if (d1 > 0) {
		y1 = lio->draw.y1;
		x1 += (((width * d1 * 2) / height) + 1) >> 1;
	}
	if (d2 > 0) {
		y2 = lio->draw.y2;
		x2 -= (((width * d2 * 2) / height) + 1) >> 1;
	}
	if (swap) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	// 進んだ距離計算
gline_clipped:
	d1 = x1 - lp->x1;
	if (d1 < 0) {
		d1 = 0 - d1;
	}
	d2 = y1 - lp->y1;
	if (d2 < 0) {
		d2 = 0 - d2;
	}
	d1 = max(d1, d2) & 15;
	pat = (UINT16)((pat >> d1) | (pat << (16 - d1)));

	csrw = (y1 * 40) + (x1 >> 4) + ((x1 & 0xf) << 20);
	if (lio->draw.flag & LIODRAW_UPPER) {
		csrw += 16000 >> 1;
	}
	gdcsub_setvectl(&vect, x1, y1, x2, y2);
	if (!(lio->draw.flag & LIODRAW_MONO)) {
		gdcsub_vectl(csrw + 0x4000, &vect, pat,
							(REG8)((lp->pal & 1)?GDCOPE_SET:GDCOPE_CLEAR));
		gdcsub_vectl(csrw + 0x8000, &vect, pat,
							(REG8)((lp->pal & 2)?GDCOPE_SET:GDCOPE_CLEAR));
		gdcsub_vectl(csrw + 0xc000, &vect, pat,
							(REG8)((lp->pal & 4)?GDCOPE_SET:GDCOPE_CLEAR));
		if (lio->draw.flag & LIODRAW_4BPP) {
			gdcsub_vectl(csrw, &vect, pat,
							(REG8)((lp->pal & 8)?GDCOPE_SET:GDCOPE_CLEAR));
		}
	}
	else {
		csrw += ((lio->draw.flag + 1) & LIODRAW_PMASK) << 12;
		gdcsub_vectl(csrw, &vect, pat,
								(REG8)((lp->pal)?GDCOPE_SET:GDCOPE_CLEAR));
	}
}

static void glineb(const _GLIO *lio, const LINEPT *lp, UINT16 pat) {

	LINEPT	lpt;

	lpt = *lp;
	lpt.y2 = lp->y1;
	gline(lio, &lpt, pat);
	lpt.y2 = lp->y2;

	lpt.x2 = lp->x1;
	gline(lio, &lpt, pat);
	lpt.x2 = lp->x2;

	lpt.x1 = lp->x2;
	gline(lio, &lpt, pat);
	lpt.x1 = lp->x1;

	lpt.y1 = lp->y2;
	gline(lio, &lpt, pat);
	lpt.y1 = lp->y1;
}


// ----

static void gbox(const _GLIO *lio, const LINEPT *lp, UINT8 *tile, UINT leng) {

	int		x1;
	int		y1;
	int		x2;
	int		y2;
	int		tmp;
	UINT32	csrw;
	GDCVECT	vect;
	UINT	planes;
	UINT	adrs[4];
	UINT8	ope[4];
	UINT16	pat;
	UINT8	*tterm;
	UINT	r;

	x1 = lp->x1;
	y1 = lp->y1;
	x2 = lp->x2;
	y2 = lp->y2;

	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}
	if ((x1 > lio->draw.x2) || (x2 < lio->draw.x1) ||
		(y1 > lio->draw.y2) || (y2 < lio->draw.y1)) {
		return;
	}
	x1 = max(x1, lio->draw.x1);
	y1 = max(y1, lio->draw.y1);
	x2 = min(x2, lio->draw.x2);
	y2 = min(y2, lio->draw.y2);

	csrw = 0;
	if (lio->draw.flag & LIODRAW_UPPER) {
		csrw += 16000 >> 1;
	}
	if (!(lio->draw.flag & LIODRAW_MONO)) {
		planes = (lio->draw.flag & LIODRAW_4BPP)?4:3;
		adrs[0] = csrw + 0x4000;
		adrs[1] = csrw + 0x8000;
		adrs[2] = csrw + 0xc000;
		adrs[3] = csrw + 0x0000;
		ope[0] = (lp->pal & 1)?GDCOPE_SET:GDCOPE_CLEAR;
		ope[1] = (lp->pal & 2)?GDCOPE_SET:GDCOPE_CLEAR;
		ope[2] = (lp->pal & 4)?GDCOPE_SET:GDCOPE_CLEAR;
		ope[3] = (lp->pal & 8)?GDCOPE_SET:GDCOPE_CLEAR;
	}
	else {
		planes = 1;
		adrs[0] = csrw + (((lio->draw.flag + 1) & LIODRAW_PMASK) << 12);
		ope[0] = (lp->pal)?GDCOPE_SET:GDCOPE_CLEAR;
	}

	if (leng == 0) {
		tile = NULL;
		tterm = NULL;
	}
	else {
		tterm = tile + leng;
		tmp = (x1 - lio->draw.x1) & 7;
		do {
			r = GDCPATREVERSE(*tile);
			*tile = (UINT8)((r << tmp) | (r >> (8 - tmp)));
		} while(++tile < tterm);
		tile -= leng;
		tmp = (y1 - lio->draw.y1) * planes;
		tile += tmp % leng;
	}

	pat = 0xffff;
	while(y1 <= y2) {
		gdcsub_setvectl(&vect, x1, y1, x2, y1);
		csrw = (y1 * 40) + (x1 >> 4) + ((x1 & 0xf) << 20);
		r = 0;
		do {
			if (tile) {
				pat = (*tile << 8) | *tile;
				if (++tile >= tterm) {
					tile -= leng;
				}
			}
			gdcsub_vectl(csrw + adrs[r], &vect, pat, ope[r]);
		} while(++r < planes);
		y1++;
	}
}


// ---- CLS

REG8 lio_gcls(GLIO lio) {

	LINEPT	lp;

	lio_updatedraw(lio);
	lp.x1 = lio->draw.x1;
	lp.y1 = lio->draw.y1;
	lp.x2 = lio->draw.x2;
	lp.y2 = lio->draw.y2;
	lp.pal = lio->work.bgcolor;
	gbox(lio, &lp, NULL, 0);
	return(LIO_SUCCESS);
}


// ----

REG8 lio_gline(GLIO lio) {

	GLINE	dat;
	LINEPT	lp;
	UINT16	pat;
	UINT	leng;
//	UINT	lengmin;
	UINT8	tile[256];

	lio_updatedraw(lio);
	MEMR_READS(CPU_DS, CPU_BX, &dat, sizeof(dat));
	lp.x1 = (SINT16)LOADINTELWORD(dat.x1);
	lp.y1 = (SINT16)LOADINTELWORD(dat.y1);
	lp.x2 = (SINT16)LOADINTELWORD(dat.x2);
	lp.y2 = (SINT16)LOADINTELWORD(dat.y2);

	TRACEOUT(("lio_gline %d,%d-%d,%d [%d]", lp.x1, lp.y1, lp.x2, lp.y2, dat.type));

	if (dat.pal == 0xff) {
		dat.pal = lio->work.fgcolor;
	}
	if ((dat.pal >= lio->draw.palmax) || (dat.sw > 2)) {
		goto gline_err;
	}
	pat = 0xffff;
	if (dat.type < 2) {
		/* sw=2（タイルパターン）は塗りつぶしの時のみ有効 */
		if (dat.sw == 2) {
			goto gline_err;
		}
		if (dat.sw == 1) {
			pat = (GDCPATREVERSE(dat.style[0]) << 8) +
											GDCPATREVERSE(dat.style[1]);
		}
		lp.pal = dat.pal;
		if (dat.type == 0) {
			gline(lio, &lp, pat);
		}
		else {
			glineb(lio, &lp, pat);
		}
	}
	else if (dat.type == 2) {
		leng = 0;
		if (dat.sw == 2) {
			leng = dat.patleng;
			if (!checktileleng(lio, leng)) {
				goto gline_err;
			}
			MEMR_READS(LOADINTELWORD(dat.seg), LOADINTELWORD(dat.off),
												tile, leng);
		}
		if (dat.sw != 1) {
			lp.pal = dat.pal;
			gbox(lio, &lp, tile, leng);
		}
		else {
			if (dat.style[0] == 0xff) {
				dat.style[0] = lio->work.fgcolor;
				if (dat.style[0] >= lio->draw.palmax) {
					dat.style[0] &= (UINT8)(lio->draw.palmax - 1);
				}
				lp.pal = dat.style[0];
				gbox(lio, &lp, tile, leng);
			}
			else {
				if (dat.style[0] >= lio->draw.palmax) {
					goto gline_err;
				}
				lp.pal = dat.style[0];
				gbox(lio, &lp, tile, leng);
				lp.pal = dat.pal;
				glineb(lio, &lp, 0xffff);
			}
		}
	}
	else {
		goto gline_err;
	}
	return(LIO_SUCCESS);

gline_err:
	return(LIO_ILLEGALFUNC);
}

