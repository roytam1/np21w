#include	"compiler.h"
#include	"cpucore.h"
#include	"lio.h"

#define GCIRCLE_F_START       0x01
#define GCIRCLE_F_STARTLINE   0x02
#define GCIRCLE_F_END         0x04
#define GCIRCLE_F_ENDLINE     0x08
#define GCIRCLE_F_SAMEPOINT   0x10
#define GCIRCLE_F_FILL        0x20
#define GCIRCLE_F_TILE        0x40
#define GCIRCLE_F_RESERVED    0x80

typedef struct {
	UINT8	cx[2];
	UINT8	cy[2];
	UINT8	rx[2];
	UINT8	ry[2];
	UINT8	pal;
	UINT8	flag;
	UINT8	sx[2];
	UINT8	sy[2];
	UINT8	ex[2];
	UINT8	ey[2];
	UINT8	pat;
	UINT8	off[2];
	UINT8	seg[2];
} GCIRCLE;

typedef struct {
	long	x;
	long	y;
} LIOVEC;


// ---- ヘルパー

static int gc_posmod(int v, int m) {

	int		r;

	r = v % m;
	if (r < 0) {
		r += m;
	}
	return(r);
}

static int gc_ellipse_inside(SINT16 cx, SINT16 cy, SINT16 rx, SINT16 ry,
														SINT16 x, SINT16 y) {

	double	dx;
	double	dy;
	double	rxd;
	double	ryd;
	double	v;
	double	limit;

	if ((rx == 0) && (ry == 0)) {
		return((x == cx) && (y == cy));
	}
	if (rx == 0) {
		return((x == cx) && (y >= (cy - ry)) && (y <= (cy + ry)));
	}
	if (ry == 0) {
		return((y == cy) && (x >= (cx - rx)) && (x <= (cx + rx)));
	}
	dx = (double)x - (double)cx;
	dy = (double)y - (double)cy;
	rxd = (double)rx;
	ryd = (double)ry;
	v = (dx * dx * ryd * ryd) + (dy * dy * rxd * rxd);
	limit = rxd * rxd * ryd * ryd;
	return(v <= (limit + 0.0001));
}

static int gc_ellipse_border(SINT16 cx, SINT16 cy, SINT16 rx, SINT16 ry,
														SINT16 x, SINT16 y) {

	if (!gc_ellipse_inside(cx, cy, rx, ry, x, y)) {
		return(0);
	}
	return(!gc_ellipse_inside(cx, cy, rx, ry, (SINT16)(x - 1), y) ||
			!gc_ellipse_inside(cx, cy, rx, ry, (SINT16)(x + 1), y) ||
			!gc_ellipse_inside(cx, cy, rx, ry, x, (SINT16)(y - 1)) ||
			!gc_ellipse_inside(cx, cy, rx, ry, x, (SINT16)(y + 1)));
}

// 座標系は数学と同じ右上が正
static LIOVEC gc_vec_from_point(SINT16 cx, SINT16 cy, SINT16 x, SINT16 y) {

	LIOVEC	v;

	v.x = (long)x - (long)cx;
	v.y = (long)cy - (long)y;
	if ((v.x == 0) && (v.y == 0)) {
		v.x = 1;
	}
	return(v);
}

static int gc_half(const LIOVEC *v) {

	return((v->y < 0) || ((v->y == 0) && (v->x < 0)));
}

static int gc_anglecmp(const LIOVEC *a, const LIOVEC *b) {

	int		ha;
	int		hb;
	double	cr;

	ha = gc_half(a);
	hb = gc_half(b);
	if (ha != hb) {
		return(ha - hb);
	}
	cr = ((double)a->x * (double)b->y) - ((double)a->y * (double)b->x);
	if (cr > 0) {
		return(-1);
	}
	if (cr < 0) {
		return(1);
	}
	return(0);
}

static int gc_angle_le(const LIOVEC *a, const LIOVEC *b) {

	return(gc_anglecmp(a, b) <= 0);
}

static int gc_same_dir(const LIOVEC *a, const LIOVEC *b) {

	double	cr;
	double	dot;

	cr = ((double)a->x * (double)b->y) - ((double)a->y * (double)b->x);
	dot = ((double)a->x * (double)b->x) + ((double)a->y * (double)b->y);
	return((cr == 0) && (dot > 0));
}

