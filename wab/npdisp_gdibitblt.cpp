/**
 * @file	npdisp_gdioutput.c
 * @brief	Implementation of the Neko Project II Display Adapter GDI BitBlt Functions
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>

#include	"pccore.h"
#include	"cpucore.h"

#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_mem.h"
#include	"npdisp_palette.h"
#include	"npdisp_gdibitblt.h"

//#define IMAGEDEBUG
//#define IMAGEDEBUG_SIZE	32
//#define IMAGEDEBUG_X	0

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
#if 1
#define	TRACEOUT_BITBLT(s)	TRACEOUT(s)
#else
#define	TRACEOUT_BITBLT(s)	(void)s
#endif
#else
#define	TRACEOUT_BITBLT(s)	(void)s
#endif	/* 1 */
#if 0
static void trace_fmt_ex2(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUT2(s)	trace_fmt_ex2 s
#else
#define	TRACEOUT2(s)	(void)s
#endif	/* 1 */

extern NPDISP_WINDOWS	npdispwin;

// StretchBltと共用にする

UINT16 npdisp_func_StretchBlt_VRAMtoVRAM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, SINT16 wDestXext, SINT16 wDestYext, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, SINT16 wSrcXext, SINT16 wSrcYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipAddr)
{
	// VRAM -> VRAM
	bool isStretch = wDestXext != wSrcXext || wDestYext != wSrcYext;
	if (isStretch) {
		TRACEOUT_BITBLT(("Stretchlt VRAM -> VRAM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
		if (npdisp.bpp == 1) return 0xffff; //モノクロソースの時はCOLORONCOLOR以外だと致命的なのでGDIにやらせる
	}
	else {
		TRACEOUT_BITBLT(("BitBlt VRAM -> VRAM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
	}
	HRGN hRgn = NULL;
	if (lpClipAddr) {
		RECT cliprect = { 0 };
		NPDISP_RECT rectTmp = { 0 };
		npdisp_readMemory(&rectTmp, lpClipAddr, sizeof(NPDISP_RECT));
		cliprect.top = rectTmp.top;
		cliprect.left = rectTmp.left;
		cliprect.bottom = rectTmp.bottom;
		cliprect.right = rectTmp.right;
		hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
	}
	NPDISP_DRAWMODE drawMode = { 0 };
	int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
	if (hasDrawMode) {
		npdisp_AdjustDrawModeColor(&drawMode);
		SetBkColor(npdispwin.hdc, drawMode.LbkColor);
		SetTextColor(npdispwin.hdc, drawMode.LTextColor);
		SetBkMode(npdispwin.hdc, drawMode.bkMode);
		SetROP2(npdispwin.hdc, drawMode.Rop2);
	}
	else {
		SetBkColor(npdispwin.hdc, 0xffffff);
		SetTextColor(npdispwin.hdc, 0x000000);
	}
	if (lpPBrushAddr) {
		// ブラシがあれば選択
		NPDISP_BRUSH brush = { 0 };
		if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
			if (brush.key != 0) {
				auto it = npdispwin.brushes.find(brush.key);
				if (it != npdispwin.brushes.end()) {
					NPDISP_HOSTBRUSH value = it->second;
					if (value.brs) {
						TRACEOUT_BITBLT(("-> style=%d, hatch=%d, color=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor));
						SelectObject(npdispwin.hdc, value.brs);
						if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
							SetBkColor(npdispwin.hdc, npdisp_AdjustColorRefForGDI(brush.lbrush.lbBkColor));
						}
					}
					else {
						SelectObject(npdispwin.hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
					}
				}
			}
		}
	}
	int srcx = wSrcX;
	int srcy = wSrcY;
	int destx = wDestX;
	int desty = wDestY;
	int srcw = wSrcXext;
	int srch = wSrcYext;
	int destw = wDestXext;
	int desth = wDestYext;
	if(hRgn) SelectClipRgn(npdispwin.hdc, hRgn);
	if (!(srcx + srcw < destx || destx + srcw < srcx || srcy + srch < desty || desty + desth < srcy)) {
		// 重なっているのでバッファ経由
		BitBlt(npdispwin.hdcBltBuf, wSrcX, wSrcY, wSrcXext, wSrcYext, npdispwin.hdc, wSrcX, wSrcY, SRCCOPY);
		if (isStretch) {
			SetStretchBltMode(npdispwin.hdc, COLORONCOLOR);
			StretchBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, npdispwin.hdcBltBuf, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3);
		}
		else {
			BitBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, npdispwin.hdcBltBuf, wSrcX, wSrcY, Rop3);
		}
	}
	else {
		// 重なっていないので直接転送
		if (isStretch) {
			SetStretchBltMode(npdispwin.hdc, COLORONCOLOR);
			StretchBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, npdispwin.hdc, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3);
		}
		else {
			BitBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, npdispwin.hdc, wSrcX, wSrcY, Rop3);
		}
	}
	if (hRgn) SelectClipRgn(npdispwin.hdc, NULL);
	SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
	npdisp.updated = 1;

	if (hRgn) {
		DeleteObject(hRgn);
	}

	return 1;
}
UINT16 npdisp_func_BitBlt_VRAMtoVRAM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, UINT16 wXext, UINT16 wYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr)
{
	return npdisp_func_StretchBlt_VRAMtoVRAM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wXext, wYext, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr, 0);
}
UINT16 npdisp_func_StretchBlt_MEMtoVRAM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, SINT16 wDestXext, SINT16 wDestYext, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, SINT16 wSrcXext, SINT16 wSrcYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipAddr)
{
	// MEM -> VRAM
	UINT16 retValue = 1;
	
	// 実際に転送する範囲を計算
	int srcBeginLine = wSrcY;
	int srcNumLines = wSrcYext;
	int dstBeginLine = wDestY;
	int dstNumLines = wDestYext;
	if (srcBeginLine < 0) {
		srcNumLines += srcBeginLine;
		srcBeginLine = 0;
	}
	if (dstBeginLine < 0) {
		dstNumLines += dstBeginLine;
		dstBeginLine = 0;
	}
	int srcBeginX = wSrcX;
	int srcCopyWidth = wSrcXext;
	int dstBeginX = wDestX;
	int dstCopyWidth = wDestXext;
	if (srcBeginX < 0) {
		srcCopyWidth += srcBeginX;
		srcBeginX = 0;
	}
	if (dstBeginX < 0) {
		dstCopyWidth += dstBeginX;
		dstBeginX = 0;
	}

	bool isStretch = wDestXext != wSrcXext || wDestYext != wSrcYext;
	if (isStretch) {
		TRACEOUT_BITBLT(("Stretchlt MEM -> VRAM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
	}
	else {
		TRACEOUT_BITBLT(("BitBlt MEM -> VRAM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
	}
	HRGN hRgn = NULL;
	if (lpClipAddr) {
		RECT cliprect = { 0 };
		NPDISP_RECT rectTmp = { 0 };
		npdisp_readMemory(&rectTmp, lpClipAddr, sizeof(NPDISP_RECT));
		cliprect.top = rectTmp.top;
		cliprect.left = rectTmp.left;
		cliprect.bottom = rectTmp.bottom;
		cliprect.right = rectTmp.right;
		hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
	}
	NPDISP_PBITMAP srcPBmp;
	if (lpSrcDevAddr && npdisp_readMemory(&srcPBmp, lpSrcDevAddr, sizeof(NPDISP_PBITMAP))) {
		if (!isStretch || srcPBmp.bmBitsPixel != 1) {
			NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
			npdisp_PreloadBitmapFromPBITMAP(&srcPBmp, 0, srcBeginLine, srcNumLines, srcBeginX, srcCopyWidth);
			if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&srcPBmp, &bmphdc, 0, srcBeginLine, srcNumLines, srcBeginX, srcCopyWidth, npdisp_palette_transTbl)) {
				npdisp_ConvertToDDBMonoBitmap(&bmphdc);
				NPDISP_DRAWMODE drawMode = { 0 };
				int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
				if (hasDrawMode) {
					npdisp_AdjustDrawModeColor(&drawMode);
					SetBkColor(npdispwin.hdc, drawMode.LbkColor);
					SetTextColor(npdispwin.hdc, drawMode.LTextColor);
					SetBkMode(npdispwin.hdc, drawMode.bkMode);
					SetROP2(npdispwin.hdc, drawMode.Rop2);
					// ソースにもセットが必要
					SetBkColor(bmphdc.hdc, drawMode.LbkColor);
					SetTextColor(bmphdc.hdc, drawMode.LTextColor);
					SetBkMode(bmphdc.hdc, drawMode.bkMode);
					SetROP2(bmphdc.hdc, drawMode.Rop2);
					//npdisp_AdjustSrcMonoPaletteByDrawMode(&bmphdc, NULL, &drawMode);
				}
				else {
					SetBkColor(npdispwin.hdc, 0xffffff);
					SetTextColor(npdispwin.hdc, 0x000000);
					SetBkColor(bmphdc.hdc, 0xffffff);
					SetTextColor(bmphdc.hdc, 0x000000);
				}
				if (lpPBrushAddr) {
					// ブラシがあれば選択
					NPDISP_BRUSH brush = { 0 };
					if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
						if (brush.key != 0) {
							auto it = npdispwin.brushes.find(brush.key);
							if (it != npdispwin.brushes.end()) {
								NPDISP_HOSTBRUSH value = it->second;
								if (value.brs) {
									TRACEOUT_BITBLT(("-> style=%d, hatch=%d, color=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor));
									SelectObject(npdispwin.hdc, value.brs);
									if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
										SetBkColor(npdispwin.hdc, npdisp_AdjustColorRefForGDI(brush.lbrush.lbBkColor));
									}
								}
								else {
									SelectObject(npdispwin.hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
								}
							}
						}
					}
				}
				if (hRgn) SelectClipRgn(npdispwin.hdc, hRgn);
				HDC srcHDC = bmphdc.hdc;
				if ((bmphdc.lpbi->bmiHeader.biBitCount == 16 || bmphdc.lpbi->bmiHeader.biBitCount == 15) && npdisp.bpp == 1 && npdispwin.hdc16BltBuf) {
					BitBlt(npdispwin.hdc16BltBuf, wSrcX, wSrcY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, SRCCOPY);
					srcHDC = npdispwin.hdc16BltBuf;
				}
				if (isStretch) {
					SetStretchBltMode(npdispwin.hdc, COLORONCOLOR);
					StretchBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3);
				}
				else {
					BitBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, Rop3);
				}
				if (hRgn) SelectClipRgn(npdispwin.hdc, NULL);

				SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
				npdisp.updated = 1;

				npdisp_FreeBitmap(&bmphdc);
			}
		}
		else {
			retValue = 0xffff; //モノクロソースの時はCOLORONCOLOR以外だと致命的なのでGDIにやらせる
		}
	}
	else if (lpPBrushAddr) {
		NPDISP_BRUSH brush = { 0 };
		TRACEOUT_BITBLT(("-> BRUSH"));
		if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
			if (brush.key != 0) {
				auto it = npdispwin.brushes.find(brush.key);
				if (it != npdispwin.brushes.end()) {
					NPDISP_HOSTBRUSH value = it->second;
					if (value.brs) {
						TRACEOUT_BITBLT(("-> style=%d, hatch=%d, color=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor));
						NPDISP_DRAWMODE drawMode = { 0 };
						int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
						SelectObject(npdispwin.hdc, value.brs);
						if (hasDrawMode) {
							npdisp_AdjustDrawModeColor(&drawMode);
							SetBkColor(npdispwin.hdc, drawMode.LbkColor);
							SetTextColor(npdispwin.hdc, drawMode.LTextColor);
							if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
							}
							SetBkMode(npdispwin.hdc, drawMode.bkMode);
							SetROP2(npdispwin.hdc, drawMode.Rop2);
						}
						else {
							SetBkColor(npdispwin.hdc, 0xffffff);
							SetTextColor(npdispwin.hdc, 0x000000);
						}
						if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
							SetBkColor(npdispwin.hdc, npdisp_AdjustColorRefForGDI(brush.lbrush.lbBkColor));
						}
						if (hRgn) SelectClipRgn(npdispwin.hdc, hRgn);
						PatBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, Rop3);
						//BitBlt(npdispwin.hdc, wDestX, wDestY, wDestXext, wDestYext, npdispwin.hdc, wDestX, wDestY, Rop3);
						if (hRgn) SelectClipRgn(npdispwin.hdc, NULL);
						npdisp.updated = 1;

						SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
					}
					else {
						SelectObject(npdispwin.hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
					}
				}
			}
		}
	}

	if (hRgn) {
		DeleteObject(hRgn);
	}

	return retValue;
}
UINT16 npdisp_func_BitBlt_MEMtoVRAM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, UINT16 wXext, UINT16 wYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr)
{
	return npdisp_func_StretchBlt_MEMtoVRAM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wXext, wYext, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr, 0);
}
UINT16 npdisp_func_StretchBlt_VRAMtoMEM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, SINT16 wDestXext, SINT16 wDestYext, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, SINT16 wSrcXext, SINT16 wSrcYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipAddr)
{
	// VRAM -> MEM

	// 実際に転送する範囲を計算
	int srcBeginLine = wSrcY;
	int srcNumLines = wSrcYext;
	int dstBeginLine = wDestY;
	int dstNumLines = wDestYext;
	if (srcBeginLine < 0) {
		srcNumLines += srcBeginLine;
		srcBeginLine = 0;
	}
	if (dstBeginLine < 0) {
		dstNumLines += dstBeginLine;
		dstBeginLine = 0;
	}
	int srcBeginX = wSrcX;
	int srcCopyWidth = wSrcXext;
	int dstBeginX = wDestX;
	int dstCopyWidth = wDestXext;
	if (srcBeginX < 0) {
		srcCopyWidth += srcBeginX;
		srcBeginX = 0;
	}
	if (dstBeginX < 0) {
		dstCopyWidth += dstBeginX;
		dstBeginX = 0;
	}

	bool isStretch = wDestXext != wSrcXext || wDestYext != wSrcYext;
	if (isStretch) {
		TRACEOUT_BITBLT(("Stretchlt VRAM -> MEM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
		if (npdisp.bpp == 1) return 0xffff; //モノクロソースの時はCOLORONCOLOR以外だと致命的なのでGDIにやらせる
	}
	else {
		TRACEOUT_BITBLT(("BitBlt VRAM -> MEM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
	}
	HRGN hRgn = NULL;
	if (lpClipAddr) {
		RECT cliprect = { 0 };
		NPDISP_RECT rectTmp = { 0 };
		npdisp_readMemory(&rectTmp, lpClipAddr, sizeof(NPDISP_RECT));
		cliprect.top = rectTmp.top;
		cliprect.left = rectTmp.left;
		cliprect.bottom = rectTmp.bottom;
		cliprect.right = rectTmp.right;
		hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
	}
	NPDISP_PBITMAP dstPBmp;
	if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
		NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
		npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth);
		if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth)) {
			npdisp_ConvertToDDBMonoBitmap(&bmphdc);
			NPDISP_DRAWMODE drawMode = { 0 };
			int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
			if (hasDrawMode) {
				npdisp_AdjustDrawModeColor(&drawMode);
				SetBkColor(bmphdc.hdc, drawMode.LbkColor);
				SetTextColor(bmphdc.hdc, drawMode.LTextColor);
				SetBkMode(bmphdc.hdc, drawMode.bkMode);
				SetROP2(bmphdc.hdc, drawMode.Rop2);
				// ソースにもセットが必要
				SetBkColor(npdispwin.hdc, drawMode.LbkColor);
				SetTextColor(npdispwin.hdc, drawMode.LTextColor);
				SetBkMode(npdispwin.hdc, drawMode.bkMode);
				SetROP2(npdispwin.hdc, drawMode.Rop2);
			}
			else {
				SetBkColor(bmphdc.hdc, 0xffffff);
				SetTextColor(bmphdc.hdc, 0x000000);
				SetBkColor(npdispwin.hdc, 0xffffff);
				SetTextColor(npdispwin.hdc, 0x000000);
			}
			if (lpPBrushAddr) {
				// ブラシがあれば選択
				NPDISP_BRUSH brush = { 0 };
				if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
					if (brush.key != 0) {
						auto it = npdispwin.brushes.find(brush.key);
						if (it != npdispwin.brushes.end()) {
							NPDISP_HOSTBRUSH value = it->second;
							if (value.brs) {
								TRACEOUT_BITBLT(("-> style=%d, hatch=%d, color=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor));
								//if (npdisp.bpp == 1) {
								//	drawMode.LbkColor = drawMode.bkColor ? 0xffffff : 0;
								//	drawMode.LTextColor = drawMode.TextColor ? 0xffffff : 0;
								//	if (Rop3 == PATCOPY) {
								//		if (drawMode.LbkColor) {
								//			SelectObject(bmphdc.hdc, GetStockObject(WHITE_BRUSH));
								//		}
								//		else {
								//			SelectObject(bmphdc.hdc, GetStockObject(BLACK_BRUSH));
								//		}
								//	}
								//}
								SelectObject(bmphdc.hdc, value.brs);
								//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
								//	SetBkColor(bmphdc.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
								//}
								//if (hasDrawMode) {
								//	SetBkColor(bmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
								//	SetTextColor(bmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
								//	if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
								//	}
								//	SetBkMode(bmphdc.hdc, drawMode.bkMode);
								//	SetROP2(bmphdc.hdc, drawMode.Rop2);
								//}
							}
							else {
								SelectObject(bmphdc.hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
							}
						}
					}
				}
			}

			if (hRgn) SelectClipRgn(bmphdc.hdc, hRgn);
			HDC srcHDC = npdispwin.hdc;
			//if ((npdisp.bpp == 16 || npdisp.bpp == 15) && bmphdc.lpbi->bmiHeader.biBitCount == 1 && npdispwin.hdc16BltBuf) {
			//	BitBlt(npdispwin.hdc16BltBuf, wSrcX, wSrcY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, SRCCOPY);
			//	srcHDC = npdispwin.hdc16BltBuf;
			//}
			if (isStretch) {
				SetStretchBltMode(bmphdc.hdc, COLORONCOLOR);
				StretchBlt(bmphdc.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3);
			}
			else {
				BitBlt(bmphdc.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, Rop3);
			}
			if (hRgn) SelectClipRgn(bmphdc.hdc, NULL);

			SelectObject(npdispwin.hdc, npdispwin.hOldBrush);

			npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth);

			npdisp_FreeBitmap(&bmphdc);
		}
	}

	if (hRgn) {
		DeleteObject(hRgn);
	}

	return 1;
}
UINT16 npdisp_func_BitBlt_VRAMtoMEM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, UINT16 wXext, UINT16 wYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr)
{
	return npdisp_func_StretchBlt_VRAMtoMEM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wXext, wYext, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr, 0);
}
UINT16 npdisp_func_StretchBlt_MEMtoMEM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, SINT16 wDestXext, SINT16 wDestYext, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, SINT16 wSrcXext, SINT16 wSrcYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipAddr)
{
	// MEM -> MEM
	UINT16 retValue = 0;

	// 実際に転送する範囲を計算
	int srcBeginLine = wSrcY;
	int srcNumLines = wSrcYext;
	int dstBeginLine = wDestY;
	int dstNumLines = wDestYext;
	if (srcBeginLine < 0) {
		srcNumLines += srcBeginLine;
		srcBeginLine = 0;
	}
	if (dstBeginLine < 0) {
		dstNumLines += dstBeginLine;
		dstBeginLine = 0;
	}
	int srcBeginX = wSrcX;
	int srcCopyWidth = wSrcXext;
	int dstBeginX = wDestX;
	int dstCopyWidth = wDestXext;
	if (srcBeginX < 0) {
		srcCopyWidth += srcBeginX;
		srcBeginX = 0;
	}
	if (dstBeginX < 0) {
		dstCopyWidth += dstBeginX;
		dstBeginX = 0;
	}

	bool isStretch = wDestXext != wSrcXext || wDestYext != wSrcYext;
	if (isStretch) {
		TRACEOUT_BITBLT(("Stretchlt MEM -> MEM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
	}
	else {
		TRACEOUT_BITBLT(("BitBlt MEM -> MEM DEST X:%d Y:%d W:%d H:%d, rop:%08x", wDestX, wDestY, wDestXext, wDestYext, Rop3));
	}
	HRGN hRgn = NULL;
	if (lpClipAddr) {
		RECT cliprect = { 0 };
		NPDISP_RECT rectTmp = { 0 };
		npdisp_readMemory(&rectTmp, lpClipAddr, sizeof(NPDISP_RECT));
		cliprect.top = rectTmp.top;
		cliprect.left = rectTmp.left;
		cliprect.bottom = rectTmp.bottom;
		cliprect.right = rectTmp.right;
		hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
	}
	NPDISP_PBITMAP dstPBmp;
	retValue = 1;
	if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
		if (lpSrcDevAddr) {
			NPDISP_PBITMAP srcPBmp;
			if (npdisp_readMemory(&srcPBmp, lpSrcDevAddr, sizeof(NPDISP_PBITMAP))) {
				if (!isStretch || srcPBmp.bmBitsPixel != 1) {
					npdisp_PreloadBitmapFromPBITMAP(&srcPBmp, 0, srcBeginLine, srcNumLines, srcBeginX, srcCopyWidth);
					npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 1, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth);
					NPDISP_WINDOWS_BMPHDC srcbmphdc = { 0 };
					if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&srcPBmp, &srcbmphdc, 0, srcBeginLine, srcNumLines, srcBeginX, srcCopyWidth)) {
						NPDISP_WINDOWS_BMPHDC dstbmphdc = { 0 };
						if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &dstbmphdc, 1, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth)) {
							npdisp_ConvertToDDBMonoBitmap(&srcbmphdc);
							npdisp_ConvertToDDBMonoBitmap(&dstbmphdc);
							HDC srcHDC = srcbmphdc.hdc;
							NPDISP_DRAWMODE drawMode = { 0 };
							int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
							if (hasDrawMode) {
								bool use24buf = false;
								if ((srcbmphdc.lpbi->bmiHeader.biBitCount == 16 || srcbmphdc.lpbi->bmiHeader.biBitCount == 15) && dstbmphdc.lpbi->bmiHeader.biBitCount == 1 && npdispwin.hdc16BltBuf && drawMode.LbkColor == 0xffffff) {
									// XXX: ペイントブラシの色指定消しゴム専用
									use24buf = true;
									srcHDC = npdispwin.hdc16BltBuf;
								}
								npdisp_AdjustDrawModeColor(&drawMode, use24buf);
								if (use24buf) {
									BitBlt(srcHDC, wSrcX, wSrcY, wDestXext, wDestYext, srcbmphdc.hdc, wSrcX, wSrcY, SRCCOPY);
								}
							}
							if (hasDrawMode) {
								SetBkColor(dstbmphdc.hdc, drawMode.LbkColor);
								SetTextColor(dstbmphdc.hdc, drawMode.LTextColor);
								SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
								SetROP2(dstbmphdc.hdc, drawMode.Rop2);
								// ソースにもセットが必要
								SetBkColor(srcHDC, drawMode.LbkColor);
								SetTextColor(srcHDC, drawMode.LTextColor);
								SetBkMode(srcHDC, drawMode.bkMode);
								SetROP2(srcHDC, drawMode.Rop2);
								//npdisp_AdjustSrcMonoPaletteByDrawMode(&srcbmphdc, &dstbmphdc, &drawMode);
							}
							else {
								SetBkColor(dstbmphdc.hdc, 0xffffff);
								SetTextColor(dstbmphdc.hdc, 0x000000);
								SetBkColor(srcHDC, 0xffffff);
								SetTextColor(srcHDC, 0x000000);
							}
							if (lpPBrushAddr) {
								// ブラシがあれば選択
								NPDISP_BRUSH brush = { 0 };
								if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
									if (brush.key != 0) {
										auto it = npdispwin.brushes.find(brush.key);
										if (it != npdispwin.brushes.end()) {
											NPDISP_HOSTBRUSH value = it->second;
											if (value.brs) {
												TRACEOUT_BITBLT(("-> style=%d, hatch=%d, color=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor));
												//if (npdisp.bpp == 1) {
												//	drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
												//	drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
												//	if (Rop3 == PATCOPY) {
												//		if (drawMode.LbkColor) {
												//			SelectObject(dstbmphdc.hdc, GetStockObject(WHITE_BRUSH));
												//		}
												//		else {
												//			SelectObject(dstbmphdc.hdc, GetStockObject(BLACK_BRUSH));
												//		}
												//	}
												//}
												SelectObject(dstbmphdc.hdc, value.brs);
												//if (dstbmphdc.lpbi->bmiHeader.biBitCount == 1 && srcbmphdc.lpbi->bmiHeader.biBitCount > 8) {
												//	SetTextColor(dstbmphdc.hdc, 0xffffff);
												//}
												//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
												//	SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
												//}
												//if (hasDrawMode) {
												//	SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
												//	SetTextColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
												//	if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
												//	}
												//	SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
												//	SetROP2(dstbmphdc.hdc, drawMode.Rop2);
												//}
											}
											else {
												SelectObject(dstbmphdc.hdc, (HBRUSH)GetStockObject(NULL_BRUSH));
											}
										}
									}
								}
							}

							if (hRgn) SelectClipRgn(dstbmphdc.hdc, hRgn);
							if (isStretch) {
								SetStretchBltMode(dstbmphdc.hdc, COLORONCOLOR);
								StretchBlt(dstbmphdc.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3);
							}
							else {
#ifdef IMAGEDEBUG
								if (wDestXext == IMAGEDEBUG_SIZE && wDestYext == IMAGEDEBUG_SIZE) {
									static int yyyy = 0;
									BitBlt(npdispwin.hdc, IMAGEDEBUG_X +0, yyyy, wDestXext, wDestYext, dstbmphdc.hdc, wDestX, wDestY, SRCCOPY);
									BitBlt(npdispwin.hdc, IMAGEDEBUG_X + IMAGEDEBUG_SIZE, yyyy, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, SRCCOPY);
									BitBlt(dstbmphdc.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, Rop3);
									SetBkColor(npdispwin.hdc, 0xffffff);
									SetTextColor(npdispwin.hdc, 0x000000);
									BitBlt(npdispwin.hdc, IMAGEDEBUG_X + IMAGEDEBUG_SIZE * 2, yyyy, wDestXext, wDestYext, dstbmphdc.hdc, wDestX, wDestY, SRCCOPY);

									yyyy += IMAGEDEBUG_SIZE;
									if (yyyy > npdisp.height) yyyy = 0;
								}
								else
#endif
								{
									BitBlt(dstbmphdc.hdc, wDestX, wDestY, wDestXext, wDestYext, srcHDC, wSrcX, wSrcY, Rop3);
								}
							}
							if (hRgn) SelectClipRgn(dstbmphdc.hdc, NULL);

							SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
							retValue = 1; // 成功

							npdisp_WriteBitmapToPBITMAP(&dstPBmp, &dstbmphdc, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth);

							npdisp_FreeBitmap(&dstbmphdc);
						}
						npdisp_FreeBitmap(&srcbmphdc);
					}
				}
				else {
					retValue = 0xffff; //モノクロソースの時はCOLORONCOLOR以外だと致命的なのでGDIにやらせる
				}
			}
		}
		else if (lpPBrushAddr)
		{
			TRACEOUT_BITBLT(("-> BRUSH"));
			NPDISP_BRUSH brush = { 0 };
			if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
				if (brush.key != 0) {
					auto it = npdispwin.brushes.find(brush.key);
					if (it != npdispwin.brushes.end()) {
						NPDISP_HOSTBRUSH value = it->second;
						if (value.brs) {
							NPDISP_WINDOWS_BMPHDC dstbmphdc = { 0 };
							npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth);
							if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &dstbmphdc, 0, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth)) {
								TRACEOUT_BITBLT(("-> style=%d, hatch=%d, color=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor));
								NPDISP_DRAWMODE drawMode = { 0 };
								int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
								if (hasDrawMode) {
									npdisp_AdjustDrawModeColor(&drawMode);
									SetBkColor(dstbmphdc.hdc, drawMode.LbkColor);
									SetTextColor(dstbmphdc.hdc, drawMode.LTextColor);
									if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
									}
									SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
									SetROP2(dstbmphdc.hdc, drawMode.Rop2);
								}
								else {
									SetBkColor(dstbmphdc.hdc, 0xffffff);
									SetTextColor(dstbmphdc.hdc, 0x000000);
								}
								if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
									SetBkColor(dstbmphdc.hdc, npdisp_AdjustColorRefForGDI(brush.lbrush.lbBkColor));
								}

								//if (npdisp.bpp == 1) {
								//	drawMode.LbkColor = drawMode.bkColor ? 0xffffff : 0;
								//	drawMode.LTextColor = drawMode.TextColor ? 0xffffff : 0;
								//	if (Rop3 == PATCOPY) {
								//		if (drawMode.LbkColor) {
								//			SelectObject(dstbmphdc.hdc, GetStockObject(WHITE_BRUSH));
								//		}
								//		else {
								//			SelectObject(dstbmphdc.hdc, GetStockObject(BLACK_BRUSH));
								//		}
								//	}
								//}

								//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
								//	SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
								//}
								//if (hasDrawMode) {
								//	SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
								//	SetTextColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
								//	if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {

								//	}
								//	SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
								//	SetROP2(dstbmphdc.hdc, drawMode.Rop2);
								//}
								HBRUSH oldBrush = (HBRUSH)SelectObject(dstbmphdc.hdc, value.brs);
								//TRACEOUT2(("BitBlt MEM -> MEM DEST X:%d Y:%d W:%d H:%d", wDestX, wDestY, wDestXext, wDestYext));
								//TRACEOUT2(("  DEST:%d", lpDestDevAddr));
								//TRACEOUT2(("  -> style=%d, hatch=%d, color=%08x, rop=%08x", value.lbrush.lbStyle, value.lbrush.lbHatch, value.lbrush.lbColor, Rop3));
								//if (lpDestDevAddr == 701956096) {
								//	TRACEOUT2(("  CHECK:%d", lpDestDevAddr));
								//}
								PatBlt(dstbmphdc.hdc, wDestX, wDestY, wDestXext, wDestYext, Rop3);
								if (hRgn) SelectClipRgn(dstbmphdc.hdc, NULL);
								SelectObject(dstbmphdc.hdc, oldBrush);

								npdisp_WriteBitmapToPBITMAP(&dstPBmp, &dstbmphdc, dstBeginLine, dstNumLines, dstBeginX, dstCopyWidth);

								npdisp_FreeBitmap(&dstbmphdc);
							}
						}
					}
				}
			}
		}
	}

	if (hRgn) {
		DeleteObject(hRgn);
	}

	return retValue;
}
UINT16 npdisp_func_BitBlt_MEMtoMEM(int hasDstDev, int hasSrcDev, UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, UINT16 wXext, UINT16 wYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr)
{
	return npdisp_func_StretchBlt_MEMtoMEM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wXext, wYext, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr, 0);
}

#endif