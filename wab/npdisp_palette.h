/**
 * @file	npdisp_palette.h
 * @brief	Interface of the Neko Project II Display Adapter Palette
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP)

 // モノクロで白扱いする閾値
#define NPDISP_MONO_THRESHOLD	350
#define NPDISP_MONO_THRESHOLD_N	350

// パレットカラー指定 or RGB指定を調整
#define NPDISP_ADJUST_COLORREF(a)	(((a) & 0xff000000) == 0x01000000 ? (a) : ((a) & 0xffffff))

// モノクロの場合RGB指定を白黒へ変換
#define NPDISP_ADJUST_MONOCOLOR_R(a)	((a) & 0xff)
#define NPDISP_ADJUST_MONOCOLOR_G(a)	(((a) >> 8) & 0xff)
#define NPDISP_ADJUST_MONOCOLOR_B(a)	(((a) >> 16) & 0xff)
#define NPDISP_ADJUST_MONOCOLOR_SUMRGB(a)	( NPDISP_ADJUST_MONOCOLOR_R(a) + NPDISP_ADJUST_MONOCOLOR_G(a) + NPDISP_ADJUST_MONOCOLOR_B(a) )
#define NPDISP_ADJUST_MONOCOLOR_TOMONO(a)	( (NPDISP_ADJUST_MONOCOLOR_SUMRGB(a) > NPDISP_MONO_THRESHOLD) ? 0xffffff : 0 )
#define NPDISP_ADJUST_MONOCOLOR_PAL(a)		( (a)==1 ? 0xffffff : 0x000000 )
#define NPDISP_ADJUST_MONOCOLOR(a)			(npdisp.bpp == 1 ? ( (((a) & 0xff000000) == 0x01000000) ? NPDISP_ADJUST_MONOCOLOR_PAL(a) : NPDISP_ADJUST_MONOCOLOR_TOMONO(a) ) : (a))

typedef struct {
	UINT8 r;
	UINT8 g;
	UINT8 b;
} NPDISP_RGB3;

extern NPDISP_RGB3 npdisp_palette_rgb2[2];
extern NPDISP_RGB3 npdisp_palette_rgb16[16];
extern NPDISP_RGB3 npdisp_palette_rgb256[256];

#ifdef __cplusplus
extern "C" {
#endif
	void npdisp_palette_makeTable();

	int npdisp_FindNearest2(UINT8 r, UINT8 g, UINT8 b);
	int npdisp_FindNearest16(UINT8 r, UINT8 g, UINT8 b);
	int npdisp_FindNearest256(UINT8 r, UINT8 g, UINT8 b);

	UINT32 npdisp_FindNearestColor(UINT8 r, UINT8 g, UINT8 b);
	UINT32 npdisp_FindNearestColorUINT32(UINT32 color);

	UINT32 npdisp_ObjIdxToColor(int idx);

	HBRUSH CreatePaletteDitherBrush(UINT32 target);
#ifdef __cplusplus
}
#endif

#endif