static int gc_arc_contains(SINT16 cx, SINT16 cy, SINT16 x, SINT16 y,
										const LIOVEC *sv, const LIOVEC *ev,
										int usearc, int fullarc, int onepoint) {

	LIOVEC	pv;
	int		se;

	if (!usearc || fullarc) {
		return(1);
	}
	if ((x == cx) && (y == cy)) {
		return(!onepoint);
	}
	pv = gc_vec_from_point(cx, cy, x, y);
	if (onepoint) {
		return(gc_same_dir(sv, &pv));
	}
	se = gc_anglecmp(sv, ev);
	if (se <= 0) {
		return(gc_angle_le(sv, &pv) && gc_angle_le(&pv, ev));
	}
	return(gc_angle_le(sv, &pv) || gc_angle_le(&pv, ev));
}

static void gc_drawline(const _GLIO *lio, SINT16 x1, SINT16 y1,
										SINT16 x2, SINT16 y2, REG8 pal) {

	int		dx;
	int		dy;
	int		sx;
	int		sy;
	int		err;
	int		e2;

	dx = x2 - x1;
	if (dx < 0) {
		dx = 0 - dx;
	}
	dy = y2 - y1;
	if (dy < 0) {
		dy = 0 - dy;
	}
	sx = (x1 < x2) ? 1 : -1;
	sy = (y1 < y2) ? 1 : -1;
	err = dx - dy;
	for (;;) {
		lio_pset(lio, x1, y1, pal);
		if ((x1 == x2) && (y1 == y2)) {
			break;
		}
		e2 = err << 1;
		if (e2 > -dy) {
			err -= dy;
			x1 = (SINT16)(x1 + sx);
		}
		if (e2 < dx) {
			err += dx;
			y1 = (SINT16)(y1 + sy);
		}
	}
}

static REG8 gc_tilepal(const _GLIO *lio, const GCIRCLE *dat,
								SINT16 x, SINT16 y, UINT planes) {

	UINT	pl;
	UINT	idx;
	UINT	seg;
	UINT	off;
	UINT8	bit;
	REG8	pal;

	seg = LOADINTELWORD(dat->seg);
	off = LOADINTELWORD(dat->off);
	bit = (UINT8)(0x80 >> gc_posmod((int)x - (int)lio->draw.x1, 8));
	pal = 0;
	for (pl=0; pl<planes; pl++) {
		idx = (UINT)gc_posmod(((int)y - (int)lio->draw.y1) * (int)planes +
														(int)pl, dat->pat);
		if (MEMR_READ8(seg, off + idx) & bit) {
			pal |= (REG8)(1 << pl);
		}
	}
	return(pal);
}

static REG8 gc_fillpal(const _GLIO *lio, const GCIRCLE *dat,
								SINT16 x, SINT16 y, REG8 arcpal) {

	UINT	planes;

	if (!(dat->flag & GCIRCLE_F_TILE)) {
		if (dat->pat == 0xff) {
			return( arcpal );
		}
		return(dat->pat);
	}
	planes = 1;
	if (!(lio->draw.flag & LIODRAW_MONO)) {
		planes = (lio->draw.flag & LIODRAW_4BPP) ? 4 : 3;
	}
	return(gc_tilepal(lio, dat, x, y, planes));
}


// ---- GCIRCLE

REG8 lio_gcircle(GLIO lio) {

	GCIRCLE	dat;
	SINT16	cx;
	SINT16	cy;
	SINT16	rx;
	SINT16	ry;
	SINT16	sx;
	SINT16	sy;
	SINT16	ex;
	SINT16	ey;
	SINT16	x;
	SINT16	y;
	REG8	pal;
	REG8	fpal;
	LIOVEC	sv;
	LIOVEC	ev;
	int		usearc;
	int		fullarc;
	int		onepoint;
	int		fill;
	int		planes;
	UINT32	waitcnt;

	lio_updatedraw(lio);
	MEMR_READS(CPU_DS, CPU_BX, &dat, sizeof(dat));

	if (dat.flag & GCIRCLE_F_RESERVED) {
		goto gcircle_err;
	}
	cx = (SINT16)LOADINTELWORD(dat.cx);
	cy = (SINT16)LOADINTELWORD(dat.cy);
	rx = (SINT16)LOADINTELWORD(dat.rx);
	ry = (SINT16)LOADINTELWORD(dat.ry);
	if ((rx < 0) || (ry < 0)) {
		goto gcircle_err;
	}
	pal = dat.pal;
	if (pal == 0xff) {
		pal = lio->work.fgcolor;
	}
	if (pal >= lio->draw.palmax) {
		goto gcircle_err;
	}
	if ((dat.flag & GCIRCLE_F_FILL) && !(dat.flag & GCIRCLE_F_TILE) &&
		(dat.pat != 0xff) && (dat.pat >= lio->draw.palmax)) {
		goto gcircle_err;
	}
	if ((dat.flag & GCIRCLE_F_FILL) && (dat.flag & GCIRCLE_F_TILE)) {
		planes = 1;
		if (!(lio->draw.flag & LIODRAW_MONO)) {
			planes = (lio->draw.flag & LIODRAW_4BPP) ? 4 : 3;
		}
		if (dat.pat < planes) {
			goto gcircle_err;
		}
	}

	sx = (dat.flag & GCIRCLE_F_START) ?
			(SINT16)LOADINTELWORD(dat.sx) : (SINT16)(cx + rx);
	sy = (dat.flag & GCIRCLE_F_START) ?
			(SINT16)LOADINTELWORD(dat.sy) : cy;
	ex = (dat.flag & GCIRCLE_F_END) ?
			(SINT16)LOADINTELWORD(dat.ex) : (SINT16)(cx + rx);
	ey = (dat.flag & GCIRCLE_F_END) ?
			(SINT16)LOADINTELWORD(dat.ey) : cy;
	sv = gc_vec_from_point(cx, cy, sx, sy);
	ev = gc_vec_from_point(cx, cy, ex, ey);
	usearc = (dat.flag & (GCIRCLE_F_START | GCIRCLE_F_END)) != 0;
	onepoint = 0;
	fullarc = !usearc;
	if (usearc && gc_same_dir(&sv, &ev)) {
		if (dat.flag & GCIRCLE_F_SAMEPOINT) {
			onepoint = 1;
			fullarc = 0;
		}
		else {
			fullarc = 1;
		}
	}

	waitcnt = 0;
	fill = (dat.flag & GCIRCLE_F_FILL) && !onepoint;
	if (fill) {
		for (y=(SINT16)(cy - ry); y<=(SINT16)(cy + ry); y++) {
			for (x=(SINT16)(cx - rx); x<=(SINT16)(cx + rx); x++) {
				if (gc_ellipse_inside(cx, cy, rx, ry, x, y) &&
					gc_arc_contains(cx, cy, x, y, &sv, &ev, usearc,
														fullarc, onepoint)) {
					fpal = gc_fillpal(lio, &dat, x, y, pal);
					lio_pset(lio, x, y, fpal);
					waitcnt++;
				}
			}
		}
	}

	if (onepoint) {
		lio_pset(lio, sx, sy, pal);
		waitcnt++;
	}
	else {
		for (y=(SINT16)(cy - ry); y<=(SINT16)(cy + ry); y++) {
			for (x=(SINT16)(cx - rx); x<=(SINT16)(cx + rx); x++) {
				if (gc_ellipse_border(cx, cy, rx, ry, x, y) &&
					gc_arc_contains(cx, cy, x, y, &sv, &ev, usearc,
														fullarc, onepoint)) {
					lio_pset(lio, x, y, pal);
					waitcnt++;
				}
			}
		}
	}

	if ((dat.flag & GCIRCLE_F_STARTLINE) && !fullarc) {
		gc_drawline(lio, cx, cy, sx, sy, pal);
	}
	if ((dat.flag & GCIRCLE_F_ENDLINE) && !fullarc) {
		gc_drawline(lio, cx, cy, ex, ey, pal);
	}
	lio->wait += waitcnt * (10 + 10 + 10);
	return(LIO_SUCCESS);

 gcircle_err:
	TRACEOUT(("LIO GCIRCLE illegal %.2x", dat.flag));
	return(LIO_ILLEGALFUNC);
}
