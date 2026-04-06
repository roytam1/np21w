/**
 * @file	npdisp.c
 * @brief	Implementation of the Neko Project II Display Adapter
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>
#include	<unordered_set>

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

#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_statsave.h"
#include	"npdisp_rle.h"
#include	"npdisp_mem.h"
#include	"npdisp_palette.h"
#include	"npdisp_gdioutput.h"
#include	"npdisp_gdibitblt.h"

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
#if 0
static void trace_fmt_exF(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUTF(s)	trace_fmt_exF s
#else
#define	TRACEOUTF(s)	(void)s
#endif	/* 1 */
#if 0
static void trace_fmt_exF(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUTP(s)	trace_fmt_exF s
#else
#define	TRACEOUTP(s)	(void)s
#endif	/* 1 */

static void npdisp_releaseScreen(void);
static void npdisp_createScreen(void);

NPDISP npdisp = { 0 };
NPDISP_WINDOWS npdispwin = { 0 };

// *** 排他制御用 *****************

static int npdisp_cs_initialized = 0;
static CRITICAL_SECTION npdisp_cs;
static CRITICAL_SECTION npdisp_cs_exception;
//static int npdisp_cs_execflag = 0;

void npdispcs_enter_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	EnterCriticalSection(&npdisp_cs);
}
BOOL npdispcs_tryenter_criticalsection(void)
{
	if (!npdisp_cs_initialized) return FALSE;
	return TryEnterCriticalSection(&npdisp_cs);
}
void npdispcs_leave_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	LeaveCriticalSection(&npdisp_cs);
}
void npdispcs_enter_exception_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	EnterCriticalSection(&npdisp_cs_exception);
}
void npdispcs_leave_exception_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	LeaveCriticalSection(&npdisp_cs_exception);
}

void npdispcs_initialize(void)
{
	/* クリティカルセクション準備 */
	if (!npdisp_cs_initialized)
	{
		memset(&npdisp_cs, 0, sizeof(npdisp_cs));
		InitializeCriticalSection(&npdisp_cs);
		InitializeCriticalSection(&npdisp_cs_exception);
		npdisp_cs_initialized = 1;
	}
}
void npdispcs_shutdown(void)
{
	/* クリティカルセクション破棄 */
	if (npdisp_cs_initialized)
	{
		memset(&npdisp_cs, 0, sizeof(npdisp_cs));
		DeleteCriticalSection(&npdisp_cs_exception);
		DeleteCriticalSection(&npdisp_cs);
		npdisp_cs_initialized = 0;
	}
}

// *** エクスポート関数処理 *****************

static void npdisp_func_NP2Initialize(UINT16 dpiX, UINT16 dpiY, UINT16 width, UINT16 height, UINT16 bpp)
{
	// 初期化
	npdisp.enabled = 0;
	npdisp.active = 0;
	np2wab.relaystateext = 0;
	np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
	npdisp_releaseScreen();

	if (width) npdisp.width = width;
	if (height) npdisp.height = height;
	if (npdisp.width > WAB_MAX_WIDTH) npdisp.width = WAB_MAX_WIDTH;
	if (npdisp.height > WAB_MAX_HEIGHT) npdisp.height = WAB_MAX_HEIGHT;
	if (npdisp.width < 160) npdisp.width = 160;
	if (npdisp.height < 100) npdisp.height = 100;
	if (bpp) npdisp.bpp = bpp;
	if (npdisp.bpp <= 1) npdisp.bpp = 1;
	else if (npdisp.bpp <= 4) npdisp.bpp = 4;
	else if (npdisp.bpp <= 8) npdisp.bpp = 8;
	else if (npdisp.bpp <= 16) npdisp.bpp = 16;
	else if (npdisp.bpp <= 24) npdisp.bpp = 24;
	else if (npdisp.bpp <= 32) npdisp.bpp = 32;
	if (dpiX) npdisp.dpiX = dpiX;
	if (dpiY) npdisp.dpiY = dpiY;

	npdisp.usePalette = (npdisp.bpp == 8);

	// バージョンを返す
	npdisp_writeMemory16(npdisp.version, npdisp.dataAddr);
}

static UINT16 npdisp_func_Enable_PDEVICE(NPDISP_PDEVICE *lpDevInfo, UINT16 wStyle, const char* lpDestDevType, const char* lpOutputFile, const NPDISP_DEVMODE* lpData) 
{
	memset(lpDevInfo, 0, sizeof(NPDISP_PDEVICE));

	lpDevInfo->bmp.bmType = 0x1003;
	lpDevInfo->bmp.bmWidth = npdisp.width;
	lpDevInfo->bmp.bmHeight = npdisp.height;
	lpDevInfo->bmp.bmBitsPixel = 1;
	lpDevInfo->bmp.bmPlanes = 4;
	npdisp.devType = lpDevInfo->bmp.bmType;
	return 1;
}
static UINT16 npdisp_func_Enable_GDIINFO(NPDISP_GDIINFO *lpDevInfo, UINT16 wStyle, const char* lpDestDevType, const char* lpOutputFile, const NPDISP_DEVMODE* lpData) 
{
	lpDevInfo->dpVersion = 0x030A;
	lpDevInfo->dpTechnology = 1; // DT_RASDISPLAY
	// 値が大きいとオーバーフローしておかしくなるので、解像度640x400の画面サイズ値を基準にしてスケール
	int virtualWidth = 640;
	int virtualHeight = 400;
	if (npdisp.width * 400 > npdisp.height * 640) {
		// 640x400よりも横長 → 横を640相当にする
		virtualHeight = npdisp.height * 640 / npdisp.width;
	}
	else {
		// 640x400よりも縦長 → 縦を400相当にする
		virtualWidth = npdisp.width * 400 / npdisp.height;
	}
	lpDevInfo->dpHorzSize = 240 * virtualWidth / 640;
	lpDevInfo->dpVertSize = 150 * virtualHeight / 400;
	lpDevInfo->dpHorzRes = npdisp.width;
	lpDevInfo->dpVertRes = npdisp.height;
	lpDevInfo->dpNumBrushes = -1;
	lpDevInfo->dpNumPens = -1;// 16 * 5;
	lpDevInfo->futureuse = 0;
	lpDevInfo->dpNumFonts = 0;
	lpDevInfo->dpDEVICEsize = sizeof(NPDISP_PDEVICE);
	lpDevInfo->dpCurves = CC_CIRCLES | CC_ELLIPSES | CC_WIDE | CC_STYLED | CC_WIDESTYLED | CC_INTERIORS | CC_PIE | CC_CHORD | CC_ROUNDRECT;
	lpDevInfo->dpLines = LC_POLYLINE | LC_STYLED | LC_WIDE | LC_WIDESTYLED | LC_INTERIORS;
	lpDevInfo->dpPolygonals = PC_SCANLINE | PC_RECTANGLE | PC_POLYGON | PC_WINDPOLYGON | PC_WIDE | PC_STYLED | PC_WIDESTYLED | PC_INTERIORS;
	lpDevInfo->dpText = TC_RA_ABLE;// 0x0004 | 0x2000;
	lpDevInfo->dpClip = CP_RECTANGLE;
	lpDevInfo->dpRaster = RC_BITBLT | RC_BITMAP64 | RC_DI_BITMAP | RC_BIGFONT | RC_SAVEBITMAP | RC_DIBTODEV | RC_GDI20_OUTPUT | RC_OP_DX_OUTPUT | RC_STRETCHBLT; // 0x4699; // RC_BITBLT | RC_BITMAP64 | RC_SAVEBITMAP | RC_GDI20_OUTPUT | RC_DI_BITMAP;
	lpDevInfo->dpAspectX = 71;
	lpDevInfo->dpAspectY = 71;
	lpDevInfo->dpAspectXY = 100;
	lpDevInfo->dpStyleLen = lpDevInfo->dpAspectXY * 2;
	lpDevInfo->dpLogPixelsX = 96; // ここのDPIはアイコンの文字サイズ等が変わる　変えない方がよさそう？
	lpDevInfo->dpLogPixelsY = 96; // ここのDPIはアイコンの文字サイズ等が変わる　変えない方がよさそう？
	lpDevInfo->dpDCManage = 0x0004;
	lpDevInfo->dpMLoWin.x = lpDevInfo->dpHorzSize * 10;
	lpDevInfo->dpMLoWin.y = lpDevInfo->dpVertSize * 10;
	lpDevInfo->dpMLoVpt.x = (int)virtualWidth;
	lpDevInfo->dpMLoVpt.y = -(int)virtualHeight;
	lpDevInfo->dpMHiWin.x = lpDevInfo->dpHorzSize * 100;
	lpDevInfo->dpMHiWin.y = lpDevInfo->dpVertSize * 100;
	lpDevInfo->dpMHiVpt.x = (int)virtualWidth;
	lpDevInfo->dpMHiVpt.y = -(int)virtualHeight;
	lpDevInfo->dpELoWin.x = 375 * virtualWidth / 640;
	lpDevInfo->dpELoWin.y = 188 * virtualHeight / 400;
	lpDevInfo->dpELoVpt.x = 254 * virtualWidth / 640;
	lpDevInfo->dpELoVpt.y = -127 * virtualHeight / 400;
	lpDevInfo->dpEHiWin.x = 3750 * virtualWidth / 640;
	lpDevInfo->dpEHiWin.y = 1875 * virtualHeight / 400;
	lpDevInfo->dpEHiVpt.x = 254 * virtualWidth / 640;
	lpDevInfo->dpEHiVpt.y = -127 * virtualHeight / 400;
	lpDevInfo->dpTwpWin.x = 5400 * virtualWidth / 640;
	lpDevInfo->dpTwpWin.y = 2700 * virtualHeight / 400;
	lpDevInfo->dpTwpVpt.x = 254 * virtualWidth / 640;
	lpDevInfo->dpTwpVpt.y = -127 * virtualHeight / 400;

	switch (npdisp.bpp) {
	case 1:
		// 2色
		lpDevInfo->dpBitsPixel = 1;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 1;
		break;
	case 4:
		// 16色
		lpDevInfo->dpBitsPixel = 4;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 16;
		break;
	case 8:
		// 256色
		lpDevInfo->dpBitsPixel = 8;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 20; // 20;
		break;
	case 16:
		// 64k色
		lpDevInfo->dpBitsPixel = 16;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 4096;
		break;
	case 24:
		// 16M色(24bit)
		lpDevInfo->dpBitsPixel = 24;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 4096;
		break;
	case 32:
		// 16M色(32bit)
		lpDevInfo->dpBitsPixel = 32;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 4096;
		break;
	}
	if (npdisp.usePalette) {
		lpDevInfo->dpRaster |= RC_PALETTE;
		lpDevInfo->dpPalColors = 256;
		lpDevInfo->dpPalReserved = 20;
		lpDevInfo->dpPalResolution = 24;
	}
	else {
		lpDevInfo->dpPalColors = 0;
		lpDevInfo->dpPalReserved = 0;
		lpDevInfo->dpPalResolution = 0;
	}

	return sizeof(NPDISP_GDIINFO); // ドキュメントに書かれていないがサイズを返さないと駄目
}
static UINT16 npdisp_func_Enable(UINT32 lpDevInfoAddr, UINT16 wStyle, UINT32 lpDestDevTypeAddr, UINT32 lpOutputFileAddr, UINT32 lpDataAddr)
{
	UINT16 retValue = 0;
	if (lpDevInfoAddr) {
		char* lpDestDevType;
		char* lpOutputFile;
		NPDISP_DEVMODE data;
		lpDestDevType = npdisp_readMemoryString(lpDestDevTypeAddr);
		lpOutputFile = npdisp_readMemoryString(lpOutputFileAddr);
		if (lpDataAddr) {
			npdisp_readMemory(&data, lpDataAddr, sizeof(data));
		}
		switch (wStyle & 0x7fff) {
		case 0:
		{
			NPDISP_PDEVICE devInfo;
			npdisp_readMemory(&devInfo, lpDevInfoAddr, sizeof(devInfo));
			retValue = npdisp_func_Enable_PDEVICE(&devInfo, wStyle, lpDestDevType, lpOutputFile, lpDataAddr ? &data : NULL);
			npdisp_writeMemory(&devInfo, lpDevInfoAddr, 2);
			npdisp_createScreen();
			npdisp.enabled = 1;
			npdisp.active = 1;
			np2wab.realWidth = npdisp.width;
			np2wab.realHeight = npdisp.height;
			np2wab.relaystateext = 3;
			np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			npdisp.updated = 1;
			TRACEOUT(("Enable PDEVICE"));
			break;
		}
		case 1:
		{
			NPDISP_GDIINFO gdiInfo;
			npdisp_readMemory(&gdiInfo, lpDevInfoAddr, sizeof(gdiInfo));
			retValue = npdisp_func_Enable_GDIINFO(&gdiInfo, wStyle, lpDestDevType, lpOutputFile, lpDataAddr ? &data : NULL);
			npdisp_writeMemory(&gdiInfo, lpDevInfoAddr, sizeof(gdiInfo));
			TRACEOUT(("Enable GDIINFO"));
			break;
		}
		}
		if (lpDestDevType) free(lpDestDevType);
		if (lpOutputFile) free(lpOutputFile);
	}
	return retValue;
}

static void npdisp_func_Disable(UINT32 lpDestDevAddr)
{
	if (lpDestDevAddr) {
		NPDISP_PDEVICE destDev;
		npdisp_readMemory(&destDev, lpDestDevAddr, sizeof(destDev));
		npdisp.enabled = 0;
		npdisp.active = 0;
		np2wab.relaystateext = 0;
		np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
	}
}

static SINT16 npdisp_func_GetDriverResourceID(SINT16 iResId, UINT32 lpResTypeAddr)
{
	// DPI毎のリソース変換？
	if (lpResTypeAddr) {
		if (lpResTypeAddr & 0xffff0000) {
			char* lpResType;
			lpResType = npdisp_readMemoryString(lpResTypeAddr);
			if (lpResType) free(lpResType);
		}
		else {
			// 上位が0の時はただの値
			SINT16 iResType = lpResTypeAddr;
		}
	}
	if (npdisp.dpiX >= 96 && (iResId > 32647 || iResId == 1 || iResId == 3)) {
		iResId += 2000;
	}
	return iResId;
}

static UINT32 npdisp_func_ColorInfo(NPDISP_PDEVICE* lpDestDev, UINT32 dwColorin, UINT32* lpPColor)
{
	if (npdisp.bpp != 8) {
		// 256色以外　色を素通しする
		if (lpPColor) {
			if (dwColorin & 0xff000000) {
				if (npdisp.bpp == 1) {
					int idx = dwColorin & 0x1;
					*lpPColor = ((UINT32)npdisp_palette_rgb2[idx].r) | ((UINT32)npdisp_palette_rgb2[idx].g << 8) | ((UINT32)npdisp_palette_rgb2[idx].b << 16);
				}
				else if (npdisp.bpp == 4) {
					int idx = dwColorin & 0xf;
					*lpPColor = ((UINT32)npdisp_palette_rgb16[idx].r) | ((UINT32)npdisp_palette_rgb16[idx].g << 8) | ((UINT32)npdisp_palette_rgb16[idx].b << 16);
				}
				else {
					*lpPColor = dwColorin & 0xffffff;
				}
			}
			else {
				*lpPColor = dwColorin;
			}
		}
		return dwColorin;
	}
	else {
		// 256色
		UINT32 rgb;
		int idx = 0;
		if (lpPColor) {
			// 論理カラー値を最も近い物理デバイスカラー値へ変換　
			if (dwColorin & 0xff000000) {
				// dwColorinは論理カラーインデックス？
				*lpPColor = dwColorin;
				idx = dwColorin & 0xffffff;
				if (idx < 0 || (1 << npdisp.bpp) <= idx) {
					return 0;
				}
			}
			else {
				// dwColorinは論理カラー値（RGB値）
				UINT8 r, g, b;
				r = (UINT8)(dwColorin & 0xFF);
				g = (UINT8)((dwColorin >> 8) & 0xFF);
				b = (UINT8)((dwColorin >> 16) & 0xFF);
				idx = npdisp_FindNearest256(r, g, b);
				if (idx < 20 || 256 - 20 <= idx) {
					// スタティックカラーはRGBで
					*lpPColor = ((UINT32)npdisp_palette_rgb256[idx].r) | ((UINT32)npdisp_palette_rgb256[idx].g << 8) | ((UINT32)npdisp_palette_rgb256[idx].b << 16);
				}
				else {
					// その他の色は物理パレット番号で
					*lpPColor = (UINT32)idx | 0xff000000;
				}
				if (idx != 0 && idx != 0xff) {
					TRACEOUTP(("IN:%08x IDX: %d", dwColorin, idx));
					if (dwColorin & 0xff000000) {
						TRACEOUTP(("P IN:%08x IDX: %d", dwColorin, idx));
					}
				}
			}
		}
		else {
			// 物理デバイスカラー値を論理カラー値へ変換　dwColorinは物理デバイスカラー値（パレット番号など）
			idx = dwColorin & 0xffffff;
			if (idx < 0 || (1 << npdisp.bpp) <= idx) {
				return 0;
			}
		}

		// 求めたカラーパレットの色をRGBで返す
		return ((UINT32)npdisp_palette_rgb256[idx].r) | ((UINT32)npdisp_palette_rgb256[idx].g << 8) | ((UINT32)npdisp_palette_rgb256[idx].b << 16);
	}
}

static UINT32 npdisp_func_RealizeObject_DeletePen(UINT32 lpInObjAddr)
{
	if (lpInObjAddr) {
		// 指定されたキーのペンを削除
		NPDISP_PEN pen = { {NPDISP_PEN_STYLE_SOLID, {1, 0}, 0} };
		npdisp_readMemory(&pen, lpInObjAddr, sizeof(NPDISP_PEN));
		if (pen.key != 0) {
			auto it = npdispwin.pens.find(pen.key);
			if (it != npdispwin.pens.end()) {
				NPDISP_HOSTPEN value = it->second;
				if (value.refCount > 0) {
					value.refCount--;
				}
				if (value.refCount == 0) {
					npdispwin.pens.erase(it);
					if (value.pen) {
						DeleteObject(value.pen);
					}
				}
			}
		}
		//memset(&pen, 0, sizeof(NPDISP_PEN));
		//npdisp_writeMemory(&pen, lpOutObjAddr, sizeof(NPDISP_PEN));
	}
	TRACEOUT(("RealizeObject Release OBJ_PEN"));

	// サイズを返す
	return sizeof(NPDISP_PEN);
}
static UINT32 npdisp_func_RealizeObject_DeleteBrush(UINT32 lpInObjAddr)
{
	if (lpInObjAddr) {
		// 指定されたキーのブラシを削除
		NPDISP_BRUSH brush = { {NPDISP_BRUSH_STYLE_SOLID, 15, NPDISP_BRUSH_HATCH_HORIZONTAL, 15} };
		npdisp_readMemory(&brush, lpInObjAddr, sizeof(NPDISP_BRUSH));
		if (brush.key != 0) {
			auto it = npdispwin.brushes.find(brush.key);
			if (it != npdispwin.brushes.end()) {
				NPDISP_HOSTBRUSH value = it->second;
				if (value.refCount > 0) {
					value.refCount--;
				}
				if (value.refCount == 0) {
					npdispwin.brushes.erase(it);
					if (value.brs) {
						DeleteObject(value.brs);
					}
				}
			}
		}
		//memset(&brush, 0, sizeof(NPDISP_BRUSH));
		//npdisp_writeMemory(&brush, lpOutObjAddr, sizeof(NPDISP_BRUSH));
	}
	TRACEOUT(("RealizeObject Release OBJ_BRUSH"));

	// サイズを返す
	return sizeof(NPDISP_BRUSH);
}
static void npdisp_createPen(NPDISP_HOSTPEN *lpHostPen) 
{
	if (lpHostPen->pen) return; // 既にあるなら作り直さない

	if (lpHostPen->lpen.opnStyle != PS_NULL) {
		if (lpHostPen->actualColorNum == 0) {
			lpHostPen->actualColorNum = 1;
			lpHostPen->actualColor = npdisp_AdjustColorRefForGDI(lpHostPen->lpen.lopnColor);
		}
		// 実線固定
		SINT16 style = lpHostPen->lpen.opnStyle;
		if (style == PS_INSIDEFRAME) {
			style = PS_SOLID;
		}
		lpHostPen->pen = CreatePen(style, lpHostPen->lpen.lopnWidth.x, lpHostPen->actualColor); // PS_INSIDEFRAMEは二重補正になるので消す
	}
}
static UINT32 npdisp_func_RealizeObject_CreatePen(UINT32 lpInObjAddr, UINT32 lpOutObjAddr)
{
	TRACEOUT(("RealizeObject Create OBJ_PEN"));
	if (lpOutObjAddr) {
		// 作成
		NPDISP_PEN pen = { {NPDISP_PEN_STYLE_SOLID, {1, 0}, 0} };
		NPDISP_HOSTPEN hostpen = { 0 };
		if (lpInObjAddr) {
			// 指定した設定で作る
			npdisp_readMemory(&(pen.lpen), lpInObjAddr, sizeof(NPDISP_LPEN));
		}
		TRACEOUT((" -> Color %08x", pen.lpen.lopnColor));
		int key = 0;
		for (auto it = npdispwin.pens.begin(); it != npdispwin.pens.end(); ++it) {
			if (it->second.lpen.lopnColor == pen.lpen.lopnColor &&
				it->second.lpen.lopnWidth.x == pen.lpen.lopnWidth.x &&
				it->second.lpen.opnStyle == pen.lpen.opnStyle) {
				key = it->first;
				break;
			}
		}
		if (key) {
			pen.key = key;
			if (npdispwin.pens[pen.key].refCount < UINT_MAX) {
				npdispwin.pens[pen.key].refCount++;
			}
		}
		else {
			hostpen.lpen = pen.lpen;
			npdisp_createPen(&hostpen); // ペン生成
			hostpen.refCount = 1;
			pen.key = npdispwin.pensIdx;
			npdispwin.pensIdx++;
			if (npdispwin.pensIdx == 0) npdispwin.pensIdx++; // 0は使わないことにする
			npdispwin.pens[pen.key] = hostpen;
		}

		// 書き込み
		npdisp_writeMemory(&pen, lpOutObjAddr, sizeof(NPDISP_PEN));
	}
	// サイズを返す
	return sizeof(NPDISP_PEN);
}
static void npdisp_createBrush(NPDISP_HOSTBRUSH* lpHostBrush) 
{
	if (lpHostBrush->brs) return; // 既にあるなら作り直さない

	if (lpHostBrush->lbrush.lbStyle == NPDISP_BRUSH_STYLE_SOLID) {
		// 単色ブラシ生成
		if (lpHostBrush->actualColorNum == 0) {
			bool preferDither;
			UINT32 color = npdisp_AdjustColorRefForGDI(lpHostBrush->lbrush.lbColor, &preferDither);
			if (!preferDither) {
				// 純色
				lpHostBrush->actualColorNum = 1;
				lpHostBrush->actualColor = color;
			}
			else {
				// ディザ
				MakePaletteDitherBrushColor(color, &lpHostBrush->actualColor, &lpHostBrush->actualColor2, &lpHostBrush->actualColor2Ratio);
				lpHostBrush->actualColorNum = 2;
			}
		}
		if (lpHostBrush->actualColorNum == 1) {
			// 純色
			lpHostBrush->brs = CreateSolidBrush(lpHostBrush->actualColor);
		}
		else {
			// ディザ
			lpHostBrush->brs = CreatePaletteDitherBrush(lpHostBrush->actualColor, lpHostBrush->actualColor2, lpHostBrush->actualColor2Ratio);
		}
		if (!lpHostBrush->brs) {
			TRACEOUT2(("RealizeObject Create OBJ_BRUSH SOLID ERROR!!!!!!!!!!!!!!"));
		}
		TRACEOUT((" -> Style:%d, Color:%08x", brush.lbrush.lbStyle, brush.lbrush.lbColor));
	}
	else if (lpHostBrush->lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
		// ハッチブラシ生成
		if (lpHostBrush->actualColorNum == 0) {
			lpHostBrush->actualColorNum = 1;
			lpHostBrush->actualColor = npdisp_AdjustColorRefForGDI(lpHostBrush->lbrush.lbColor);
		}
		lpHostBrush->brs = CreateHatchBrush(lpHostBrush->lbrush.lbHatch, lpHostBrush->actualColor);
		if (!lpHostBrush->brs) {
			TRACEOUT2(("RealizeObject Create OBJ_BRUSH HATCHED ERROR!!!!!!!!!!!!!!"));
		}
		TRACEOUT((" -> Style:%d, Color:%08x", brush.lbrush.lbStyle, brush.lbrush.lbColor));
	}
	else if (lpHostBrush->lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
		// パターンブラシ生成
		HDC hdcSrc = npdispwin.hdcCache[0];
		HBITMAP hPatBmpSrc = CreateDIBSection(hdcSrc, (BITMAPINFO*)(&(lpHostBrush->pattern)), DIB_RGB_COLORS, (void**)(&lpHostBrush->pattern.bmBits), NULL, 0);
		if (hPatBmpSrc) {
			HGDIOBJ hOldBmpSrc = SelectObject(hdcSrc, hPatBmpSrc);
			HBITMAP hPatBmp = CreateBitmap(8, 8, 1, 1, NULL); // DDBでないとパターンにできない？
			if (hPatBmp) {
				HDC hdcPat = npdispwin.hdcCache[1];
				HGDIOBJ hOldBmp = SelectObject(hdcPat, hPatBmp);
				BitBlt(hdcPat, 0, 0, 8, 8, hdcSrc, 0, 0, SRCCOPY);
				lpHostBrush->brs = CreatePatternBrush(hPatBmp);
				SelectObject(hdcPat, hOldBmp);
				DeleteObject(hPatBmp);
			}
			SelectObject(hdcSrc, hOldBmpSrc);
			DeleteObject(hPatBmpSrc);
		}
	}
	else if (lpHostBrush->lbrush.lbStyle == NPDISP_BRUSH_STYLE_HOLLOW) {
		// 何もしないブラシ生成
		lpHostBrush->brs = NULL;
	}
}
static UINT32 npdisp_func_RealizeObject_CreateBrush(UINT32 lpInObjAddr, UINT32 lpOutObjAddr)
{
	TRACEOUT(("RealizeObject Create OBJ_BRUSH"));
	if (lpOutObjAddr) {
		// 作成
		NPDISP_BRUSH brush = { {NPDISP_BRUSH_STYLE_SOLID, 15, NPDISP_BRUSH_HATCH_HORIZONTAL, 15} };
		NPDISP_HOSTBRUSH hostbrush = { 0 };
		if (lpInObjAddr) {
			// 指定した設定で作る
			npdisp_readMemory(&(brush.lbrush), lpInObjAddr, sizeof(NPDISP_LBRUSH));
		}
		int key = 0;
		if (brush.lbrush.lbStyle != NPDISP_BRUSH_STYLE_PATTERN) {
			for (auto it = npdispwin.brushes.begin(); it != npdispwin.brushes.end(); ++it) {
				if (it->second.lbrush.lbStyle == brush.lbrush.lbStyle &&
					it->second.lbrush.lbColor == brush.lbrush.lbColor &&
					it->second.lbrush.lbBkColor == brush.lbrush.lbBkColor &&
					it->second.lbrush.lbHatch == brush.lbrush.lbHatch) {
					key = it->first;
					break;
				}
			}
		}
		if (key) {
			brush.key = key;
			if (npdispwin.brushes[brush.key].refCount < UINT_MAX) {
				npdispwin.brushes[brush.key].refCount++;
			}
			TRACEOUT((" -> Style:%d, Color:%08x", brush.lbrush.lbStyle, brush.lbrush.lbColor));
		}
		else {
			hostbrush.lbrush = brush.lbrush;
			if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
				// パターンブラシデータ取得
				NPDISP_PBITMAP patternBmp = { 0 };
				if (npdisp_readMemory(&patternBmp, brush.lbrush.lbColor, sizeof(patternBmp))) {
					NPDISP_WINDOWS_BMPHDC patternBmphdc = { 0 };
					if (npdisp_MakeBitmapFromPBITMAP(&patternBmp, &patternBmphdc, 0)) {
						if (patternBmp.bmHeight < 0) patternBmp.bmHeight = -patternBmp.bmHeight;
						HBITMAP hPatBmp = CreateBitmap(8, 8, 1, 1, NULL); // DDBでないとパターンにできない？
						HDC hdcPat = npdispwin.hdcCache[1];
						HGDIOBJ hOldBmp = SelectObject(hdcPat, hPatBmp);
						BitBlt(hdcPat, 0, 0, 8, 8, patternBmphdc.hdc, 0, 0, SRCCOPY);
						hostbrush.brs = CreatePatternBrush(hPatBmp); // 取得のついでに生成までやる　npdisp_createBrushはnop
						hostbrush.pattern.biHeader.biSize = sizeof(BITMAPINFOHEADER);
						hostbrush.pattern.biHeader.biWidth = 8;
						hostbrush.pattern.biHeader.biHeight = -8;
						hostbrush.pattern.biHeader.biPlanes = 1;
						hostbrush.pattern.biHeader.biBitCount = 1;
						hostbrush.pattern.biHeader.biCompression = BI_RGB;
						GetDIBits(hdcPat, hPatBmp, 0, 8, hostbrush.pattern.bmBits, (BITMAPINFO*)(&hostbrush.pattern.biHeader), DIB_RGB_COLORS);
						SelectObject(hdcPat, hOldBmp);
						DeleteObject(hPatBmp);
						npdisp_FreeBitmap(&patternBmphdc);
					}
					else {
						hostbrush.brs = CreateSolidBrush(npdisp_AdjustColorRefForGDI(brush.lbrush.lbColor));
					}
				}
			}
			npdisp_createBrush(&hostbrush); // ブラシ生成
			hostbrush.refCount = 1;
			brush.key = npdispwin.brushesIdx;
			npdispwin.brushesIdx++;
			if (npdispwin.brushesIdx == 0) npdispwin.brushesIdx++; // 0は使わないことにする
			npdispwin.brushes[brush.key] = hostbrush;
		}
		// 書き込み
		npdisp_writeMemory(&brush, lpOutObjAddr, sizeof(NPDISP_BRUSH));
	}
	// サイズを返す
	return sizeof(NPDISP_BRUSH);
}
static UINT32 npdisp_func_RealizeObject(UINT32 lpDestDevAddr, UINT16 wStyle, UINT32 lpInObjAddr, UINT32 lpOutObjAddr, UINT32 lpTextXFormAddr)
{
	UINT32 retValue = 0;
	NPDISP_PDEVICE destDev;
	npdisp_readMemory(&destDev, lpDestDevAddr, sizeof(destDev));
	if ((SINT16)wStyle < 0) {
		// 削除
		retValue = 1; // 常に成功したことにする
		switch (-((SINT16)wStyle)) {
		case 1: // OBJ_PEN
		{
			retValue = npdisp_func_RealizeObject_DeletePen(lpInObjAddr);
			break;
		}
		case 2: // OBJ_BRUSH
		{
			retValue = npdisp_func_RealizeObject_DeleteBrush(lpInObjAddr);
			break;
		}
		case 3: // OBJ_FONT
		{
			TRACEOUT(("RealizeObject Release OBJ_FONT"));
			// サイズを返す
			retValue = 0;
			break;
		}
		case 5: // OBJ_PBITMAP
		{
			TRACEOUT(("RealizeObject Release OBJ_PBITMAP"));
			// サイズを返す
			retValue = 0;
			break;
		}
		default:
		{
			TRACEOUT(("RealizeObject Release UNKNOWN"));
			break;
		}
		}
	}
	else {
		switch (wStyle) {
		case 1: // OBJ_PEN
		{
			retValue = npdisp_func_RealizeObject_CreatePen(lpInObjAddr, lpOutObjAddr);
			break;
		}
		case 2: // OBJ_BRUSH
		{
			retValue = npdisp_func_RealizeObject_CreateBrush(lpInObjAddr, lpOutObjAddr);
			break;
		}
		case 3: // OBJ_FONT
		{
			TRACEOUT(("RealizeObject Create OBJ_FONT"));
			// 失敗ということにして0を返す
			retValue = 0;
			break;
		}
		case 5: // OBJ_PBITMAP
		{
			TRACEOUT(("RealizeObject Create OBJ_PBITMAP"));
			//if (lpOutObjAddr) {
			//	// 作成
			//	NPDISP_PBITMAP inBmp = { 0 };
			//	NPDISP_PBITMAP outBmp = { 0 };
			//	if (lpInObjAddr) {
			//		npdisp_readMemory(&inBmp, lpInObjAddr, sizeof(NPDISP_PBITMAP));
			//		npdisp_readMemory(&outBmp, lpInObjAddr, sizeof(NPDISP_PBITMAP));
			//	}
			//	// 書き込み
			//	npdisp_writeMemory(&inBmp, lpOutObjAddr, sizeof(NPDISP_PBITMAP));
			//}
			//// サイズを返す
			//retValue = sizeof(NPDISP_PBITMAP);
			retValue = 0; // デバイス作成不可
			break;
		}
		default:
		{
			retValue = 0; // デバイス作成不可
			TRACEOUT(("RealizeObject Create UNKNOWN"));
			break;
		}
		}
	}
	return retValue;
}

static UINT16 npdisp_func_Control(UINT32 lpDestDevAddr, UINT16 wFunction, UINT32 lpInDataAddr, UINT32 lpOutDataAddr)
{
	UINT16 retValue = 0;
	//if (lpDestDevAddr) {
	//	NPDISP_PDEVICE destDev;
	//	npdisp_readMemory(&destDev, lpDestDevAddr, sizeof(destDev));
	//	if (wFunction == 8) {
	//		// QUERYESCSUPPORT
	//		UINT16 escNum = npdisp_readMemory16(lpInDataAddr);
	//		if (escNum == 8) retValue = 1; // QUERYESCSUPPORTは必ずサポート
	//	}
	//	// 必要ならサポート
	//}
	return retValue;
}

static UINT16 npdisp_func_BitBlt(UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, UINT16 wXext, UINT16 wYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr)
{
	int dstDevType = 0;
	int srcDevType = 0;
	int hasDstDev = 0;
	int hasSrcDev = 0;
	UINT16 retValue = 0;
	if (lpDestDevAddr) {
		if (npdisp_readMemory(&dstDevType, lpDestDevAddr, 2)) {
			hasDstDev = 1;
		}
	}
	if (lpSrcDevAddr) {
		if (npdisp_readMemory(&srcDevType, lpSrcDevAddr, 2)) {
			hasSrcDev = 1;
		}
	}
	if (npdisp.longjmpnum == 0) {
		if (dstDevType != 0 && srcDevType != 0) {
			retValue = npdisp_func_BitBlt_VRAMtoVRAM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr);
		}
		else if (dstDevType != 0 && srcDevType == 0) {
			retValue = npdisp_func_BitBlt_MEMtoVRAM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr);
		}
		else if (dstDevType == 0 && srcDevType != 0) {
			retValue = npdisp_func_BitBlt_VRAMtoMEM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr);
		}
		else if (dstDevType == 0 && srcDevType == 0) {
			retValue = npdisp_func_BitBlt_MEMtoMEM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, lpSrcDevAddr, wSrcX, wSrcY, wXext, wYext, Rop3, lpPBrushAddr, lpDrawModeAddr);
		}
	}
	else {
		retValue = 1; // 成功したことにする
	}
	return retValue;
}


static UINT16 npdisp_func_StretchBlt(UINT32 lpDestDevAddr, SINT16 wDestX, SINT16 wDestY, SINT16 wDestXext, SINT16 wDestYext, UINT32 lpSrcDevAddr, SINT16 wSrcX, SINT16 wSrcY, SINT16 wSrcXext, SINT16 wSrcYext, UINT32 Rop3, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipAddr)
{
	int dstDevType = 0;
	int srcDevType = 0;
	int hasDstDev = 0;
	int hasSrcDev = 0;
	UINT16 retValue = 0;

	if (wDestXext < 0 || wDestYext < 0 || wSrcXext < 0 || wSrcYext < 0) {
		// 暫定　反転転送はややこしいのでGDIにやらせる
		return 0xffff;
	}

	if (lpDestDevAddr) {
		if (npdisp_readMemory(&dstDevType, lpDestDevAddr, 2)) {
			hasDstDev = 1;
		}
	}
	if (lpSrcDevAddr) {
		if (npdisp_readMemory(&srcDevType, lpSrcDevAddr, 2)) {
			hasSrcDev = 1;
		}
	}
	if (npdisp.longjmpnum == 0) {
		if (dstDevType != 0 && srcDevType != 0) {
			retValue = npdisp_func_StretchBlt_VRAMtoVRAM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wDestXext, wDestYext, lpSrcDevAddr, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3, lpPBrushAddr, lpDrawModeAddr, lpClipAddr);
		}
		else if (dstDevType != 0 && srcDevType == 0) {
			retValue = npdisp_func_StretchBlt_MEMtoVRAM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wDestXext, wDestYext, lpSrcDevAddr, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3, lpPBrushAddr, lpDrawModeAddr, lpClipAddr);
		}
		else if (dstDevType == 0 && srcDevType != 0) {
			retValue = npdisp_func_StretchBlt_VRAMtoMEM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wDestXext, wDestYext, lpSrcDevAddr, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3, lpPBrushAddr, lpDrawModeAddr, lpClipAddr);
		}
		else if (dstDevType == 0 && srcDevType == 0) {
			retValue = npdisp_func_StretchBlt_MEMtoMEM(hasDstDev, hasSrcDev, lpDestDevAddr, wDestX, wDestY, wDestXext, wDestYext, lpSrcDevAddr, wSrcX, wSrcY, wSrcXext, wSrcYext, Rop3, lpPBrushAddr, lpDrawModeAddr, lpClipAddr);
		}
	}
	else {
		retValue = 1; // 成功したことにする
	}
	return retValue;
}

static UINT16 npdisp_func_DeviceBitmapBits(UINT32 lpBitmapAddr, UINT16 fGet, UINT16 iStart, UINT16 cScans, UINT32 lpDIBitsAddr, UINT32 lpBitmapInfoAddr, UINT32 lpDrawModeAddr, UINT32 lpTranslateAddr)
{
	UINT16 retValue = 0;
	UINT16* transTbl = NULL;
	if (lpTranslateAddr && npdisp.bpp <= 8) {
		int colors = (1 << npdisp.bpp);
		UINT8* transTbl8 = (UINT8*)malloc(colors);
		if (transTbl8) {
			if (npdisp_readMemory(transTbl8, lpTranslateAddr, colors)) {
				transTbl = (UINT16*)malloc(colors * sizeof(UINT16));
				if (transTbl) {
					for (int i = 0; i < colors; i++) {
						transTbl[i] = transTbl8[i];
					}
				}
			}
			free(transTbl8);
		}
	}
	if (lpBitmapAddr) {
		NPDISP_PBITMAP tgtPBmp;
		if (npdisp_readMemory(&tgtPBmp, lpBitmapAddr, sizeof(NPDISP_PBITMAP))) {
			BITMAPINFOHEADER biHeader = { 0 };
			npdisp_readMemory(&biHeader, lpBitmapInfoAddr, sizeof(BITMAPINFOHEADER));
			if (biHeader.biPlanes == 1 && (biHeader.biBitCount == 1 || biHeader.biBitCount == 4 || biHeader.biBitCount == 8 || biHeader.biBitCount == 24 || biHeader.biBitCount == 32) && biHeader.biHeight > iStart) {
				NPDISP_WINDOWS_BMPHDC tgtbmphdc = { 0 };
				int stride = ((biHeader.biWidth * biHeader.biBitCount + 31) / 32) * 4;
				int height = cScans;
				int beginLine = biHeader.biHeight - height - iStart;
				int lpbiLen = 0;
				int lpbiReadLen = 0;
				int lpbiWriteLen = 0;
				if (biHeader.biBitCount <= 8) {
					lpbiLen = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << biHeader.biBitCount);
					if (npdisp.usePalette) {
						if (!lpTranslateAddr) {
							lpbiReadLen = sizeof(BITMAPINFOHEADER) + sizeof(UINT16) * (1 << biHeader.biBitCount);
							lpbiWriteLen = lpbiReadLen;
						}
						else {
							lpbiReadLen = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << biHeader.biBitCount);
							lpbiWriteLen = lpbiLen;
						}
					}
					else {
						lpbiReadLen = lpbiLen;
						lpbiWriteLen = lpbiLen;
					}
					if (lpbiLen < lpbiReadLen) {
						lpbiLen = lpbiReadLen;
					}
					if (lpbiLen < lpbiWriteLen) {
						lpbiLen = lpbiWriteLen;
					}
				}
				else {
					lpbiReadLen = lpbiLen = sizeof(BITMAPINFO);
				}
				BITMAPINFO* lpbi;
				lpbi = (BITMAPINFO*)malloc(lpbiLen);
				if (lpbi) {
					if (lpbiLen > lpbiReadLen) {
						memset((UINT8*)lpbi + lpbiReadLen, 0, lpbiLen - lpbiReadLen);
					}
					npdisp_readMemory(lpbi, lpBitmapInfoAddr, lpbiReadLen);
					if (lpDIBitsAddr) {
						npdisp_PreloadBitmapFromPBITMAP(&tgtPBmp, 0, beginLine, height);
						if (lpbi->bmiHeader.biCompression == BI_RGB) {
							npdisp_preloadMemory(lpDIBitsAddr, stride * height); // 無圧縮なら画像サイズで先読み
						}
						else if (lpbi->bmiHeader.biSizeImage) {
							npdisp_preloadMemory(lpDIBitsAddr, lpbi->bmiHeader.biSizeImage); // RLE圧縮でサイズ既知なら先読み
						}
						if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&tgtPBmp, &tgtbmphdc, 0, beginLine, height)) {
							void* pBits = NULL;
							int i;
							if (iStart + height > biHeader.biHeight) {
								height = biHeader.biHeight - iStart;
							}
							lpbi->bmiHeader.biHeight = height;
							//if (lpbi->bmiHeader.biSizeImage == 0 && lpbi->bmiHeader.biBitCount <= 8) {
							//	// XXX: 画像データがなければパレットセット、あればそのまま　根拠無し
							//	int colors = (1 << lpbi->bmiHeader.biBitCount);
							//	for (i = 0; i < colors; i++) {
							//		if (lpbi->bmiColors[i].rgbReserved || fGet) {
							//			lpbi->bmiColors[i].rgbRed = tgtbmphdc.lpbi->bmiColors[i].rgbRed;
							//			lpbi->bmiColors[i].rgbGreen = tgtbmphdc.lpbi->bmiColors[i].rgbGreen;
							//			lpbi->bmiColors[i].rgbBlue = tgtbmphdc.lpbi->bmiColors[i].rgbBlue;
							//			lpbi->bmiColors[i].rgbReserved = tgtbmphdc.lpbi->bmiColors[i].rgbReserved;
							//		}
							//	}
							//}
							bool useRGBBlt = false;
							if (npdisp.usePalette) {
								if (lpbi->bmiHeader.biBitCount <= 8) {
									int colors = (1 << lpbi->bmiHeader.biBitCount);
									if (lpTranslateAddr) {
										//UINT16 palTrans[256];
										//memcpy(palTrans, lpbi->bmiColors, colors * sizeof(UINT16));
										if (lpbi->bmiHeader.biBitCount == 8) {
											for (i = 0; i < colors; i++) {
												lpbi->bmiColors[i].rgbRed = transTbl[i] & 0xff;
												lpbi->bmiColors[i].rgbGreen = transTbl[i] & 0xff;
												lpbi->bmiColors[i].rgbBlue = transTbl[i] & 0xff;
												lpbi->bmiColors[i].rgbReserved = 0;
											}
										}
										else {
											// XXX: 256色→16色の時はtransTblに16色インデックスへの変換表が入る 本来はこれに従って色変換しなければならない
											// しかし大変過ぎるので、16色パレットの内容決め打ちでカラー転送で進める
											useRGBBlt = true;
										}
									}
									else {
										UINT16 palTrans[256];
										memcpy(palTrans, lpbi->bmiColors, colors * sizeof(UINT16));
										for (i = 0; i < colors; i++) {
											lpbi->bmiColors[i].rgbRed = palTrans[i] & 0xff;
											lpbi->bmiColors[i].rgbGreen = palTrans[i] & 0xff;
											lpbi->bmiColors[i].rgbBlue = palTrans[i] & 0xff;
											lpbi->bmiColors[i].rgbReserved = 0;
										}

									}
								}
							}
							else if (fGet) {
								if (lpbi->bmiHeader.biBitCount == 1) {
									// 有効なパレットでなければ2色パレットセット
									if (lpbi->bmiColors[0].rgbRed != 0 || lpbi->bmiColors[0].rgbGreen != 0 || lpbi->bmiColors[0].rgbBlue != 0 || lpbi->bmiColors[0].rgbReserved != 0 ||
										lpbi->bmiColors[1].rgbRed != 0xff || lpbi->bmiColors[1].rgbGreen != 0xff || lpbi->bmiColors[1].rgbBlue != 0xff || lpbi->bmiColors[1].rgbReserved != 0) {
										for (i = biHeader.biClrUsed; i < NELEMENTS(npdisp_palette_rgb2); i++) {
											lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb2[i].r;
											lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb2[i].g;
											lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb2[i].b;
											lpbi->bmiColors[i].rgbReserved = 0;
										}
										if (fGet) {
											npdisp_writeMemory(lpbi, lpBitmapInfoAddr, lpbiReadLen); // 変更したパレットを書き戻し
										}
									}
								}
								else if (lpbi->bmiHeader.biBitCount == 4) {
									// 有効なパレットでなければ16色パレットセット
									if (lpbi->bmiColors[0].rgbRed != 0 || lpbi->bmiColors[0].rgbGreen != 0 || lpbi->bmiColors[0].rgbBlue != 0 ||
										lpbi->bmiColors[15].rgbRed != 0xff || lpbi->bmiColors[15].rgbGreen != 0xff || lpbi->bmiColors[15].rgbBlue != 0xff) {
										for (i = biHeader.biClrUsed; i < NELEMENTS(npdisp_palette_rgb16); i++) {
											lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb16[i].r;
											lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb16[i].g;
											lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb16[i].b;
											lpbi->bmiColors[i].rgbReserved = 0;
										}
										if (fGet) {
											npdisp_writeMemory(lpbi, lpBitmapInfoAddr, lpbiReadLen); // 変更したパレットを書き戻し
										}
									}
								}
								else if (lpbi->bmiHeader.biBitCount == 8) {
									// 有効なパレットでなければ256色パレットセット
									if (lpbi->bmiColors[0].rgbRed != 0 || lpbi->bmiColors[0].rgbGreen != 0 || lpbi->bmiColors[0].rgbBlue != 0 ||
										lpbi->bmiColors[255].rgbRed != 0xff || lpbi->bmiColors[255].rgbGreen != 0xff || lpbi->bmiColors[255].rgbBlue != 0xff) {
										for (i = biHeader.biClrUsed; i < NELEMENTS(npdisp_palette_rgb256); i++) {
											lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb256[i].r;
											lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb256[i].g;
											lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb256[i].b;
											lpbi->bmiColors[i].rgbReserved = 0;
										}
										if (fGet) {
											npdisp_writeMemory(lpbi, lpBitmapInfoAddr, lpbiReadLen); // 変更したパレットを書き戻し
										}
									}
								}
							}
							UINT32 biCompression = lpbi->bmiHeader.biCompression;
							lpbi->bmiHeader.biCompression = BI_RGB;
							HBITMAP hBmp = CreateDIBSection(npdispwin.hdc, lpbi, DIB_RGB_COLORS, &pBits, NULL, 0);
							if (hBmp) {
								HDC hdc = npdispwin.hdcCache[1];
								HGDIOBJ hOldBmp = SelectObject(hdc, hBmp);
								bool hasError = false;
								if (biCompression == BI_RGB) {
									npdisp_readMemory(pBits, lpDIBitsAddr, stride * height);
								}
								else {
									UINT32 rleSize = lpbi->bmiHeader.biSizeImage;
									if (rleSize == 0) {
										rleSize = stride * height; // とりあえず無圧縮のサイズを確保
									}
									UINT8* cdata = (UINT8*)malloc(rleSize);
									if (cdata) {
										BITMAPINFOHEADER bmiHeaderRLE = lpbi->bmiHeader;
										bmiHeaderRLE.biCompression = biCompression;
										if (bmiHeaderRLE.biHeight > 0) {
											bmiHeaderRLE.biHeight = -bmiHeaderRLE.biHeight; // 逆さで出力
										}
										if (lpbi->bmiHeader.biSizeImage != 0) {
											npdisp_readMemory(cdata, lpDIBitsAddr, rleSize);
										}
										else {
											// RLE終端が来るまで読む 最大読み取りサイズは無圧縮サイズとする
											rleSize = npdisp_RLEBMPReadAndCalcSize(lpDIBitsAddr, bmiHeaderRLE.biCompression, cdata, rleSize);
										}
										npdisp_DecompressRLEBMP(&bmiHeaderRLE, cdata, rleSize, (UINT8*)pBits);
										free(cdata);
									}
								}
								if (!hasError) {
									bool palChanged = false;
									if (npdisp.usePalette) {
										if (lpbi->bmiHeader.biBitCount > 8 || useRGBBlt) {
											// グレースケールから実際のデバイス色へ置き換え
											if (npdisp.bpp == 8) {
												RGBQUAD pal[256];
												for (int i = 0; i < 256; i++) {
													pal[i].rgbRed = npdisp_palette_rgb256[i].r;
													pal[i].rgbGreen = npdisp_palette_rgb256[i].g;
													pal[i].rgbBlue = npdisp_palette_rgb256[i].b;
													pal[i].rgbReserved = 0;
												}
												SetDIBColorTable(tgtbmphdc.hdc, 0, 256, pal);
												palChanged = true;
											}
										}
									}
									if (fGet) {
										// Get Bits
										//BitBlt(hdc, 0, iStart, biHeader.biWidth, cScans, NULL, 0, 0, WHITENESS);
										BitBlt(hdc, 0, 0, biHeader.biWidth, height, tgtbmphdc.hdc, 0, biHeader.biHeight - height - iStart, SRCCOPY);
										retValue = height;
										if (biCompression == BI_RGB) {
											npdisp_writeMemory(pBits, lpDIBitsAddr, stride * height);
										}
										else {
											// サポートしない
											//UINT8* cdata = (UINT8*)malloc(lpbi->bmiHeader.biSizeImage);
											//if (cdata) {

											//	npdisp_writeMemory(cdata, lpDIBitsAddr, lpbi->bmiHeader.biSizeImage);
											//	free(cdata);
											//}
										}
										if (npdisp.usePalette) {
											if (lpbi->bmiHeader.biBitCount <= 8) {
												if (lpTranslateAddr) {
													int colors = (1 << lpbi->bmiHeader.biBitCount);
													if (lpbi->bmiHeader.biBitCount == 8) {
														for (i = 0; i < colors; i++) {
															int idx = lpbi->bmiColors[i].rgbRed;
															lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb256[idx].r;
															lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb256[idx].g;
															lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb256[idx].b;
															lpbi->bmiColors[i].rgbReserved = 0;
														}
														npdisp_writeMemory(lpbi, lpBitmapInfoAddr, lpbiWriteLen); // 論理カラーを書き戻し
													}
													//else if (lpbi->bmiHeader.biBitCount == 4) {
													//	for (i = 0; i < colors; i++) {
													//		int idx = lpbi->bmiColors[i].rgbRed;
													//		lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb16[idx].r;
													//		lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb16[idx].g;
													//		lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb16[idx].b;
													//		lpbi->bmiColors[i].rgbReserved = 0;
													//	}
													//	npdisp_writeMemory(lpbi, lpBitmapInfoAddr, lpbiWriteLen); // 論理カラーを書き戻し
													//}
												}
											}
										}
										if (tgtbmphdc.lpbi->bmiHeader.biWidth == 32 && tgtbmphdc.lpbi->bmiHeader.biHeight == 32) {
											TRACEOUT2(("DeviceBitmapBits Check Get %08x", lpBitmapAddr));
										}
									}
									else {
										// Set Bits
										//BitBlt(tgtbmphdc.hdc, 0, iStart, biHeader.biWidth, cScans, NULL, 0, 0, WHITENESS);
										BitBlt(tgtbmphdc.hdc, 0, biHeader.biHeight - height - iStart, biHeader.biWidth, height, hdc, 0, 0, SRCCOPY);
										//BitBlt(npdispwin.hdc, 0, biHeader.biHeight - height - iStart, biHeader.biWidth, height, hdc, 0, 0, SRCCOPY); // Debug

										retValue = height;
										npdisp.updated = 1;
										npdisp_WriteBitmapToPBITMAP(&tgtPBmp, &tgtbmphdc, beginLine, height);

										if (tgtbmphdc.lpbi->bmiHeader.biWidth == 32 && tgtbmphdc.lpbi->bmiHeader.biHeight == 32) {
											TRACEOUT2(("DeviceBitmapBits Check Set %08x", lpBitmapAddr));
										}
									}

									if (palChanged) {
										// 色を戻す
										SetDIBColorTable(hdc, 0, 256, (RGBQUAD*)npdisp_palette_gray256);
									}
								}

								SelectObject(hdc, hOldBmp);
								DeleteObject(hBmp);
							}
							npdisp_FreeBitmap(&tgtbmphdc);
						}
					}
					else {
						lpbi->bmiHeader.biSizeImage = stride * (biHeader.biHeight >= 0 ? biHeader.biHeight : -biHeader.biHeight);
						npdisp_writeMemory(lpbi, lpBitmapInfoAddr, lpbiReadLen);
					}
					free(lpbi);
				}
				else {
					retValue = 0;
				}
			}
		}
	}
	if (transTbl) {
		free(transTbl);
	}
	return retValue;
}

static UINT32 npdisp_func_ExtTextOut(UINT32 lpDestDevAddr, SINT16 wDestXOrg, SINT16 wDestYOrg, UINT32 lpClipRectAddr, UINT32 lpStringAddr, SINT16 wCount, UINT32 lpFontInfoAddr, UINT32 lpDrawModeAddr, UINT32 lpTextXFormAddr, UINT32 lpCharWidthsAddr, UINT32 lpOpaqueRectAddr, UINT16 wOptions)
{
	UINT32 retValue = 0;
	UINT8* lpText;
	if (wCount != 0) {
		lpText = (UINT8*)npdisp_readMemoryStringWithCount(lpStringAddr, wCount < 0 ? -wCount : wCount);
	}
	else {
		// ダミーをいれておく
		lpText = (UINT8*)malloc(1);
		lpText[0] = '\0';
	}
	if (lpText) {
		if (npdisp.longjmpnum == 0) {
			int i;
			RECT cliprect = { 0 };
			NPDISP_RECT rectTmp = { 0 };
			NPDISP_RECT opaquerect = { 0 };
			npdisp_readMemory(&rectTmp, lpClipRectAddr, sizeof(NPDISP_RECT));
			if (lpOpaqueRectAddr) npdisp_readMemory(&opaquerect, lpOpaqueRectAddr, sizeof(NPDISP_RECT));
			cliprect.top = rectTmp.top;
			cliprect.left = rectTmp.left;
			cliprect.bottom = rectTmp.bottom;
			cliprect.right = rectTmp.right;
			NPDISP_DRAWMODE drawMode = { 0 };
			int hasDrawMode = 0;
			if (lpDrawModeAddr) hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
			//if (npdisp.bpp == 1) {
			//	drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
			//	drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
			//}
			////if (drawMode.LTextColor != 0x000000 && drawMode.LTextColor != 0xffffff && drawMode.LTextColor != 0xffffffff) {
			////	drawMode.LTextColor = 0x00ff0090;
			////}
			//UINT32 bkColor = 0xffffff;
			//UINT32 textColor = 0x000000;
			if (hasDrawMode) {
				npdisp_AdjustDrawModeColor(&drawMode);
			}
			else {
				drawMode.LbkColor = 0xffffff;
				drawMode.LTextColor = 0x000000;
			}
			//HGDIOBJ oldFont = SelectObject(tgtDC, npdispwin.hFont);
			NPDISP_FONTINFO fontInfo;
			if (npdisp_readMemory(&fontInfo, lpFontInfoAddr, sizeof(NPDISP_FONTINFO))) {
				SIZE sz = { 0, fontInfo.dfPixHeight };
				int loopLen = wCount >= 0 ? wCount : -wCount;
				for (i = 0; i < loopLen; i++) {
					NPDISP_FONTCHARINFO3 charInfo;
					int charIdx = (int)lpText[i] - fontInfo.dfFirstChar;
					if (fontInfo.dfLastChar < charIdx) {
						charIdx = fontInfo.dfDefaultChar;
					}
					if (lpCharWidthsAddr) {
						sz.cx += npdisp_readMemory16(lpCharWidthsAddr + i * 2);
					}
					else {
						if (npdisp_readMemory(&charInfo, lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
							sz.cx += charInfo.width;
						}
					}
				}
				//GetTextExtentPoint32A(tgtDC, "NEKO", strlen("NEKO"), &sz);
				retValue = ((UINT32)sz.cx) | ((UINT32)sz.cy << 16);
				if (wCount < 0) {
					// nothing to do
				}
				else if (wCount == 0) {
					// 塗りつぶし
					TRACEOUT(("-> FILL"));
					if (wOptions & 2) {
						NPDISP_PBITMAP dstPBmp;
						NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
						int dstDevType = 0;
						HDC tgtDC = npdispwin.hdc;
						npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);
						if (dstDevType == 0) {
							// memory 
							if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
								if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
									tgtDC = bmphdc.hdc;
								}
							}
						}
						if (npdisp.longjmpnum == 0) {
							if (lpOpaqueRectAddr && (wOptions & 2)) {
								HRGN hRgn = lpClipRectAddr ? CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom) : NULL;
								if (hRgn) {
									SelectClipRgn(tgtDC, hRgn);
								}
								RECT gdiopaquerect = { 0 };
								gdiopaquerect.top = opaquerect.top;
								gdiopaquerect.left = opaquerect.left;
								gdiopaquerect.bottom = opaquerect.bottom;
								gdiopaquerect.right = opaquerect.right;
								HBRUSH hBrush = CreateSolidBrush(drawMode.LbkColor);
								HGDIOBJ oldBrush = SelectObject(tgtDC, hBrush);
								SetBkMode(tgtDC, OPAQUE);
								SetROP2(tgtDC, drawMode.Rop2);
								FillRect(tgtDC, &gdiopaquerect, hBrush);
								//PatBlt(tgtDC, opaquerect.left, opaquerect.top, opaquerect.right - opaquerect.left, opaquerect.bottom - opaquerect.top, drawMode.Rop2);
								//Rectangle(tgtDC, opaquerect.left, opaquerect.top, opaquerect.right, opaquerect.bottom);
								SelectObject(tgtDC, oldBrush);
								DeleteObject(hBrush);
								if (hRgn) {
									SelectClipRgn(tgtDC, NULL);
									DeleteObject(hRgn);
								}
								if (bmphdc.hdc) {
									// 書き戻し
									npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);
								}
								npdisp.updated = 1;
							}
						}
						if (bmphdc.hdc) {
							npdisp_FreeBitmap(&bmphdc);
						}
					}
				}
				else {
					// 描画
					TRACEOUT(("-> TEXT"));

					BITMAPINFO* lpbi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 2);
					if (lpbi) {
						HDC hdcText = CreateCompatibleDC(NULL);
						if (hdcText) {
							//lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
							//lpbi->bmiHeader.biWidth = sz.cx;
							//lpbi->bmiHeader.biHeight = -sz.cy;
							//lpbi->bmiHeader.biPlanes = 1;
							//lpbi->bmiHeader.biBitCount = 1;
							//lpbi->bmiHeader.biCompression = BI_RGB;
							//lpbi->bmiHeader.biSizeImage = 0;
							//lpbi->bmiHeader.biXPelsPerMeter = 0;
							//lpbi->bmiHeader.biYPelsPerMeter = 0;
							//lpbi->bmiHeader.biClrUsed = 0;
							//lpbi->bmiHeader.biClrImportant = 0;
							//lpbi->bmiColors[0].rgbRed = 0xff;
							//lpbi->bmiColors[0].rgbGreen = 0xff;
							//lpbi->bmiColors[0].rgbBlue = 0xff;
							//lpbi->bmiColors[0].rgbReserved = 0x00;
							//lpbi->bmiColors[1].rgbRed = 0x00;
							//lpbi->bmiColors[1].rgbGreen = 0x00;
							//lpbi->bmiColors[1].rgbBlue = 0x00;
							//lpbi->bmiColors[1].rgbReserved = 0x00;
							int stride = ((sz.cx + 7) / 8 + 1) / 2 * 2;
							void* pBits = (char*)malloc(stride * sz.cy);
							if (pBits) {
								HGDIOBJ hbmpOld;
								memset(pBits, 0x00, stride * sz.cy);
								// てんそう
								int baseXbyte = 0;
								int baseXbit = 0;
								for (i = 0; i < loopLen; i++) {
									NPDISP_FONTCHARINFO3 charInfo;
									int charIdx = (int)lpText[i] - fontInfo.dfFirstChar;
									if (fontInfo.dfLastChar < charIdx) {
										charIdx = fontInfo.dfDefaultChar;
									}
									if (npdisp_readMemory(&charInfo, lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
										int y, yx;
										int charXLen = (charInfo.width + 7) / 8;
										for (yx = 0; yx < charXLen; yx++) {
											int curWidth = charInfo.width - yx * 8;
											if (curWidth > 8) curWidth = 8;
											int bitMask = ((1 << curWidth) - 1) << (8 - curWidth);
											int dstBitMask1 = (bitMask >> baseXbit) & 0xff;
											int dstBitMask2 = (bitMask << (8 - baseXbit)) & 0xff;
											char* buf = (char*)pBits + baseXbyte + yx;
											for (y = 0; y < sz.cy; y++) {
												UINT8 data = npdisp_readMemory8With32Offset(fontInfo.dfBitsPointer >> 16, charInfo.offset + yx * sz.cy + y) & bitMask;
												UINT8 data1 = (data >> baseXbit) & 0xff;
												UINT8 data2 = (data << (8 - baseXbit)) & 0xff;
												*buf = (*buf & ~dstBitMask1) | (data1 & dstBitMask1);
												if (dstBitMask2) {
													*(buf + 1) = (*(buf + 1) & ~dstBitMask2) | (data2 & dstBitMask2);
												}
												buf += stride;
											}
										}
									}
									if (lpCharWidthsAddr) {
										int curCharWidth = npdisp_readMemory16(lpCharWidthsAddr + i * 2);
										baseXbyte += (baseXbit + curCharWidth) / 8;
										baseXbit = (baseXbit + curCharWidth) % 8;
									}
									else {
										baseXbyte += (baseXbit + charInfo.width) / 8;
										baseXbit = (baseXbit + charInfo.width) % 8;
									}
								}
								HBITMAP hBmp = CreateBitmap(sz.cx, sz.cy, 1, 1, pBits);
								if (hBmp) {
									HDC hdcTextInv = CreateCompatibleDC(NULL);
									if (hdcTextInv) {
										HBITMAP hInvBmp = CreateBitmap(sz.cx, sz.cy, 1, 1, NULL);
										if (hInvBmp) {
											HGDIOBJ hbmpOld;
											HGDIOBJ hbmpInvOld;
											hbmpOld = SelectObject(hdcText, hBmp);
											hbmpInvOld = SelectObject(hdcTextInv, hInvBmp);
											//SetBkColor(tgtDC, 0xffffff);
											//SetTextColor(tgtDC, 0x000000);
											//SetBkMode(tgtDC, TRANSPARENT);
											//BitBlt(hdcText, 0, 0, sz.cx, sz.cy, NULL, 0, 0, BLACKNESS);
											SetBkColor(hdcTextInv, 0xffffff);
											SetTextColor(hdcTextInv, 0x000000);
											SetBkColor(hdcText, drawMode.LbkColor);
											SetTextColor(hdcText, drawMode.LTextColor);
											BitBlt(hdcTextInv, 0, 0, sz.cx, sz.cy, hdcText, 0, 0, NOTSRCCOPY);
											//SetBkMode(hdcText, TRANSPARENT);
											NPDISP_PBITMAP dstPBmp;
											NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
											int dstDevType = 0;
											HDC tgtDC = npdispwin.hdc;
											npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);
											if (dstDevType == 0) {
												// memory 
												if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
													if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
														tgtDC = bmphdc.hdc;
													}
												}
											}
											if (npdisp.longjmpnum == 0) {
												bool hasClipRect = lpClipRectAddr != 0;
												HRGN hRgn = NULL;
												if (lpOpaqueRectAddr && (wOptions & 2)) {
													RECT gdiopaquerect = { 0 };
													gdiopaquerect.top = opaquerect.top;
													gdiopaquerect.left = opaquerect.left;
													gdiopaquerect.bottom = opaquerect.bottom;
													gdiopaquerect.right = opaquerect.right;
													HBRUSH hBrush = CreateSolidBrush(drawMode.LbkColor);
													HGDIOBJ oldBrush = SelectObject(tgtDC, hBrush);
													TRACEOUT(("-> HAS BACKGROUND"));
													if (hasClipRect) {
														if (cliprect.left < gdiopaquerect.left) cliprect.left = gdiopaquerect.left;
														if (cliprect.top < gdiopaquerect.top) cliprect.top = gdiopaquerect.top;
														if (cliprect.right > gdiopaquerect.right) cliprect.right = gdiopaquerect.right;
														if (cliprect.bottom > gdiopaquerect.bottom) cliprect.bottom = gdiopaquerect.bottom;
														hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
														if (hRgn) {
															SelectClipRgn(tgtDC, hRgn);
														}
														FillRect(tgtDC, &gdiopaquerect, hBrush);

													}
													else {
														cliprect = gdiopaquerect;
														hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
														if (hRgn) {
															SelectClipRgn(tgtDC, hRgn);
														}
														FillRect(tgtDC, &gdiopaquerect, hBrush);
														hasClipRect = true;
													}
													SelectObject(tgtDC, oldBrush);
													DeleteObject(hBrush);
												}
												else if (hasClipRect) {
													hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
													if (hRgn) {
														SelectClipRgn(tgtDC, hRgn);
													}
												}
												SetROP2(tgtDC, R2_COPYPEN);
												HDC hdcTextTgt = hdcText;
												//if (npdisp.bpp == 1 && NPDISP_ADJUST_MONOCOLOR(textColor) != 0) {
												//	hdcTextTgt = hdcTextInv;
												//}
												if ((drawMode.bkMode == 1 || drawMode.bkMode == 4)) {
													// 背景透過
													TRACEOUT(("FG:%08x BG:TRANS", drawMode.LTextColor));
													SetBkMode(tgtDC, OPAQUE);
													SetBkColor(tgtDC, 0x000000);
													SetTextColor(tgtDC, 0xffffff);
													BitBlt(tgtDC, wDestXOrg, wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCAND);
													SetBkColor(tgtDC, drawMode.LTextColor);
													SetTextColor(tgtDC, 0x000000);
													BitBlt(tgtDC, wDestXOrg, wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCPAINT);
												}
												else {
													// 背景不透明
													TRACEOUT(("FG:%08x BG:%08x", drawMode.LTextColor, drawMode.LbkColor));
													SetBkColor(tgtDC, drawMode.LTextColor);
													SetTextColor(tgtDC, drawMode.LbkColor);
													BitBlt(tgtDC, wDestXOrg, wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCCOPY);
												}
												if (hRgn) {
													SelectClipRgn(tgtDC, NULL);
													DeleteObject(hRgn);
												}
												if (bmphdc.hdc) {
													// 書き戻し
													npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);
												}
											}
											if (bmphdc.hdc) {
												npdisp_FreeBitmap(&bmphdc);
											}
											SelectObject(hdcText, hbmpOld);
											SelectObject(hdcTextInv, hbmpInvOld);
											DeleteObject(hInvBmp);
										}
										DeleteDC(hdcTextInv);
									}
									DeleteObject(hBmp);
								}
								free(pBits);
							}
							DeleteDC(hdcText);
						}
						free(lpbi);
					}

					npdisp.updated = 1;
				}
			}
		}
		//SelectObject(npdispwin.hdc, oldFont);
		free(lpText);
	}
	return retValue;
}

static UINT16 npdisp_func_SetDIBitsToDevice(UINT32 lpDestDevAddr, SINT16 X, SINT16 Y, UINT16 iScan ,UINT16 cScans, UINT32 lpClipRectAddr, UINT32 lpDrawModeAddr, UINT32 lpDIBitsAddr, UINT32 lpBitmapInfoAddr, UINT32 lpTranslateAddr)
{
	int dstDevType = 0;
	UINT16* transTbl = NULL;
	if (lpTranslateAddr && npdisp.bpp <= 8) {
		int colors = (1 << npdisp.bpp);
		UINT8* transTbl8 = (UINT8*)malloc(colors);
		if (transTbl8) {
			if (npdisp_readMemory(transTbl8, lpTranslateAddr, colors)) {
				transTbl = (UINT16*)malloc(colors * sizeof(UINT16));
				if (transTbl) {
					for (int i = 0; i < colors; i++) {
						transTbl[i] = transTbl8[i];
					}
				}
			}
			free(transTbl8);
		}
	}
	UINT16 retValue = 0;
	if (lpDestDevAddr) {
		if (npdisp_readMemory(&dstDevType, lpDestDevAddr, 2)) {
			BITMAPINFOHEADER biHeader = { 0 };
			if (npdisp_readMemory(&biHeader, lpBitmapInfoAddr, sizeof(BITMAPINFOHEADER))) {
				int stride = ((biHeader.biWidth * biHeader.biBitCount + 31) / 32) * 4;
				int height = biHeader.biHeight >= 0 ? biHeader.biHeight : -biHeader.biHeight;
				int lpbiLen = 0;
				int lpbiReadLen = 0;
				if (biHeader.biBitCount <= 8) {
					lpbiLen = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << biHeader.biBitCount);
					if (npdisp.usePalette) {
						lpbiReadLen = sizeof(BITMAPINFOHEADER) + sizeof(UINT16) * (1 << biHeader.biBitCount);
					}
					else {
						lpbiReadLen = lpbiLen;
					}
				}
				else {
					lpbiReadLen = lpbiLen = sizeof(BITMAPINFO);
				}
				BITMAPINFO* lpbi;
				lpbi = (BITMAPINFO*)malloc(lpbiLen);
				if (lpbi) {
					npdisp_readMemory(lpbi, lpBitmapInfoAddr, lpbiReadLen);
					UINT8* pBits = (UINT8*)malloc(stride * height);
					if (pBits) {
						npdisp_readMemory(pBits, lpDIBitsAddr, stride * height);
						if (npdisp.longjmpnum == 0) {
							HDC tgtDC = npdispwin.hdc;
							if (height > iScan) {
								int i;
								if (npdisp.usePalette) {
									if (lpbi->bmiHeader.biBitCount <= 8) {
										int colors = (1 << lpbi->bmiHeader.biBitCount);
										UINT16 palTrans[256];
										memcpy(palTrans, lpbi->bmiColors, colors * sizeof(UINT16));
										for (i = 0; i < colors; i++) {
											lpbi->bmiColors[i].rgbRed = palTrans[i] & 0xff;
											lpbi->bmiColors[i].rgbGreen = palTrans[i] & 0xff;
											lpbi->bmiColors[i].rgbBlue = palTrans[i] & 0xff;
											lpbi->bmiColors[i].rgbReserved = 0;
											lpbi->bmiColors[i].rgbReserved = 0;
										}
									}
								}
								else {
									if (lpbi->bmiHeader.biBitCount == 1) {
										// 2色パレットセット
										for (i = 0; i < NELEMENTS(npdisp_palette_rgb2); i++) {
											lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb2[i].r;
											lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb2[i].g;
											lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb2[i].b;
											lpbi->bmiColors[i].rgbReserved = 0;
										}
									}
									else if (lpbi->bmiHeader.biBitCount == 4) {
										// 有効なパレットでなければ16色パレットセット
										if (lpbi->bmiColors[0].rgbRed != 0 || lpbi->bmiColors[0].rgbGreen != 0 || lpbi->bmiColors[0].rgbBlue != 0 || lpbi->bmiColors[0].rgbReserved != 0 ||
											lpbi->bmiColors[15].rgbRed != 0xff || lpbi->bmiColors[15].rgbGreen != 0xff || lpbi->bmiColors[15].rgbBlue != 0xff || lpbi->bmiColors[15].rgbReserved != 0) {
											for (i = 0; i < NELEMENTS(npdisp_palette_rgb16); i++) {
												lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb16[i].r;
												lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb16[i].g;
												lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb16[i].b;
												lpbi->bmiColors[i].rgbReserved = 0;
											}
										}
									}
								}
								NPDISP_DRAWMODE drawMode = { 0 };
								if (npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE))) {
									npdisp_AdjustDrawModeColor(&drawMode);
									SetBkColor(tgtDC, drawMode.LbkColor);
									SetTextColor(tgtDC, drawMode.LTextColor);
									SetBkMode(tgtDC, drawMode.bkMode);
									SetROP2(tgtDC, drawMode.Rop2);
								}
								HRGN hRgn = NULL;
								if (lpClipRectAddr) {
									RECT cliprect = { 0 };
									NPDISP_RECT rectTmp = { 0 };
									npdisp_readMemory(&rectTmp, lpClipRectAddr, sizeof(NPDISP_RECT));
									cliprect.top = rectTmp.top;
									cliprect.left = rectTmp.left;
									cliprect.bottom = rectTmp.bottom;
									cliprect.right = rectTmp.right;
									hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
									SelectClipRgn(tgtDC, hRgn);
								}

								if (iScan + cScans > height) {
									cScans = height - iScan;
								}

								bool palChanged = false;
								if (npdisp.usePalette) {
									if (lpbi->bmiHeader.biBitCount > 8) {
										// グレースケールから実際のデバイス色へ置き換え
										if (npdisp.bpp == 8) {
											RGBQUAD pal[256];
											for (int i = 0; i < 256; i++) {
												pal[i].rgbRed = npdisp_palette_rgb256[i].r;
												pal[i].rgbGreen = npdisp_palette_rgb256[i].g;
												pal[i].rgbBlue = npdisp_palette_rgb256[i].b;
												pal[i].rgbReserved = 0;
											}
											SetDIBColorTable(tgtDC, 0, 256, pal);
											palChanged = true;
										}
									}
								}

								if (SetDIBitsToDevice(tgtDC, X, Y, biHeader.biWidth, height, 0, 0,
									iScan, cScans, pBits, lpbi, DIB_RGB_COLORS) == 0) {
									TRACEOUTF(("ERROR"));

								}

								if (hRgn) {
									SelectClipRgn(tgtDC, NULL);
									DeleteObject(hRgn);
								}

								if (palChanged) {
									// 色を戻す
									SetDIBColorTable(tgtDC, 0, 256, (RGBQUAD*)npdisp_palette_gray256);
								}

								npdisp.updated = 1;
								retValue = cScans;
								if (cScans == 24 && iScan == 0) {
									TRACEOUTF(("OK"));
								}
							}
						}
						free(pBits);
					}
					free(lpbi);
				}
			}
		}
	}
	if (transTbl) {
		free(transTbl);
	}
	return retValue;
}

static UINT16 npdisp_func_SaveScreenBitmap(UINT32 lpRect, UINT16 wCommand)
{
	UINT16 retValue = 0;
	NPDISP_RECT rect = { 0 };
	if (npdisp_readMemory(&rect, lpRect, sizeof(rect)) && npdisp.longjmpnum == 0) {
		if (wCommand == 0) {
			BitBlt(npdispwin.hdcShadow, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, npdispwin.hdc, rect.left, rect.top, SRCCOPY);
			npdispwin.rectShadow.left = rect.left;
			npdispwin.rectShadow.right = rect.right;
			npdispwin.rectShadow.top = rect.top;
			npdispwin.rectShadow.bottom = rect.bottom;
			retValue = 1;
		}
		else if (wCommand == 1) {
			if (npdispwin.rectShadow.left == rect.left && npdispwin.rectShadow.right == rect.right && npdispwin.rectShadow.top == rect.top && npdispwin.rectShadow.bottom == rect.bottom) {
				BitBlt(npdispwin.hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, npdispwin.hdcShadow, rect.left, rect.top, SRCCOPY);
				npdisp.updated = 1;
				retValue = 1;
			}
			else {
				retValue = 0;
			}
		}
		else if (wCommand == 2) {
			BitBlt(npdispwin.hdcShadow, 0, 0, npdisp.width, npdisp.height, NULL, 0, 0, BLACKNESS);
			npdispwin.rectShadow.left = 0;
			npdispwin.rectShadow.right = 0;
			npdispwin.rectShadow.top = 0;
			npdispwin.rectShadow.bottom = 0;
			retValue = 1;
		}
	}
	return retValue;
}

static void npdisp_func_SetCursor(UINT32 lpCursorShapeAddr)
{
	NPDISP_CURSORSHAPE cursorShape = { 0 };
	if (npdisp_readMemory(&cursorShape, lpCursorShapeAddr, sizeof(cursorShape)) && npdisp.longjmpnum == 0) {
		int cursorDataStride = cursorShape.csWidthBytes;
		int stride = ((cursorShape.csWidth + 7) / 8 + 1) / 2 * 2;
		if (cursorShape.csWidth > 0 && cursorShape.csHeight > 0) {
			void* pBitsCursorMask = (char*)malloc(stride * cursorShape.csHeight);
			if (pBitsCursorMask) {
				void* pBitsCursor = (char*)malloc(stride * cursorShape.csHeight);
				if (pBitsCursor) {
					int x, y;
					HBITMAP hBmp;
					UINT8* pBits8 = (UINT8*)pBitsCursorMask;
					UINT32 pixelBufAddr = lpCursorShapeAddr + sizeof(cursorShape);
					int minStride = cursorDataStride < stride ? cursorDataStride : stride;
					for (y = 0; y < cursorShape.csHeight; y++) {
						for (x = 0; x < minStride; x++) {
							UINT8 value = npdisp_readMemory8(pixelBufAddr);
							*pBits8 = value;
							pixelBufAddr++;
							pBits8++;
						}
						if (stride > cursorDataStride) {
							pBits8 += stride - cursorDataStride;
						}
						else if (stride < cursorDataStride) {
							pixelBufAddr += cursorDataStride - stride;
						}
					}
					hBmp = CreateBitmap(cursorShape.csWidth, cursorShape.csHeight, 1, 1, pBitsCursorMask);
					if (hBmp) {
						if (npdispwin.hBmpCursorMask) {
							SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursorMask);
							DeleteObject(npdispwin.hBmpCursorMask);
							npdispwin.hBmpCursorMask = NULL;
						}
						npdispwin.hOldBmpCursorMask = (HBITMAP)SelectObject(npdispwin.hdcCursorMask, hBmp);
						npdispwin.hBmpCursorMask = hBmp;
					}

					pBits8 = (UINT8*)pBitsCursor;
					for (y = 0; y < cursorShape.csHeight; y++) {
						for (x = 0; x < minStride; x++) {
							UINT8 value = npdisp_readMemory8(pixelBufAddr);
							*pBits8 = value;
							pixelBufAddr++;
							pBits8++;
						}
						if (stride > cursorDataStride) {
							pBits8 += stride - cursorDataStride;
						}
						else if (stride < cursorDataStride) {
							pixelBufAddr += cursorDataStride - stride;
						}
					}
					hBmp = CreateBitmap(cursorShape.csWidth, cursorShape.csHeight, 1, 1, pBitsCursor);
					if (hBmp) {
						if (npdispwin.hBmpCursor) {
							SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
							DeleteObject(npdispwin.hBmpCursor);
							npdispwin.hBmpCursor = NULL;
						}
						npdispwin.hOldBmpCursor = (HBITMAP)SelectObject(npdispwin.hdcCursor, hBmp);
						npdispwin.hBmpCursor = hBmp;
					}

					npdisp.cursorHotSpotX = cursorShape.csHotX;
					npdisp.cursorHotSpotY = cursorShape.csHotY;
					npdisp.cursorWidth = cursorShape.csWidth;
					npdisp.cursorHeight = cursorShape.csHeight;

					npdisp.cursorStride = stride;

					void* oldpBitsCursor = npdispwin.pBitsCursor;
					void* oldpBitsCursorMask = npdispwin.pBitsCursorMask;
					npdispwin.pBitsCursor = pBitsCursor;
					npdispwin.pBitsCursorMask = pBitsCursorMask;
					if (oldpBitsCursor) free(oldpBitsCursor);
					if (oldpBitsCursorMask) free(oldpBitsCursorMask);

					npdisp.updated = 1;

				}
				else {
					free(pBitsCursorMask);
				}
			}
		}
		else {
			// 破棄
			if (npdispwin.hBmpCursorMask) {
				SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursorMask);
				DeleteObject(npdispwin.hBmpCursorMask);
				npdispwin.hBmpCursorMask = NULL;
			}
			if (npdispwin.hBmpCursor) {
				SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
				DeleteObject(npdispwin.hBmpCursor);
				npdispwin.hBmpCursor = NULL;
			}
		}
	}
}
static void npdisp_func_MoveCursor(UINT16 wAbsX, UINT16 wAbsY)
{
	if (npdisp.cursorX != wAbsX || npdisp.cursorY != wAbsY) {
		npdisp.cursorX = wAbsX;
		npdisp.cursorY = wAbsY;
		npdisp.updated = 1;
	}
}
static void npdisp_func_CheckCursor()
{
	// nothing to do
}

static UINT16 npdisp_func_FastBorder(UINT32 lpRectAddr, UINT16 wHorizBorderThick, UINT16 wVertBorderThick, UINT32 dwRasterOp, UINT32 lpDestDevAddr, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipRectAddr)
{
	UINT16 retValue = 0;
	int dstDevType = 0;
	HDC tgtDC = npdispwin.hdc;
	npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);
	if (dstDevType != 0) {
		// PDEVICE
		if (lpPBrushAddr) {
			// ブラシがあれば選択
			NPDISP_BRUSH brush = { 0 };
			if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
				if (brush.key != 0) {
					auto it = npdispwin.brushes.find(brush.key);
					if (it != npdispwin.brushes.end()) {
						NPDISP_HOSTBRUSH value = it->second;
						if (value.brs) {
							SelectObject(tgtDC, value.brs);
						}
						else {
							SelectObject(tgtDC, (HBRUSH)GetStockObject(NULL_BRUSH));
						}
					}
				}
			}
		}
		NPDISP_DRAWMODE drawMode = { 0 };
		if (npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE))) {
			npdisp_AdjustDrawModeColor(&drawMode);
			SetBkColor(tgtDC, drawMode.LbkColor);
			SetTextColor(tgtDC, drawMode.LTextColor);
			SetBkMode(tgtDC, drawMode.bkMode);
			SetROP2(tgtDC, drawMode.Rop2);
		}
		HRGN hRgn = NULL;
		if (lpClipRectAddr) {
			RECT cliprect = { 0 };
			NPDISP_RECT rectTmp = { 0 };
			npdisp_readMemory(&rectTmp, lpClipRectAddr, sizeof(NPDISP_RECT));
			cliprect.top = rectTmp.top;
			cliprect.left = rectTmp.left;
			cliprect.bottom = rectTmp.bottom;
			cliprect.right = rectTmp.right;
			hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
			SelectClipRgn(tgtDC, hRgn);
		}

		NPDISP_RECT rectBdr = { 0 };
		npdisp_readMemory(&rectBdr, lpRectAddr, sizeof(NPDISP_RECT));

		int tx = wVertBorderThick;
		int ty = wHorizBorderThick;
		PatBlt(tgtDC, rectBdr.left, rectBdr.top, rectBdr.right - rectBdr.left, ty, dwRasterOp);
		PatBlt(tgtDC, rectBdr.left, rectBdr.top + ty, tx, (rectBdr.bottom - rectBdr.top) - ty * 2, dwRasterOp);
		PatBlt(tgtDC, rectBdr.right - tx, rectBdr.top + ty, tx, (rectBdr.bottom - rectBdr.top) - ty * 2, dwRasterOp);
		PatBlt(tgtDC, rectBdr.left, rectBdr.bottom - ty, rectBdr.right - rectBdr.left, ty, dwRasterOp);
		if (rectBdr.right - rectBdr.left != 52 && rectBdr.right - rectBdr.left != 52) {
			retValue = 1;
		}

		retValue = 1;
		npdisp.updated = 1;

		if (hRgn) {
			SelectClipRgn(tgtDC, NULL);
			DeleteObject(hRgn);
		}
	}

	return retValue;
}

static UINT16 npdisp_func_Output(UINT32 lpDestDevAddr, UINT16 wStyle, UINT16 wCount, UINT32 lpPointsAddr, UINT32 lpPPenAddr, UINT32 lpPBrushAddr, UINT32 lpDrawModeAddr, UINT32 lpClipRectAddr)
{
	UINT16 retValue = 0xffff;
	NPDISP_PBITMAP dstPBmp;
	NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
	int dstDevType = 0;
	HDC tgtDC = npdispwin.hdc;
	npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);

	HPEN curPen = NULL;
	if (lpPPenAddr) {
		// ペンがあれば取得
		NPDISP_PEN pen = { 0 };
		if (npdisp_readMemory(&pen, lpPPenAddr, sizeof(NPDISP_PEN))) {
			if (pen.key != 0) {
				auto it = npdispwin.pens.find(pen.key);
				if (it != npdispwin.pens.end()) {
					NPDISP_HOSTPEN value = it->second;
					if (value.pen) {
						curPen = value.pen;
					}
					else {
						curPen = (HPEN)GetStockObject(NULL_PEN);
					}
				}
			}
		}
	}
	HBRUSH curBrush = NULL;
	if (lpPBrushAddr) {
		// ブラシがあれば取得
		NPDISP_BRUSH brush = { 0 };
		if (npdisp_readMemory(&brush, lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
			if (brush.key != 0) {
				auto it = npdispwin.brushes.find(brush.key);
				if (it != npdispwin.brushes.end()) {
					NPDISP_HOSTBRUSH value = it->second;
					if (value.brs) {
						curBrush = value.brs;
					}
					else {
						curBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
					}
				}
			}
		}
	}

	if (npdisp.longjmpnum != 0) return retValue;

	int dstBeginLine = 0;
	int dstNumLines = -1;
	switch (wStyle) {
	case 18: // OS_POLYLINE
	{
		npdisp_func_Output_GetYRange_POLYLINE(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 80: // OS_BEGINNSCAN
	case 81: // OS_ENDNSCAN
	{
		break;
	}
	case 4: // OS_SCANLINES
	{
		npdisp_func_Output_GetYRange_SCANLINES(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 6: // OS_RECTANGLE
	{
		npdisp_func_Output_GetYRange_RECTANGLE(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 20: // OS_WINDPOLYGON
	case 22: // OS_ALTPOLYGON
	{
		npdisp_func_Output_GetYRange_POLYGON(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 55: // OS_CIRCLE
	case 7: // OS_ELLIPSE 
	{
		npdisp_func_Output_GetYRange_ELLIPSE(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 3: // OS_ARC
	{
		npdisp_func_Output_GetYRange_ARC(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 23: // OS_PIE
	{
		npdisp_func_Output_GetYRange_PIE(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 39: // OS_CHORD 
	{
		npdisp_func_Output_GetYRange_CHORD(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	case 72: // OS_ROUNDRECT 
	{
		npdisp_func_Output_GetYRange_ROUNDRECT(&dstBeginLine, &dstNumLines, curPen, curBrush, wCount, lpPointsAddr);
		break;
	}
	default:
	{
		TRACEOUTF(("Unsupported Output: %d", wStyle));
		break;
	}
	}

	// 負になっていたら補正
	if (dstBeginLine < 0) {
		dstNumLines += dstBeginLine;
		dstBeginLine = 0;
	}

	if (dstDevType == 0) {
		// memory 
		if (dstNumLines != 0) {
			if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
				npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0, dstBeginLine, dstNumLines);
				if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0, dstBeginLine, dstNumLines)) {
					tgtDC = bmphdc.hdc;
				}
			}
		}
		else {
			tgtDC = NULL;
		}
	}
	if (npdisp.longjmpnum == 0) {
		// ペンがあれば選択
		if (curPen) {
			SelectObject(tgtDC, curPen);
		}
		else {
			SelectObject(tgtDC, GetStockObject(NULL_PEN));
		}
		// ブラシがあれば選択
		if (curBrush) {
			SelectObject(tgtDC, curBrush);
		}
		else {
			SelectObject(tgtDC, GetStockObject(NULL_BRUSH));
		}
		NPDISP_DRAWMODE drawMode = { 0 };
		if (npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE))) {
			npdisp_AdjustDrawModeColor(&drawMode);
			SetBkColor(tgtDC, drawMode.LbkColor);
			SetTextColor(tgtDC, drawMode.LTextColor);
			SetBkMode(tgtDC, drawMode.bkMode);
			SetROP2(tgtDC, drawMode.Rop2);
		}
		HRGN hRgn = NULL;
		if (lpClipRectAddr) {
			RECT cliprect = { 0 };
			NPDISP_RECT rectTmp = { 0 };
			npdisp_readMemory(&rectTmp, lpClipRectAddr, sizeof(NPDISP_RECT));
			cliprect.top = rectTmp.top;
			cliprect.left = rectTmp.left;
			cliprect.bottom = rectTmp.bottom;
			cliprect.right = rectTmp.right;
			hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
			SelectClipRgn(tgtDC, hRgn);
		}
		else {
			SelectClipRgn(tgtDC, NULL);
		}
		bool success = false;
		switch (wStyle) {
		case 18: // OS_POLYLINE
		{
			success = npdisp_func_Output_POLYLINE(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 80: // OS_BEGINNSCAN
		case 81: // OS_ENDNSCAN
		{
			retValue = 1;
			break;
		}
		case 4: // OS_SCANLINES
		{
			success = npdisp_func_Output_SCANLINES(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 6: // OS_RECTANGLE
		{
			success = npdisp_func_Output_RECTANGLE(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 20: // OS_WINDPOLYGON
		{
			success = npdisp_func_Output_WINDPOLYGON(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 22: // OS_ALTPOLYGON
		{
			success = npdisp_func_Output_ALTPOLYGON(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 55: // OS_CIRCLE
		case 7: // OS_ELLIPSE 
		{
			success = npdisp_func_Output_ELLIPSE(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 3: // OS_ARC
		{
			success = npdisp_func_Output_ARC(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 23: // OS_PIE
		{
			success = npdisp_func_Output_PIE(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 39: // OS_CHORD 
		{
			success = npdisp_func_Output_CHORD(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		case 72: // OS_ROUNDRECT 
		{
			success = npdisp_func_Output_ROUNDRECT(tgtDC, &bmphdc, &dstPBmp, curPen, curBrush, wCount, lpPointsAddr);
			retValue = 1;
			break;
		}
		default:
		{
			TRACEOUTF(("Unsupported Output: %d", wStyle));
			break;
		}
		}
		if (success && bmphdc.hdc) {
			// 書き戻し
			npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc, dstBeginLine, dstNumLines);
		}
		if (hRgn) {
			SelectClipRgn(tgtDC, NULL);
			DeleteObject(hRgn);
		}
		if (bmphdc.hdc) {
			npdisp_FreeBitmap(&bmphdc);
		}
		npdisp.updated = 1;
	}
	return retValue;
}

static UINT32 npdisp_func_Pixel(UINT32 lpDestDevAddr, UINT16 X, UINT16 Y, UINT32 dwPhysColor, UINT32 lpDrawModeAddr)
{
	UINT32 retValue = 0x80000000L;
	int dstDevType = 0;
	HDC tgtDC = npdispwin.hdc;
	npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);
	NPDISP_PBITMAP dstPBmp;
	NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
	if (dstDevType == 0) {
		// memory 
		if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
			if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
				tgtDC = bmphdc.hdc;
			}
		}
	}
	NPDISP_DRAWMODE drawMode = { 0 };
	int hasDrawMode = npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
	if (hasDrawMode) {
		if (npdisp.usePalette && npdisp.bpp == 8 && (dwPhysColor & 0xff000000)) {
			dwPhysColor = dwPhysColor & 0xff;
			dwPhysColor = dwPhysColor | (dwPhysColor << 8) | (dwPhysColor << 16);
		}
		if (SetPixel(tgtDC, X, Y, dwPhysColor) != -1) {
			retValue = 1;
		}
		npdisp.updated = 1;
	}
	else {
		retValue = GetPixel(tgtDC, X, Y);
		if (npdisp.usePalette && npdisp.bpp == 8) {
			retValue = (retValue & 0xff) | 0xff000000; // to palette index
		}
	}
	if (bmphdc.hdc) {
		npdisp_FreeBitmap(&bmphdc);
	}

	return retValue;
}

static UINT16 npdisp_func_ScanLR(UINT32 lpDestDevAddr, UINT16 X, UINT16 Y, UINT32 dwPhysColor, UINT16 Style)
{
	UINT16 retValue = 0xffff;
	int dstDevType = 0;
	HDC tgtDC = npdispwin.hdc;
	npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);
	NPDISP_PBITMAP dstPBmp;
	NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
	BITMAPINFOHEADER* lpBiHeader = &(npdispwin.bi.bmiHeader);
	UINT8* lpBits = (UINT8*)npdispwin.pBits;
	if (dstDevType == 0) {
		// memory 
		if (lpDestDevAddr && npdisp_readMemory(&dstPBmp, lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
			if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
				tgtDC = bmphdc.hdc;
				lpBiHeader = &(bmphdc.lpbi->bmiHeader);
				lpBits = (UINT8*)bmphdc.pBits;
			}
		}
	}
	UINT32 devColor = dwPhysColor;
	if (npdisp.usePalette && npdisp.bpp == 8 && (devColor & 0xff000000)) {
		devColor &= 0xff; // to palette index
	}
	else {
		const UINT8 r = (UINT8)(devColor & 0xFF);
		const UINT8 g = (UINT8)((devColor >> 8) & 0xFF);
		const UINT8 b = (UINT8)((devColor >> 16) & 0xFF);
		if (npdisp.bpp == 16) {
			const UINT8 r5 = r >> 3;
			const UINT8 g5 = g >> 3;
			const UINT8 b5 = b >> 3;
			devColor = (r5 << 10) | (g5 << 5) | b5;
		}
		else if (npdisp.bpp == 8) {
			devColor = npdisp_FindNearest256(r, g, b);
		}
		else if (npdisp.bpp == 4) {
			devColor = npdisp_FindNearest16(r, g, b);
		}
		else if (npdisp.bpp == 1) {
			devColor = npdisp_FindNearest2(r, g, b);
		}
	}
	int w = lpBiHeader->biWidth;
	int h = lpBiHeader->biHeight;
	UINT32 compMask = (1 << lpBiHeader->biBitCount) - 1;
	UINT32 stepBit = lpBiHeader->biBitCount;
	if (h < 0) h = -h;
	if (Y < h && X < w) {
		int stride = ((lpBiHeader->biWidth * lpBiHeader->biBitCount + 31) / 32) * 4;
		int x;
		if (Style & 2) {
			// 左へスキャン
			lpBits += stride * Y;
			for (x = X; x >= 0; x--) {
				UINT8* p = lpBits + x * stepBit / 8;
				if (lpBiHeader->biBitCount > 16) {
					if (((*((UINT32*)p) & compMask) == devColor) == !!(Style & 0x1)) {
						break;
					}
				}
				else if (lpBiHeader->biBitCount > 8) {
					if (((*((UINT16*)p) & compMask) == devColor) == !!(Style & 0x1)) {
						break;
					}
				}
				else {
					int bitPos = (x * stepBit) % 8;
					bitPos = 7 - bitPos - (stepBit - 1); // 並びを反転
					if ((((*p >> bitPos) & compMask) == devColor) == !!(Style & 0x1)) {
						break;
					}
				}
			}
			if (x == -1) {
				retValue = -1; // 端まで到達
			}
			else {
				retValue = x;
				//if (!(Style & 0x1))retValue++;
			}
		}
		else {
			// 右へスキャン
			lpBits += stride * Y;
			for (x = X; x < w; x++) {
				UINT8* p = lpBits + x * stepBit / 8;
				if (lpBiHeader->biBitCount > 16) {
					if (((*((UINT32*)p) & compMask) == devColor) == !!(Style & 0x1)) {
						break;
					}
				}
				else if (lpBiHeader->biBitCount > 8) {
					if (((*((UINT16*)p) & compMask) == devColor) == !!(Style & 0x1)) {
						break;
					}
				}
				else {
					int bitPos = (x * stepBit) % 8;
					bitPos = 7 - bitPos - (stepBit - 1); // 並びを反転
					if ((((*p >> bitPos) & compMask) == devColor) == !!(Style & 0x1)) {
						break;
					}
				}
			}
			if (x == w) {
				retValue = -1; // 端まで到達
			}
			else {
				retValue = x;
				//if (!(Style & 0x1))retValue--;
			}
		}
	}
	if (bmphdc.hdc) {
		npdisp_FreeBitmap(&bmphdc);
	}

	return retValue;
}

static UINT16 npdisp_func_EnumObj(UINT32 lpDestDevAddr, UINT16 wStyle, UINT16 enumIdx, UINT32 lpLogObjAddr)
{
	UINT16 retValue = 0;
	int dstDevType = 0;
	npdisp_readMemory(&dstDevType, lpDestDevAddr, 2);
	if (dstDevType != 0) {
		UINT16 idx = enumIdx;
		if (wStyle == 1) {
			// pen
			NPDISP_LPEN pen;
			pen.lopnWidth.x = 1;
			pen.lopnWidth.y = 0;
			pen.opnStyle = 0;
			pen.lopnColor = npdisp_ObjIdxToColor(idx);
			if (pen.lopnColor != -1) {
				retValue = 1;
			}
			npdisp_writeMemory(&pen, lpLogObjAddr, sizeof(pen));
		}
		else if (wStyle == 2) {
			// brush
			NPDISP_LBRUSH brush;
			brush.lbBkColor = 1;
			brush.lbHatch = 0;
			brush.lbStyle = 0;
			brush.lbColor = npdisp_ObjIdxToColor(idx);
			if (brush.lbColor != -1) {
				retValue = 1;
			}
			npdisp_writeMemory(&brush, lpLogObjAddr, sizeof(brush));
		}
	}
	return retValue;
}

static void npdisp_func_GetPalette(UINT16 nStartIndex, UINT16 nNumEntries, UINT32 lpPaletteAddr)
{
	if (!npdisp.usePalette) return;
	if (!lpPaletteAddr) return;

	if (nStartIndex < NELEMENTS(npdisp_palette_rgb256)) {
		UINT32 endIdx = (UINT32)nStartIndex + nNumEntries;
		if (endIdx > NELEMENTS(npdisp_palette_rgb256)) endIdx = NELEMENTS(npdisp_palette_rgb256);
		for (int i = nStartIndex; i < endIdx; i++) {
			UINT32 col = ((UINT32)npdisp_palette_rgb256[i].b << 16) | ((UINT32)npdisp_palette_rgb256[i].g << 8) | ((UINT32)npdisp_palette_rgb256[i].r);
			npdisp_writeMemory32(col, lpPaletteAddr);
			lpPaletteAddr += 4;
		}
	}
}
static void npdisp_func_SetPalette(UINT16 nStartIndex, UINT16 nNumEntries, UINT32 lpPaletteAddr)
{
	if (!npdisp.usePalette) return;
	if (!lpPaletteAddr) return;

	if (nStartIndex < NELEMENTS(npdisp_palette_rgb256)) {
		UINT32 endIdx = (UINT32)nStartIndex + nNumEntries;
		if (endIdx > NELEMENTS(npdisp_palette_rgb256)) endIdx = NELEMENTS(npdisp_palette_rgb256);
		for (int i = nStartIndex; i < endIdx; i++) {
			UINT32 col = npdisp_readMemory32(lpPaletteAddr);
			//npdisp_palette_rgb256[i].reserved = (col >> 24) & 0xff;
			npdisp_palette_rgb256[i].b = (col >> 16) & 0xff;
			npdisp_palette_rgb256[i].g = (col >> 8) & 0xff;
			npdisp_palette_rgb256[i].r = col & 0xff;
			lpPaletteAddr += 4;
		}
		npdisp.paletteUpdated = 1;
	}
}
static void npdisp_func_GetPalTrans(UINT32 lpIndexesAddr)
{
	if (!npdisp.usePalette) return;
	if (!lpIndexesAddr) return;

	npdisp_writeMemory(npdisp_palette_transTbl, lpIndexesAddr, sizeof(npdisp_palette_transTbl));
}
static void npdisp_func_SetPalTrans(UINT32 lpIndexesAddr)
{
	if (!npdisp.usePalette) return;

	if (lpIndexesAddr) {
		npdisp_readMemory(npdisp_palette_transTbl, lpIndexesAddr, sizeof(npdisp_palette_transTbl));
	}
	else {
		for (int i = 0; i < NELEMENTS(npdisp_palette_transTbl); i++) {
			npdisp_palette_transTbl[i] = i;
		}
	}
}
static void npdisp_func_UpdateColors(SINT16 wStartX, SINT16 wStartY, UINT16 wExtX, UINT16 wExtY, UINT32 lpTranslateAddr)
{
	if (!npdisp.usePalette) return;
	if (!lpTranslateAddr) return;
	if (npdisp.bpp != 8) return;

	UINT16 transTbl[256];
	npdisp_readMemory(transTbl, lpTranslateAddr, sizeof(transTbl));

	int colors = (npdisp.bpp <= 8) ? (1 << npdisp.bpp) : 0;

	if (wStartX + (int)wExtX <= 0) return;
	if (wStartY + (int)wExtY <= 0) return;
	if (wStartX < 0) {
		wExtX += wStartX;
		wStartX = 0;
	}
	if (wStartY < 0) {
		wExtY += wStartY;
		wStartY = 0;
	}
	if (wStartX >= npdisp.width) return;
	if (wStartY >= npdisp.height) return;
	if ((UINT32)wStartX + wExtX > npdisp.width) wExtX = npdisp.width - wStartX;
	if ((UINT32)wStartY + wExtY > npdisp.height) wExtY = npdisp.height - wStartY;

	// 与えられた変換テーブルを使用してパレット色を置き換え
	int stride = npdispwin.stride;
	UINT8 *lpBuf = (UINT8*)npdispwin.pBits + wStartY * stride + wStartX;
	for (int y = 0; y < wExtY; y++) {
		for (int x = 0; x < wExtX; x++) {
			*lpBuf = transTbl[*lpBuf] & 0xff;
			lpBuf++;
		}
		lpBuf += stride - wExtX;
	}
}

static UINT16 npdisp_func_GetCharWidth(UINT32 lpDestDevAddr, UINT32 lpBufferAddr, UINT16 wFirstChar, UINT16 wLastChar, UINT32 lpFontInfoAddr, UINT32 lpDrawModeAddr, UINT32 lpFontTransAddr) {

	NPDISP_FONTINFO fontInfo;
	if (npdisp_readMemory(&fontInfo, lpFontInfoAddr, sizeof(NPDISP_FONTINFO))) {
		for (int i = wFirstChar; i <= wLastChar; i++) {
			NPDISP_FONTCHARINFO3 charInfo;
			int charIdx = i - (int)fontInfo.dfFirstChar;
			if (charIdx < 0 || fontInfo.dfLastChar - fontInfo.dfFirstChar < charIdx) {
				charIdx = fontInfo.dfDefaultChar;
			}
			if (npdisp_readMemory(&charInfo, lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
				npdisp_writeMemory16(charInfo.width, lpBufferAddr);
				lpBufferAddr += 2;
			}
			else {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

static void npdisp_func_INT2Fh(UINT16 ax)
{
	if (ax == 0x4001) {
		// DOS窓全画面モード設定
		np2wab.relaystateext = 0;
		np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
		npdisp.active = 0;
	}
	else if (ax == 0x4002) {
		// DOS窓全画面モード解除
		npdisp.active = 1;
		npdisp.updated = 1;
		np2wab.relaystateext = 3;
		np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
	}
}

static void npdisp_func_WEP()
{
	// Windows終了
	npdisp.enabled = 0;
	npdisp.active = 0;
	np2wab.relaystateext = 0;
	np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
	npdisp_releaseScreen();
}


// *** エクスポート関数処理 エントリ *****************

/// <summary>
/// ゲストから渡された序数に対応する機能を実行する　データはnpdisp.dataAddrから受け取る
/// </summary>
/// <param name=""></param>
void npdisp_exec(void) {
	// 読み書き開始位置を先頭へ戻す
	npdisp_memory_resetposition();

	UINT16 version = npdisp_readMemory16(npdisp.dataAddr); // バージョンだけ取得

	// 排他開始
	npdispcs_enter_criticalsection();

	if (version >= 1) {
		UINT16 retCode = NPDISP_RETCODE_SUCCESS;
		NPDISP_REQUEST req;
		npdisp_readMemory(&req, npdisp.dataAddr, sizeof(req)); // 全体読み込み
		npdisp.version = req.version; // プロトコルバージョン
		switch (req.funcOrder) {
		case NPDISP_FUNCORDER_NP2INITIALIZE:
		{
			TRACEOUT(("Initialize"));
			npdisp_func_NP2Initialize(req.parameters.init.dpiX, req.parameters.init.dpiY, req.parameters.init.width, req.parameters.init.height, req.parameters.init.bpp);
			break;
		}
		case NPDISP_FUNCORDER_Enable:
		{
			TRACEOUT(("Enable"));
			const UINT16 retValue = npdisp_func_Enable(req.parameters.enable.lpDevInfoAddr, req.parameters.enable.wStyle, req.parameters.enable.lpDestDevTypeAddr, req.parameters.enable.lpOutputFileAddr, req.parameters.enable.lpDataAddr);
			npdisp_writeMemory16(retValue, req.parameters.enable.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Disable:
		{
			TRACEOUT(("Disable"));
			npdisp_func_Disable(req.parameters.disable.lpDestDevAddr);
			break;
		}
		case NPDISP_FUNCORDER_GetDriverResourceID:
		{
			TRACEOUT(("GetDriverResourceID"));
			const SINT16 retValue = npdisp_func_GetDriverResourceID(req.parameters.GetDriverResourceID.iResId, req.parameters.GetDriverResourceID.lpResTypeAddr);
			npdisp_writeMemory16(retValue, req.parameters.GetDriverResourceID.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_ColorInfo:
		{
			//TRACEOUT(("ColorInfo"));
			// 色の変換
			UINT32 retValue = 0;
			NPDISP_PDEVICE devInfo;
			UINT32 pcolor;
			npdisp_readMemory(&retValue, req.parameters.ColorInfo.lpRetValueAddr, 4);
			npdisp_readMemory(&devInfo, req.parameters.ColorInfo.lpDestDevAddr, sizeof(devInfo));
			if (req.parameters.ColorInfo.lpPColorAddr) {
				npdisp_readMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
				retValue = npdisp_func_ColorInfo(&devInfo, req.parameters.ColorInfo.dwColorin, &pcolor);
				npdisp_writeMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
			}
			else {
				retValue = npdisp_func_ColorInfo(&devInfo, req.parameters.ColorInfo.dwColorin, NULL);
			}
			npdisp_writeMemory32(retValue, req.parameters.ColorInfo.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_RealizeObject:
		{
			TRACEOUT(("RealizeObject"));
			// オブジェクト生成と破棄
			const UINT32 retValue = npdisp_func_RealizeObject(req.parameters.RealizeObject.lpDestDevAddr, req.parameters.RealizeObject.wStyle, req.parameters.RealizeObject.lpInObjAddr, req.parameters.RealizeObject.lpOutObjAddr, req.parameters.RealizeObject.lpTextXFormAddr);
			npdisp_writeMemory32(retValue, req.parameters.RealizeObject.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Control:
		{
			TRACEOUT(("Control"));
			const UINT16 retValue = npdisp_func_Control(req.parameters.Control.lpDestDevAddr, req.parameters.Control.wFunction, req.parameters.Control.lpInDataAddr, req.parameters.Control.lpOutDataAddr);
			npdisp_writeMemory16(retValue, req.parameters.Control.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_BitBlt:
		{
			//TRACEOUT(("BitBlt"));
			const UINT16 retValue = npdisp_func_BitBlt(req.parameters.BitBlt.lpDestDevAddr, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3, req.parameters.BitBlt.lpPBrushAddr, req.parameters.BitBlt.lpDrawModeAddr);
			npdisp_writeMemory16(retValue, req.parameters.BitBlt.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_StretchBlt:
		{
			//TRACEOUT(("StretchBlt"));
			const UINT16 retValue = npdisp_func_StretchBlt(req.parameters.stretchBlt.lpDestDevAddr, req.parameters.stretchBlt.wDestX, req.parameters.stretchBlt.wDestY, req.parameters.stretchBlt.wDestXext, req.parameters.stretchBlt.wDestYext, req.parameters.stretchBlt.lpSrcDevAddr, req.parameters.stretchBlt.wSrcX, req.parameters.stretchBlt.wSrcY, req.parameters.stretchBlt.wSrcXext, req.parameters.stretchBlt.wSrcYext, req.parameters.stretchBlt.Rop3, req.parameters.stretchBlt.lpPBrushAddr, req.parameters.stretchBlt.lpDrawModeAddr, req.parameters.stretchBlt.lpClipAddr);
			npdisp_writeMemory16(retValue, req.parameters.stretchBlt.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_DeviceBitmapBits:
		{
			TRACEOUT(("DeviceBitmapBits"));
			const UINT16 retValue = npdisp_func_DeviceBitmapBits(req.parameters.DeviceBitmapBits.lpBitmapAddr, req.parameters.DeviceBitmapBits.fGet, req.parameters.DeviceBitmapBits.iStart, req.parameters.DeviceBitmapBits.cScans, req.parameters.DeviceBitmapBits.lpDIBitsAddr, req.parameters.DeviceBitmapBits.lpBitmapInfoAddr, req.parameters.DeviceBitmapBits.lpDrawModeAddr, req.parameters.DeviceBitmapBits.lpTranslateAddr);
			npdisp_writeMemory16(retValue, req.parameters.DeviceBitmapBits.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_StrBlt:
		case NPDISP_FUNCORDER_ExtTextOut:
		{
			if (req.funcOrder == NPDISP_FUNCORDER_StrBlt) {
				TRACEOUT(("StrBlt"));
				req.parameters.extTextOut.lpCharWidthsAddr = 0;
				req.parameters.extTextOut.lpOpaqueRectAddr = 0;
				req.parameters.extTextOut.wOptions = 0;
			}
			else {
				TRACEOUT(("ExtTextOut"));
			}
			const UINT32 retValue = npdisp_func_ExtTextOut(req.parameters.extTextOut.lpDestDevAddr, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, req.parameters.extTextOut.lpClipRectAddr, req.parameters.extTextOut.lpStringAddr, req.parameters.extTextOut.wCount, req.parameters.extTextOut.lpFontInfoAddr, req.parameters.extTextOut.lpDrawModeAddr, req.parameters.extTextOut.lpTextXFormAddr, req.parameters.extTextOut.lpCharWidthsAddr, req.parameters.extTextOut.lpOpaqueRectAddr, req.parameters.extTextOut.wOptions);
			npdisp_writeMemory32(retValue, req.parameters.extTextOut.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetDIBitsToDevice:
		{
			TRACEOUT(("SetDIBitsToDevice"));
			const UINT16 retValue = npdisp_func_SetDIBitsToDevice(req.parameters.SetDIBitsToDevice.lpDestDevAddr, req.parameters.SetDIBitsToDevice.X, req.parameters.SetDIBitsToDevice.Y, req.parameters.SetDIBitsToDevice.iScan, req.parameters.SetDIBitsToDevice.cScans, req.parameters.SetDIBitsToDevice.lpClipRectAddr, req.parameters.SetDIBitsToDevice.lpDrawModeAddr, req.parameters.SetDIBitsToDevice.lpDIBitsAddr, req.parameters.SetDIBitsToDevice.lpBitmapInfoAddr, req.parameters.SetDIBitsToDevice.lpTranslateAddr);
			npdisp_writeMemory16(retValue, req.parameters.SetDIBitsToDevice.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SaveScreenBitmap:
		{
			TRACEOUT(("SaveScreenBitmap"));
			const UINT16 retValue = npdisp_func_SaveScreenBitmap(req.parameters.SaveScreenBitmap.lpRect, req.parameters.SaveScreenBitmap.wCommand);
			npdisp_writeMemory16(retValue, req.parameters.SaveScreenBitmap.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetCursor:
		{
			TRACEOUT(("SetCursor"));
			npdisp_func_SetCursor(req.parameters.SetCursor.lpCursorShapeAddr);
			break;
		}
		case NPDISP_FUNCORDER_MoveCursor:
		{
			//TRACEOUT(("MoveCursor"));
			npdisp_func_MoveCursor(req.parameters.MoveCursor.wAbsX, req.parameters.MoveCursor.wAbsY);
			break;
		}
		case NPDISP_FUNCORDER_CheckCursor:
		{
			//TRACEOUT(("CheckCursor"));
			npdisp_func_CheckCursor();
			break;
		}
		case NPDISP_FUNCORDER_FastBorder:
		{
			TRACEOUT(("FastBorder"));
			const UINT16 retValue = npdisp_func_FastBorder(req.parameters.fastBorder.lpRectAddr, req.parameters.fastBorder.wHorizBorderThick, req.parameters.fastBorder.wVertBorderThick, req.parameters.fastBorder.dwRasterOp, req.parameters.fastBorder.lpDestDevAddr, req.parameters.fastBorder.lpPBrushAddr, req.parameters.fastBorder.lpDrawModeAddr, req.parameters.fastBorder.lpClipRectAddr);
			npdisp_writeMemory16(retValue, req.parameters.fastBorder.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Output:
		{
			//TRACEOUT(("Output"));
			const UINT16 retValue = npdisp_func_Output(req.parameters.output.lpDestDevAddr, req.parameters.output.wStyle, req.parameters.output.wCount, req.parameters.output.lpPointsAddr, req.parameters.output.lpPPenAddr, req.parameters.output.lpPBrushAddr, req.parameters.output.lpDrawModeAddr, req.parameters.output.lpClipRectAddr);
			npdisp_writeMemory16(retValue, req.parameters.output.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Pixel:
		{
			TRACEOUT(("Pixel"));
			const UINT32 retValue = npdisp_func_Pixel(req.parameters.pixel.lpDestDevAddr, req.parameters.pixel.X, req.parameters.pixel.Y, req.parameters.pixel.dwPhysColor, req.parameters.pixel.lpDrawModeAddr);
			npdisp_writeMemory32(retValue, req.parameters.scanLR.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_ScanLR:
		{
			TRACEOUT(("ScanLR"));
			const UINT16 retValue = npdisp_func_ScanLR(req.parameters.scanLR.lpDestDevAddr, req.parameters.scanLR.X, req.parameters.scanLR.Y, req.parameters.scanLR.dwPhysColor, req.parameters.scanLR.Style);
			npdisp_writeMemory16(retValue, req.parameters.scanLR.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_EnumObj:
		{
			TRACEOUT(("EnumObj"));
			const UINT16 retValue = npdisp_func_EnumObj(req.parameters.enumObj.lpDestDevAddr, req.parameters.enumObj.wStyle, req.parameters.enumObj.enumIdx, req.parameters.enumObj.lpLogObjAddr);
			npdisp_writeMemory16(retValue, req.parameters.enumObj.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_GetPalette:
		{
			TRACEOUTP(("GetPalette"));
			npdisp_func_GetPalette(req.parameters.getPalette.nStartIndex, req.parameters.getPalette.nNumEntries, req.parameters.getPalette.lpPaletteAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetPalette:
		{
			TRACEOUTP(("SetPalette"));
			npdisp_func_SetPalette(req.parameters.setPalette.nStartIndex, req.parameters.setPalette.nNumEntries, req.parameters.setPalette.lpPaletteAddr);
			break;
		}
		case NPDISP_FUNCORDER_GetPalTrans:
		{
			TRACEOUTP(("GetPalTrans"));
			npdisp_func_GetPalTrans(req.parameters.getPalTrans.lpIndexesAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetPalTrans:
		{
			TRACEOUTP(("SetPalTrans"));
			npdisp_func_SetPalTrans(req.parameters.setPalTrans.lpIndexesAddr);
			break;
		}
		case NPDISP_FUNCORDER_UpdateColors:
		{
			TRACEOUTP(("UpdateColors"));
			npdisp_func_UpdateColors(req.parameters.updateColors.wStartX, req.parameters.updateColors.wStartY, req.parameters.updateColors.wExtX, req.parameters.updateColors.wExtY, req.parameters.updateColors.lpTranslateAddr);
			break;
		}
		case NPDISP_FUNCORDER_GetCharWidth:
		{
			TRACEOUTP(("GetCharWidth"));
			const UINT16 retValue = npdisp_func_GetCharWidth(req.parameters.getCharWidth.lpDestDevAddr, req.parameters.getCharWidth.lpBufferAddr, req.parameters.getCharWidth.wFirstChar, req.parameters.getCharWidth.wLastChar, req.parameters.getCharWidth.lpFontInfoAddr, req.parameters.getCharWidth.lpDrawModeAddr, req.parameters.getCharWidth.lpFontTransAddr);
			npdisp_writeMemory16(retValue, req.parameters.getCharWidth.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_INT2Fh:
		{
			TRACEOUT(("INT2Fh"));
			npdisp_func_INT2Fh(req.parameters.INT2Fh.ax);
			break;
		}
		case NPDISP_FUNCORDER_WEP:
		{
			TRACEOUT(("WEP"));
			npdisp_func_WEP();
			break;
		}
		default:
			TRACEOUT(("Function %d", req.funcOrder));
			retCode = NPDISP_RETCODE_FAILED;
			break;
		}
		npdisp_writeReturnCode(&req, npdisp.dataAddr, retCode); // ReturnCode書き込み
	}
	
	// 排他終了
	npdispcs_leave_criticalsection();

	// 例外が発生していたらlongjmpで戻る
	if (npdisp.longjmpnum) {
		if (npdisp_memory_hasNewCacheData()) {
			CPU_STAT_EXCEPTION_COUNTER_CLEAR(); // 読み書きが進んでいたら例外繰り返しではない
		}
		int longjmpnum = npdisp.longjmpnum;
		siglongjmp(exec_1step_jmpbuf, longjmpnum); // 転送
	}

	// 例外発生せずに全部送れたらCPUクロックを進め、読み書きバッファはクリアする
	CPU_REMCLOCK -= 2 * (npdisp_memory_getTotalReadSize() + npdisp_memory_getTotalWriteSize()); // メモリアクセスあたり2clock
	npdisp_memory_clearpreload();
}


#define NPDISP_REQUEST_READFROMSTACK(a, b, c)  npdisp_readMemoryWith32Offset(&(a.b.c), CPU_SS, CPU_BP + 6 + sizeof(a.b) - ((UINT32)((char*)&a.b.c - (char*)&a) - offsetof(NPDISP_REQUEST, parameters.others.arguments)) - sizeof(a.b.c), sizeof(a.b.c))

/// <summary>
/// npdisp_execの高速実行版。スタックから直接パラメータを読み取る。一部のfuncOrderには非対応のため注意。使用するには一度ver.2を指定してnpdisp_execを実行する必要あり。
/// </summary>
/// <param name=""></param>
void npdisp_exec_fast(void) {
	UINT16 lastAX = CPU_AX;
	UINT16 lastDX = CPU_DX;

	// 読み書き開始位置を先頭へ戻す
	npdisp_memory_resetposition();

	UINT16 version = npdisp_readMemory16(npdisp.dataAddr); // バージョンだけ取得

	// 排他開始
	npdispcs_enter_criticalsection();

	// スタックの状態は次のようになっている前提
	// [bp + 6以降]　引数（PASCALコール）
	// [bp + 4]  return CS
	// [bp + 2]  return IP
	// [bp + 0]  old BP

	UINT16 bx = CPU_BX;
	switch (bx) {
	//case NPDISP_FUNCORDER_NP2INITIALIZE: // 非対応
	//{
	//	break;
	//}
	case NPDISP_FUNCORDER_Enable:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.enable, lpDevInfoAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.enable, wStyle);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.enable, lpDestDevTypeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.enable, lpOutputFileAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.enable, lpDataAddr);

		TRACEOUT(("Enable"));
		const UINT16 retValue = npdisp_func_Enable(req.parameters.enable.lpDevInfoAddr, req.parameters.enable.wStyle, req.parameters.enable.lpDestDevTypeAddr, req.parameters.enable.lpOutputFileAddr, req.parameters.enable.lpDataAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_Disable:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.disable, lpDestDevAddr);

		TRACEOUT(("Disable"));
		npdisp_func_Disable(req.parameters.disable.lpDestDevAddr);
		break;
	}
	case NPDISP_FUNCORDER_GetDriverResourceID:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.GetDriverResourceID, iResId);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.GetDriverResourceID, lpResTypeAddr);

		TRACEOUT(("GetDriverResourceID"));
		const SINT16 retValue = npdisp_func_GetDriverResourceID(req.parameters.GetDriverResourceID.iResId, req.parameters.GetDriverResourceID.lpResTypeAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_ColorInfo:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.ColorInfo, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.ColorInfo, dwColorin);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.ColorInfo, lpPColorAddr);

		// 色の変換
		UINT32 retValue = 0;
		NPDISP_PDEVICE devInfo;
		UINT32 pcolor;
		npdisp_readMemory(&devInfo, req.parameters.ColorInfo.lpDestDevAddr, sizeof(devInfo));
		if (req.parameters.ColorInfo.lpPColorAddr) {
			npdisp_readMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
			retValue = npdisp_func_ColorInfo(&devInfo, req.parameters.ColorInfo.dwColorin, &pcolor);
			npdisp_writeMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
		}
		else {
			retValue = npdisp_func_ColorInfo(&devInfo, req.parameters.ColorInfo.dwColorin, NULL);
		}

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue & 0xffff;
			CPU_DX = (retValue >> 16) & 0xffff;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_RealizeObject:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.RealizeObject, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.RealizeObject, wStyle);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.RealizeObject, lpInObjAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.RealizeObject, lpOutObjAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.RealizeObject, lpTextXFormAddr);

		TRACEOUT(("RealizeObject"));
		// オブジェクト生成と破棄
		const UINT32 retValue = npdisp_func_RealizeObject(req.parameters.RealizeObject.lpDestDevAddr, req.parameters.RealizeObject.wStyle, req.parameters.RealizeObject.lpInObjAddr, req.parameters.RealizeObject.lpOutObjAddr, req.parameters.RealizeObject.lpTextXFormAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue & 0xffff;
			CPU_DX = (retValue >> 16) & 0xffff;

			CPU_CX = 0; // 成功の時CXを0に
		}

		break;
	}
	case NPDISP_FUNCORDER_Control:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.Control, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.Control, wFunction);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.Control, lpInDataAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.Control, lpOutDataAddr);

		TRACEOUT(("Control"));
		const UINT16 retValue = npdisp_func_Control(req.parameters.Control.lpDestDevAddr, req.parameters.Control.wFunction, req.parameters.Control.lpInDataAddr, req.parameters.Control.lpOutDataAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_BitBlt:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, wDestX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, wDestY);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, lpSrcDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, wSrcX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, wSrcY);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, wXext);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, wYext);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, Rop3);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, lpPBrushAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.BitBlt, lpDrawModeAddr);

		const UINT16 retValue = npdisp_func_BitBlt(req.parameters.BitBlt.lpDestDevAddr, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3, req.parameters.BitBlt.lpPBrushAddr, req.parameters.BitBlt.lpDrawModeAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_StretchBlt:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wDestX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wDestY);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wDestXext);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wDestYext);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, lpSrcDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wSrcX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wSrcY);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wSrcXext);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, wSrcYext);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, Rop3);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, lpPBrushAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, lpDrawModeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.stretchBlt, lpClipAddr);

		//TRACEOUT(("StretchBlt"));
		const UINT16 retValue = npdisp_func_StretchBlt(req.parameters.stretchBlt.lpDestDevAddr, req.parameters.stretchBlt.wDestX, req.parameters.stretchBlt.wDestY, req.parameters.stretchBlt.wDestXext, req.parameters.stretchBlt.wDestYext, req.parameters.stretchBlt.lpSrcDevAddr, req.parameters.stretchBlt.wSrcX, req.parameters.stretchBlt.wSrcY, req.parameters.stretchBlt.wSrcXext, req.parameters.stretchBlt.wSrcYext, req.parameters.stretchBlt.Rop3, req.parameters.stretchBlt.lpPBrushAddr, req.parameters.stretchBlt.lpDrawModeAddr, req.parameters.stretchBlt.lpClipAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_DeviceBitmapBits:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, lpBitmapAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, fGet);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, iStart);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, cScans);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, lpDIBitsAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, lpBitmapInfoAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, lpDrawModeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.DeviceBitmapBits, lpTranslateAddr);

		TRACEOUT(("DeviceBitmapBits"));
		const UINT16 retValue = npdisp_func_DeviceBitmapBits(req.parameters.DeviceBitmapBits.lpBitmapAddr, req.parameters.DeviceBitmapBits.fGet, req.parameters.DeviceBitmapBits.iStart, req.parameters.DeviceBitmapBits.cScans, req.parameters.DeviceBitmapBits.lpDIBitsAddr, req.parameters.DeviceBitmapBits.lpBitmapInfoAddr, req.parameters.DeviceBitmapBits.lpDrawModeAddr, req.parameters.DeviceBitmapBits.lpTranslateAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_StrBlt:
	case NPDISP_FUNCORDER_ExtTextOut:
	{
		NPDISP_REQUEST req;
		if (bx == NPDISP_FUNCORDER_StrBlt) {
			TRACEOUT(("StrBlt"));
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, lpDestDevAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, wDestXOrg);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, wDestYOrg);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, lpClipRectAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, lpStringAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, wCount);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, lpFontInfoAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, lpDrawModeAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.strBlt, lpTextXFormAddr);
			req.parameters.extTextOut.lpCharWidthsAddr = 0;
			req.parameters.extTextOut.lpOpaqueRectAddr = 0;
			req.parameters.extTextOut.wOptions = 0;
		}
		else {
			TRACEOUT(("ExtTextOut"));
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpDestDevAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, wDestXOrg);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, wDestYOrg);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpClipRectAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpStringAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, wCount);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpFontInfoAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpDrawModeAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpTextXFormAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpCharWidthsAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, lpOpaqueRectAddr);
			NPDISP_REQUEST_READFROMSTACK(req, parameters.extTextOut, wOptions);
		}
		const UINT32 retValue = npdisp_func_ExtTextOut(req.parameters.extTextOut.lpDestDevAddr, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, req.parameters.extTextOut.lpClipRectAddr, req.parameters.extTextOut.lpStringAddr, req.parameters.extTextOut.wCount, req.parameters.extTextOut.lpFontInfoAddr, req.parameters.extTextOut.lpDrawModeAddr, req.parameters.extTextOut.lpTextXFormAddr, req.parameters.extTextOut.lpCharWidthsAddr, req.parameters.extTextOut.lpOpaqueRectAddr, req.parameters.extTextOut.wOptions);
		
		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue & 0xffff;
			CPU_DX = (retValue >> 16) & 0xffff;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_SetDIBitsToDevice:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, X);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, Y);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, iScan);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, cScans);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, lpClipRectAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, lpDrawModeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, lpDIBitsAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, lpBitmapInfoAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetDIBitsToDevice, lpTranslateAddr);

		TRACEOUT(("SetDIBitsToDevice"));
		const UINT16 retValue = npdisp_func_SetDIBitsToDevice(req.parameters.SetDIBitsToDevice.lpDestDevAddr, req.parameters.SetDIBitsToDevice.X, req.parameters.SetDIBitsToDevice.Y, req.parameters.SetDIBitsToDevice.iScan, req.parameters.SetDIBitsToDevice.cScans, req.parameters.SetDIBitsToDevice.lpClipRectAddr, req.parameters.SetDIBitsToDevice.lpDrawModeAddr, req.parameters.SetDIBitsToDevice.lpDIBitsAddr, req.parameters.SetDIBitsToDevice.lpBitmapInfoAddr, req.parameters.SetDIBitsToDevice.lpTranslateAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_SaveScreenBitmap:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SaveScreenBitmap, lpRect);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SaveScreenBitmap, wCommand);

		TRACEOUT(("SaveScreenBitmap"));
		const UINT16 retValue = npdisp_func_SaveScreenBitmap(req.parameters.SaveScreenBitmap.lpRect, req.parameters.SaveScreenBitmap.wCommand);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_SetCursor:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.SetCursor, lpCursorShapeAddr);

		npdisp_func_SetCursor(req.parameters.SetCursor.lpCursorShapeAddr);

		if (!npdisp.longjmpnum) {
			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_MoveCursor:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.MoveCursor, wAbsX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.MoveCursor, wAbsY);

		npdisp_func_MoveCursor(req.parameters.MoveCursor.wAbsX, req.parameters.MoveCursor.wAbsY);

		if (!npdisp.longjmpnum) {
			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_CheckCursor:
	{
		npdisp_func_CheckCursor();

		if (!npdisp.longjmpnum) {
			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_FastBorder:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, lpRectAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, wHorizBorderThick);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, wVertBorderThick);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, dwRasterOp);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, lpPBrushAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, lpDrawModeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.fastBorder, lpClipRectAddr);

		TRACEOUT(("FastBorder"));
		const UINT16 retValue = npdisp_func_FastBorder(req.parameters.fastBorder.lpRectAddr, req.parameters.fastBorder.wHorizBorderThick, req.parameters.fastBorder.wVertBorderThick, req.parameters.fastBorder.dwRasterOp, req.parameters.fastBorder.lpDestDevAddr, req.parameters.fastBorder.lpPBrushAddr, req.parameters.fastBorder.lpDrawModeAddr, req.parameters.fastBorder.lpClipRectAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_Output:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, wStyle);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, wCount);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, lpPointsAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, lpPPenAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, lpPBrushAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, lpDrawModeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.output, lpClipRectAddr);

		const UINT16 retValue = npdisp_func_Output(req.parameters.output.lpDestDevAddr, req.parameters.output.wStyle, req.parameters.output.wCount, req.parameters.output.lpPointsAddr, req.parameters.output.lpPPenAddr, req.parameters.output.lpPBrushAddr, req.parameters.output.lpDrawModeAddr, req.parameters.output.lpClipRectAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_Pixel:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.pixel, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.pixel, X);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.pixel, Y);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.pixel, dwPhysColor);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.pixel, lpDrawModeAddr);

		TRACEOUT(("Pixel"));
		const UINT32 retValue = npdisp_func_Pixel(req.parameters.pixel.lpDestDevAddr, req.parameters.pixel.X, req.parameters.pixel.Y, req.parameters.pixel.dwPhysColor, req.parameters.pixel.lpDrawModeAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue & 0xffff;
			CPU_DX = (retValue >> 16) & 0xffff;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_ScanLR:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.scanLR, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.scanLR, X);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.scanLR, Y);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.scanLR, dwPhysColor);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.scanLR, Style);

		TRACEOUT(("ScanLR"));
		const UINT16 retValue = npdisp_func_ScanLR(req.parameters.scanLR.lpDestDevAddr, req.parameters.scanLR.X, req.parameters.scanLR.Y, req.parameters.scanLR.dwPhysColor, req.parameters.scanLR.Style);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	//case NPDISP_FUNCORDER_EnumObj: // 非対応
	//{
	//	break;
	//}
	case NPDISP_FUNCORDER_GetPalette:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getPalette, nStartIndex);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getPalette, nNumEntries);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getPalette, lpPaletteAddr);

		TRACEOUTP(("GetPalette"));
		npdisp_func_GetPalette(req.parameters.getPalette.nStartIndex, req.parameters.getPalette.nNumEntries, req.parameters.getPalette.lpPaletteAddr);
		break;
	}
	case NPDISP_FUNCORDER_SetPalette:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.setPalette, nStartIndex);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.setPalette, nNumEntries);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.setPalette, lpPaletteAddr);

		TRACEOUTP(("SetPalette"));
		npdisp_func_SetPalette(req.parameters.setPalette.nStartIndex, req.parameters.setPalette.nNumEntries, req.parameters.setPalette.lpPaletteAddr);
		break;
	}
	case NPDISP_FUNCORDER_GetPalTrans:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getPalTrans, lpIndexesAddr);

		TRACEOUTP(("GetPalTrans"));
		npdisp_func_GetPalTrans(req.parameters.getPalTrans.lpIndexesAddr);
		break;
	}
	case NPDISP_FUNCORDER_SetPalTrans:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getPalTrans, lpIndexesAddr);

		TRACEOUTP(("SetPalTrans"));
		npdisp_func_SetPalTrans(req.parameters.setPalTrans.lpIndexesAddr);
		break;
	}
	case NPDISP_FUNCORDER_UpdateColors:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.updateColors, wStartX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.updateColors, wStartY);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.updateColors, wExtX);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.updateColors, wExtY);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.updateColors, lpTranslateAddr);

		TRACEOUTP(("UpdateColors"));
		npdisp_func_UpdateColors(req.parameters.updateColors.wStartX, req.parameters.updateColors.wStartY, req.parameters.updateColors.wExtX, req.parameters.updateColors.wExtY, req.parameters.updateColors.lpTranslateAddr);
		break;
	}
	case NPDISP_FUNCORDER_GetCharWidth:
	{
		NPDISP_REQUEST req;
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, lpDestDevAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, lpBufferAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, wFirstChar);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, wLastChar);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, lpFontInfoAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, lpDrawModeAddr);
		NPDISP_REQUEST_READFROMSTACK(req, parameters.getCharWidth, lpFontTransAddr);

		TRACEOUTP(("GetCharWidth"));
		const UINT16 retValue = npdisp_func_GetCharWidth(req.parameters.getCharWidth.lpDestDevAddr, req.parameters.getCharWidth.lpBufferAddr, req.parameters.getCharWidth.wFirstChar, req.parameters.getCharWidth.wLastChar, req.parameters.getCharWidth.lpFontInfoAddr, req.parameters.getCharWidth.lpDrawModeAddr, req.parameters.getCharWidth.lpFontTransAddr);

		if (!npdisp.longjmpnum) {
			// 戻り値
			CPU_AX = retValue;

			CPU_CX = 0; // 成功の時CXを0に
		}
		break;
	}
	case NPDISP_FUNCORDER_INT2Fh:
	{
		TRACEOUT(("INT2Fh"));
		npdisp_func_INT2Fh(CPU_SI); // 特例 SIに元のAXの値を格納すること
		break;
	}
	case NPDISP_FUNCORDER_WEP:
	{
		TRACEOUT(("WEP"));
		npdisp_func_WEP();
		break;
	}
	default:
	{
		TRACEOUT(("npdisp_exec_fast not supported (Function %d).", req.funcOrder));
		break;
	}
	}

	// 排他終了
	npdispcs_leave_criticalsection();

	// 例外が発生していたらlongjmpで戻る
	if (npdisp.longjmpnum) {
		if (npdisp_memory_hasNewCacheData()) {
			CPU_STAT_EXCEPTION_COUNTER_CLEAR(); // 読み書きが進んでいたら例外繰り返しではない
		}

		// 戻れるようにレジスタセット
		CPU_AX = lastAX;
		CPU_DX = lastDX;
		CPU_CX = (NPDISP_EXEC_MAGIC & 0xffff);

		int longjmpnum = npdisp.longjmpnum;
		siglongjmp(exec_1step_jmpbuf, longjmpnum); // 転送
	}

	// 例外発生せずに全部送れたらCPUクロックを進め、読み書きバッファはクリアする
	CPU_REMCLOCK -= 2 * (npdisp_memory_getTotalReadSize() + npdisp_memory_getTotalWriteSize()); // メモリアクセスあたり2clock
	npdisp_memory_clearpreload();
}

 // ---------- IO Ports

static int npdisp_debug_seqCounter = 0;

static char dbgBuf[32] = { 0 };
static int dbgBufIdx = 0;
static void IOOUTCALL npdisp_o7e7(UINT port, REG8 dat)
{
	dbgBuf[dbgBufIdx] = dat;
	dbgBufIdx++;
	if (dbgBufIdx >= sizeof(dbgBuf) - 1) {
		dbgBufIdx = 0;
	}
}

static void IOOUTCALL npdisp_o7e8(UINT port, REG8 dat)
{
	npdisp.dataAddr = (dat << 24) | (npdisp.dataAddr >> 8);
	if (npdisp_debug_seqCounter >= 4) {
		TRACEOUT(("ADDRESS ERROR! %d %08x %08x", npdisp_debug_seqCounter, CPU_SS, lastID));
	}
	else {
		//TRACEOUT(("ADDRESS %d %08x", npdisp_debug_seqCounter, CPU_SS));
	}
	npdisp_debug_seqCounter++;
	(void)port;
}

static void IOOUTCALL npdisp_o7e9(UINT port, REG8 dat)
{
	if (npdisp.version >= 2 && npdisp.enabled && dat == 'F' && CPU_CX == (NPDISP_EXEC_MAGIC & 0xffff)) {
		// 高速実行パス ver.2以降で対応
		npdisp_exec_fast();
		return;
	}

	const int retFromException = (dat == '1' && npdisp.longjmpnum != 0);
	if (npdisp.cmdBuf != NPDISP_EXEC_MAGIC || !retFromException) {
		npdisp.cmdBuf = (dat << 24) | (npdisp.cmdBuf >> 8);
		if (npdisp.longjmpnum && npdisp_memory_getLastEIP() != CPU_EIP) {
			// 例外処理中に他が来た場合は放棄
			npdisp_memory_clearpreload();
			TRACEOUTF(("DISCARD! %c %08x", (char)dat, CPU_EIP));
		}
	}
	else {
		// 例外復帰の再実行を認める
		TRACEOUTF(("EXCEPTION!!!!!!!!!!!!: %c", (char)dat));
	}

	// エクスポート関数処理実行
	if (npdisp.cmdBuf == NPDISP_EXEC_MAGIC) {
		npdisp_debug_seqCounter = 0;
		npdisp_exec();
	}

	(void)port;
}

static REG8 IOINPCALL npdisp_i7e8(UINT port)
{
	return(98);
}

static REG8 IOINPCALL npdisp_i7e9(UINT port)
{
	return(21);
}

int npdisp_drawGraphic(void) 
{
	UINT32 updated;
	UINT32 paletteUpdated;
	HDC hdc = np2wabwnd.hDCBuf;

	if (!npdispwin.hdc) return 0;

	np2wab.realWidth = npdisp.width;
	np2wab.realHeight = npdisp.height;

	updated = npdisp.updated;
	paletteUpdated = npdisp.paletteUpdated;
	npdisp.updated = 0;
	npdisp.paletteUpdated = 0;

	if (!updated && !paletteUpdated) return 0;

	npdispcs_enter_criticalsection();
	//if (paletteUpdated) {
	//	npdisp_updatePalette();
	//}
	bool palChanged = false;
	if (npdisp.usePalette) {
		// グレースケールから実際のデバイス色へ置き換え
		if (npdisp.bpp == 8) {
			SetDIBColorTable(npdispwin.hdc, 0, 256, (RGBQUAD*)npdisp_palette_rgb256);
			palChanged = true;
		}
		else if (npdisp.bpp == 4) {
			SetDIBColorTable(npdispwin.hdc, 0, 16, (RGBQUAD*)npdisp_palette_rgb16);
			palChanged = true;
		}
	}
	BitBlt(hdc, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, SRCCOPY);
	if (palChanged) {
		// 描画後に元のグレースケールへ戻す
		SetDIBColorTable(npdispwin.hdc, 0, 256, (RGBQUAD*)npdisp_palette_gray256);
	}
	if (npdispwin.hBmpCursorMask && npdispwin.hBmpCursor) {
		SetTextColor(npdispwin.hdcCursorMask, 0);
		SetBkColor(npdispwin.hdcCursorMask, 0xffffff);
		SetTextColor(npdispwin.hdcCursor, 0);
		SetBkColor(npdispwin.hdcCursor, 0xffffff);
		BitBlt(hdc, npdisp.cursorX - npdisp.cursorHotSpotX, npdisp.cursorY - npdisp.cursorHotSpotY, npdisp.cursorWidth, npdisp.cursorHeight, npdispwin.hdcCursorMask, 0, 0, SRCAND);
		BitBlt(hdc, npdisp.cursorX - npdisp.cursorHotSpotX, npdisp.cursorY - npdisp.cursorHotSpotY, npdisp.cursorWidth, npdisp.cursorHeight, npdispwin.hdcCursor, 0, 0, SRCINVERT);
	}
	else {
		//// Test用
		//BitBlt(hdc, npdisp.cursorX, npdisp.cursorY, 4, 4, NULL, 0, 0, BLACKNESS);
		//BitBlt(hdc, npdisp.cursorX + 1, npdisp.cursorY + 1, 2, 2, NULL, 0, 0, WHITENESS);
	}
	npdispcs_leave_criticalsection();

	return 1;
}

static void npdisp_releaseScreen(void) {
	if (npdispwin.hdc) {
		SelectObject(npdispwin.hdc, npdispwin.hOldPen);
		SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
		for (auto it = npdispwin.pens.begin(); it != npdispwin.pens.end(); ++it) {
			if (it->second.pen) DeleteObject(it->second.pen);
		}
		npdispwin.pens.clear();
		npdispwin.pensIdx = 1;
		for (auto it = npdispwin.brushes.begin(); it != npdispwin.brushes.end(); ++it) {
			if (it->second.brs) DeleteObject(it->second.brs);
		}
		npdispwin.brushes.clear();
		npdispwin.brushesIdx = 1;
		SelectObject(npdispwin.hdc, npdispwin.hOldBmp);
		DeleteObject(npdispwin.hBmp);
		SelectObject(npdispwin.hdcShadow, npdispwin.hOldBmpShadow);
		SelectObject(npdispwin.hdcBltBuf, npdispwin.hOldBmpBltBuf);
		DeleteObject(npdispwin.hBmpShadow);
		DeleteObject(npdispwin.hBmpBltBuf);
		DeleteObject(npdispwin.hFont);
		DeleteDC(npdispwin.hdc);
		DeleteDC(npdispwin.hdcShadow);
		DeleteDC(npdispwin.hdcBltBuf);
		npdispwin.hdc = NULL;
		npdispwin.hBmp = NULL;
		npdispwin.hOldBmp = NULL;
		npdispwin.pBits = NULL;
		npdispwin.hdcShadow = NULL;
		npdispwin.hBmpShadow = NULL;
		npdispwin.hOldBmpShadow = NULL;
		npdispwin.pBitsShadow = NULL;
		npdispwin.hdcBltBuf = NULL;
		npdispwin.hBmpBltBuf = NULL;
		npdispwin.hOldBmpBltBuf = NULL;
		npdispwin.pBitsBltBuf = NULL;

		if (npdispwin.pBitsCursor) {
			free(npdispwin.pBitsCursor);
			npdispwin.pBitsCursor = NULL;
		}
		if (npdispwin.pBitsCursorMask) {
			free(npdispwin.pBitsCursorMask);
			npdispwin.pBitsCursorMask = NULL;
		}

		for (int i = 0; i < NELEMENTS(npdispwin.hdcCache); i++) {
			if (npdispwin.hdcCache[i]) {
				DeleteDC(npdispwin.hdcCache[i]);
				npdispwin.hdcCache[i] = NULL;
			}
		}

		if (npdispwin.hdcCursor) {
			if (npdispwin.hBmpCursor) {
				SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
				DeleteObject(npdispwin.hBmpCursor);
				npdispwin.hBmpCursor = NULL;
			}
			DeleteDC(npdispwin.hdcCursor);
			npdispwin.hdcCursor = NULL;
		}
		if (npdispwin.hdcCursorMask) {
			if (npdispwin.hBmpCursorMask) {
				SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursor);
				DeleteObject(npdispwin.hBmpCursorMask);
				npdispwin.hBmpCursorMask = NULL;
			}
			DeleteDC(npdispwin.hdcCursorMask);
			npdispwin.hdcCursorMask = NULL;
		}
		npdisp.cursorHotSpotX = 0;
		npdisp.cursorHotSpotY = 0;
		npdisp.cursorWidth = 0;
		npdisp.cursorHeight = 0;
	}
}
static void npdisp_createScreen(void) {
	const int width = npdisp.width;
	const int height = npdisp.height;
	int i;

	npdisp_releaseScreen();

	HDC hdcScreen = GetDC(NULL);
	npdispwin.hdc = CreateCompatibleDC(hdcScreen);
	npdispwin.hdcShadow = CreateCompatibleDC(hdcScreen);
	npdispwin.hdcBltBuf = CreateCompatibleDC(hdcScreen);

	int colors = (npdisp.bpp <= 8) ? (1 << npdisp.bpp) : 0;

	npdispwin.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	npdispwin.bi.bmiHeader.biWidth = width;
	npdispwin.bi.bmiHeader.biHeight = -height; 
	npdispwin.bi.bmiHeader.biPlanes = 1;
	npdispwin.bi.bmiHeader.biBitCount = npdisp.bpp;
	npdispwin.bi.bmiHeader.biCompression = BI_RGB;
	npdispwin.bi.bmiHeader.biSizeImage = 0;
	npdispwin.bi.bmiHeader.biXPelsPerMeter = 0;
	npdispwin.bi.bmiHeader.biYPelsPerMeter = 0;
	npdispwin.bi.bmiHeader.biClrUsed = colors;
	npdispwin.bi.bmiHeader.biClrImportant = colors;

	if (colors == 2) {
		// 2色パレットセット
		memcpy(npdispwin.bi.bmiColors, npdisp_palette_rgb2, sizeof(RGBQUAD) * NELEMENTS(npdisp_palette_rgb2));
	}
	else if (colors == 16) {
		// 16色パレットセット
		memcpy(npdispwin.bi.bmiColors, npdisp_palette_rgb16, sizeof(RGBQUAD) * NELEMENTS(npdisp_palette_rgb16));
	}
	else if (colors == 256) {
		// 256色パレットセット
		memcpy(npdispwin.bi.bmiColors, npdisp_palette_gray256, sizeof(RGBQUAD) * NELEMENTS(npdisp_palette_gray256));
	}

	npdispwin.hBmp = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBits, NULL, 0);
	if (!npdispwin.hBmp || !npdispwin.pBits) {
		DeleteDC(npdispwin.hdc);
		npdispwin.hdc = NULL;
		return;
	}
	npdispwin.hBmpShadow = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBitsShadow, NULL, 0);
	if (!npdispwin.hBmpShadow || !npdispwin.pBitsShadow) {
		DeleteObject(npdispwin.hBmp);
		DeleteDC(npdispwin.hdc);
		npdispwin.hBmp = NULL;
		npdispwin.hdc = NULL;
		return;
	}
	npdispwin.hBmpBltBuf = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBitsBltBuf, NULL, 0);
	ReleaseDC(NULL, hdcScreen); // もういらない

	npdispwin.hOldPen = SelectObject(npdispwin.hdc, GetStockObject(WHITE_PEN));
	npdispwin.hOldBrush = SelectObject(npdispwin.hdc, GetStockObject(BLACK_BRUSH));

	npdispwin.stride = ((width * npdisp.bpp + 31) / 32) * 4;
	memset(npdispwin.pBits, 0x00, npdispwin.stride * height);

	npdispwin.hOldBmp = SelectObject(npdispwin.hdc, npdispwin.hBmp);
	npdispwin.hOldBmpShadow = SelectObject(npdispwin.hdcShadow, npdispwin.hBmpShadow);
	npdispwin.hOldBmpBltBuf = SelectObject(npdispwin.hdcBltBuf, npdispwin.hBmpBltBuf);

	for (int i = 0; i < NELEMENTS(npdispwin.hdcCache); i++) {
		if (!npdispwin.hdcCache[i]) {
			npdispwin.hdcCache[i] = CreateCompatibleDC(NULL);
		}
	}

	npdispwin.rectShadow.left = 0;
	npdispwin.rectShadow.right = 0;
	npdispwin.rectShadow.top = 0;
	npdispwin.rectShadow.bottom = 0;

	npdispwin.hdcCursor = CreateCompatibleDC(NULL);
	npdispwin.hdcCursorMask = CreateCompatibleDC(NULL);

	BitBlt(npdispwin.hdcShadow, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, BLACKNESS);
	BitBlt(npdispwin.hdcBltBuf, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, BLACKNESS);

	// デバッグ用フォント
	LOGFONT lf = { 0 };
	lf.lfHeight = -9;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	lstrcpy(lf.lfFaceName, _T("MS Gothic"));
	npdispwin.hFont = CreateFontIndirect(&lf);
	SelectObject(npdispwin.hdc, npdispwin.hFont);
}

void npdisp_reset(const NP2CFG* pConfig)
{
	int i;
	npdispcs_initialize();

	npdisp_palette_makeTable();

	npdisp_releaseScreen();

	npdisp.ioenabled = pConfig->usenpdisp;
	npdisp.enabled = 0;
	npdisp.active = 0;
	npdisp.width = 1024;
	npdisp.height = 720;
	npdisp.bpp = 24;
	npdisp.dpiX = 96;
	npdisp.dpiY = 96;
	npdisp.usePalette = 0;
	npdisp.cursorX = 0;
	npdisp.cursorY = 0;
	npdispwin.pensIdx = 1;
	npdispwin.brushesIdx = 1;

	npdispwin.hdcCursor = NULL;
	npdispwin.hBmpCursor = NULL;
	npdispwin.hOldBmpCursor = NULL;
	npdispwin.hdcCursorMask = NULL;
	npdispwin.hBmpCursorMask = NULL;
	npdispwin.hOldBmpCursorMask = NULL;

	npdisp.cursorHotSpotX = 0;
	npdisp.cursorHotSpotY = 0;
	npdisp.cursorWidth = 0;
	npdisp.cursorHeight = 0;

	npdisp_memory_clearpreload();
}
void npdisp_bind(void)
{
	if (npdisp.ioenabled) {
		iocore_attachout(0x07e7, npdisp_o7e7);
		iocore_attachout(0x07e8, npdisp_o7e8);
		iocore_attachout(0x07e9, npdisp_o7e9);
		iocore_attachinp(0x07e8, npdisp_i7e8);
		iocore_attachinp(0x07e9, npdisp_i7e9);
	}
}
void npdisp_unbind(void)
{
	iocore_detachout(0x07e7);
	iocore_detachout(0x07e8);
	iocore_detachout(0x07e9);
	iocore_detachinp(0x07e8);
	iocore_detachinp(0x07e9);
}

void npdisp_shutdown()
{
	npdisp_releaseScreen();
	npdispcs_shutdown();
}

// ---------- state save

int npdisp_sfsave(STFLAGH sfh, const SFENTRY* tbl)
{
	int	sfVersion = 2;
	int	ret = STATFLAG_SUCCESS;

	ret = statflag_write(sfh, &sfVersion, sizeof(int));
	if (ret != STATFLAG_SUCCESS) return ret;

	std::vector<UINT8> buffer;

	// 必要な範囲で記録
	// 共通
	UINT32 npdisplen = sizeof(npdisp);
	buffer.insert(buffer.end(), (UINT8*)(&npdisplen), (UINT8*)(&npdisplen + 1));
	buffer.insert(buffer.end(), (UINT8*)(&npdisp), (UINT8*)(&npdisp + 1));

	// WAB有効なら保存
	if (npdisp.enabled) {
		// OS依存部
		// スクリーン
		buffer.insert(buffer.end(), (UINT8*)(&npdispwin.bi), (UINT8*)(&npdispwin.bi + 1));
		UINT32 screenBufSize = npdispwin.stride * npdisp.height;
		if (npdispwin.pBits) {
			buffer.insert(buffer.end(), (UINT8*)(&screenBufSize), (UINT8*)(&screenBufSize + 1));
			buffer.insert(buffer.end(), (UINT8*)npdispwin.pBits, (UINT8*)npdispwin.pBits + screenBufSize);
		}
		else {
			screenBufSize = 0;
			buffer.insert(buffer.end(), (UINT8*)(&screenBufSize), (UINT8*)(&screenBufSize + 1));
		}
		// カーソル
		UINT32 cursorBufSize = npdisp.cursorStride * npdisp.cursorHeight;
		if (npdispwin.pBitsCursorMask && npdispwin.pBitsCursor) {
			buffer.insert(buffer.end(), (UINT8*)(&cursorBufSize), (UINT8*)(&cursorBufSize + 1));
			buffer.insert(buffer.end(), (UINT8*)npdispwin.pBitsCursorMask, (UINT8*)npdispwin.pBitsCursorMask + cursorBufSize);
			buffer.insert(buffer.end(), (UINT8*)npdispwin.pBitsCursor, (UINT8*)npdispwin.pBitsCursor + cursorBufSize);
		}
		else {
			cursorBufSize = 0;
			buffer.insert(buffer.end(), (UINT8*)(&cursorBufSize), (UINT8*)(&cursorBufSize + 1));
		}
		// パレット
		if (npdisp.bpp == 1) {
			buffer.insert(buffer.end(), (UINT8*)npdisp_palette_rgb2, (UINT8*)npdisp_palette_rgb2 + sizeof(npdisp_palette_rgb2));
		}
		else if (npdisp.bpp == 4) {
			buffer.insert(buffer.end(), (UINT8*)npdisp_palette_rgb16, (UINT8*)npdisp_palette_rgb16 + sizeof(npdisp_palette_rgb16));
		}
		else if (npdisp.bpp == 8) {
			buffer.insert(buffer.end(), (UINT8*)npdisp_palette_rgb256, (UINT8*)npdisp_palette_rgb256 + sizeof(npdisp_palette_rgb256));
			buffer.insert(buffer.end(), (UINT8*)npdisp_palette_transTbl, (UINT8*)npdisp_palette_transTbl + sizeof(npdisp_palette_transTbl));
		}
		// ペン
		buffer.insert(buffer.end(), (UINT8*)(&npdispwin.pensIdx), (UINT8*)(&npdispwin.pensIdx + 1));
		UINT32 penCount = npdispwin.pens.size();
		buffer.insert(buffer.end(), (UINT8*)(&penCount), (UINT8*)(&penCount + 1));
		for (auto it = npdispwin.pens.begin(); it != npdispwin.pens.end(); ++it) {
			buffer.insert(buffer.end(), (UINT8*)(&(it->first)), (UINT8*)(&(it->first) + 1));
			buffer.insert(buffer.end(), (UINT8*)(&(it->second)), (UINT8*)(&(it->second) + 1));
		}
		// ブラシ
		buffer.insert(buffer.end(), (UINT8*)(&npdispwin.brushesIdx), (UINT8*)(&npdispwin.brushesIdx + 1));
		UINT32 brushCount = npdispwin.brushes.size();
		buffer.insert(buffer.end(), (UINT8*)(&brushCount), (UINT8*)(&brushCount + 1));
		for (auto it = npdispwin.brushes.begin(); it != npdispwin.brushes.end(); ++it) {
			buffer.insert(buffer.end(), (UINT8*)(&(it->first)), (UINT8*)(&(it->first) + 1));
			buffer.insert(buffer.end(), (UINT8*)(&(it->second)), (UINT8*)(&(it->second) + 1));
		}
	}

	// 書き込み
	int statLen = buffer.size();
	ret = statflag_write(sfh, &statLen, sizeof(int));
	if (ret != STATFLAG_SUCCESS) return ret;
	if (statLen) {
		ret = statflag_write(sfh, &(buffer[0]), statLen);
		if (ret != STATFLAG_SUCCESS) return ret;
	}

	return(ret);
}

int npdisp_sfload(STFLAGH sfh, const SFENTRY* tbl)
{
	int	sfVersion = 0;
	int statLen = 0;
	int	ret = STATFLAG_SUCCESS;

	// 画面など解放
	npdisp_releaseScreen();

	ret = statflag_read(sfh, &sfVersion, sizeof(sfVersion));
	if (ret != STATFLAG_SUCCESS) return ret;
	ret = statflag_read(sfh, &statLen, sizeof(statLen));
	if (ret != STATFLAG_SUCCESS) return ret;
	if (statLen == 0) return STATFLAG_SUCCESS; // データ長さ0はバージョンに関係なくOK

	int readBufLen = 0;

	// 共通
	if (sfVersion == 1) {
		// ステートセーブ ver.1
		ret = statflag_read(sfh, &npdisp, sizeof(npdisp) - 1);
		if (ret != STATFLAG_SUCCESS) return ret;
		readBufLen += sizeof(npdisp) - 1;
		npdisp.active = npdisp.enabled;
	}
	else {
		// ステートセーブ ver.2以降
		UINT32 npdisplen = 0;
		ret = statflag_read(sfh, &npdisplen, sizeof(npdisplen));
		readBufLen += sizeof(npdisplen);
		if (ret != STATFLAG_SUCCESS) return ret;
		if (npdisplen < 0 || npdisplen > 32768) return STATFLAG_FAILURE; // 異常
		std::vector<UINT8> temp(npdisplen);
		ret = statflag_read(sfh, &(temp[0]), npdisplen);
		if (ret != STATFLAG_SUCCESS) return ret;
		readBufLen += npdisplen;
		memcpy(&npdisp, &(temp[0]), min(sizeof(npdisp), npdisplen));
	}

	// WAB有効なら読み込み
	if (npdisp.enabled) {
		if (sfVersion == 1 || sfVersion == 2)
		{
			// 画面など生成
			npdisp_createScreen();

			// OS依存部
			// スクリーン
			ret = statflag_read(sfh, &npdispwin.bi, sizeof(npdispwin.bi));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(npdispwin.bi);
			UINT32 screenBufSize;
			ret = statflag_read(sfh, &screenBufSize, sizeof(screenBufSize));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(screenBufSize);
			if (screenBufSize) {
				ret = statflag_read(sfh, npdispwin.pBits, screenBufSize);
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += screenBufSize;
			}
			// カーソル
			UINT32 cursorBufSize;
			ret = statflag_read(sfh, &cursorBufSize, sizeof(cursorBufSize));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(cursorBufSize);
			if (cursorBufSize) {
				// 読み取りと再生成
				void* pBitsCursorMask = (char*)malloc(cursorBufSize);
				if (!pBitsCursorMask) goto error;
				void* pBitsCursor = (char*)malloc(cursorBufSize);
				if (!pBitsCursor) {
					free(pBitsCursorMask);
					goto error;
				}

				ret = statflag_read(sfh, pBitsCursorMask, cursorBufSize);
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += cursorBufSize;
				ret = statflag_read(sfh, pBitsCursor, cursorBufSize);
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += cursorBufSize;

				HBITMAP hBmpCursorMask = CreateBitmap(npdisp.cursorWidth, npdisp.cursorHeight, 1, 1, pBitsCursorMask);
				if (hBmpCursorMask) {
					if (npdispwin.hBmpCursorMask) {
						SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursorMask);
						DeleteObject(npdispwin.hBmpCursorMask);
						npdispwin.hBmpCursorMask = NULL;
					}
					npdispwin.hOldBmpCursorMask = (HBITMAP)SelectObject(npdispwin.hdcCursorMask, hBmpCursorMask);
					npdispwin.hBmpCursorMask = hBmpCursorMask;
				}
				HBITMAP hBmpCursor = CreateBitmap(npdisp.cursorWidth, npdisp.cursorHeight, 1, 1, pBitsCursor);
				if (hBmpCursor) {
					if (npdispwin.hBmpCursor) {
						SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
						DeleteObject(npdispwin.hBmpCursor);
						npdispwin.hBmpCursor = NULL;
					}
					npdispwin.hOldBmpCursor = (HBITMAP)SelectObject(npdispwin.hdcCursor, hBmpCursor);
					npdispwin.hBmpCursor = hBmpCursor;
				}

				void* oldpBitsCursor = npdispwin.pBitsCursor;
				void* oldpBitsCursorMask = npdispwin.pBitsCursorMask;
				npdispwin.pBitsCursor = pBitsCursor;
				npdispwin.pBitsCursorMask = pBitsCursorMask;
				if (oldpBitsCursor) free(oldpBitsCursor);
				if (oldpBitsCursorMask) free(oldpBitsCursorMask);
			}
			// パレット
			if (npdisp.bpp == 1) {
				ret = statflag_read(sfh, npdisp_palette_rgb2, sizeof(npdisp_palette_rgb2));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(npdisp_palette_rgb2);
			}
			else if (npdisp.bpp == 4) {
				ret = statflag_read(sfh, npdisp_palette_rgb16, sizeof(npdisp_palette_rgb16));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(npdisp_palette_rgb16);
			}
			else if (npdisp.bpp == 8) {
				ret = statflag_read(sfh, npdisp_palette_rgb256, sizeof(npdisp_palette_rgb256));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(npdisp_palette_rgb256);
				ret = statflag_read(sfh, npdisp_palette_transTbl, sizeof(npdisp_palette_transTbl));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(npdisp_palette_transTbl);
			}
			// ペン
			ret = statflag_read(sfh, &npdispwin.pensIdx, sizeof(npdispwin.pensIdx));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(npdispwin.pensIdx);
			UINT32 penCount;
			ret = statflag_read(sfh, &penCount, sizeof(penCount));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(penCount);
			for (int i = 0; i < penCount; i++) {
				UINT32 key;
				NPDISP_HOSTPEN pen;
				ret = statflag_read(sfh, &key, sizeof(key));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(key);
				ret = statflag_read(sfh, &pen, sizeof(pen));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(pen);
				pen.pen = NULL; // statロードなので無効
				npdisp_createPen(&pen); // ブラシ生成
				npdispwin.pens[key] = pen;

			}
			// ブラシ
			ret = statflag_read(sfh, &npdispwin.brushesIdx, sizeof(npdispwin.brushesIdx));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(npdispwin.brushesIdx);
			UINT32 brushCount;
			ret = statflag_read(sfh, &brushCount, sizeof(brushCount));
			if (ret != STATFLAG_SUCCESS) goto error;
			readBufLen += sizeof(brushCount);
			for (int i = 0; i < brushCount; i++) {
				UINT32 key;
				NPDISP_HOSTBRUSH brush;
				ret = statflag_read(sfh, &key, sizeof(key));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(key);
				ret = statflag_read(sfh, &brush, sizeof(brush));
				if (ret != STATFLAG_SUCCESS) goto error;
				readBufLen += sizeof(brush);
				brush.brs = NULL; // statロードなので無効
				npdisp_createBrush(&brush); // ブラシ生成
				npdispwin.brushes[key] = brush;
			}

			if (readBufLen != statLen) goto error;

			// 読み込みバッファリセット
			int longjmpnum = npdisp.longjmpnum;
			npdisp_memory_clearpreload();
			npdisp.longjmpnum = longjmpnum; // 読み込み中例外のフラグは残す

			// 画面更新
			npdisp.paletteUpdated = 1;
			npdisp.updated = 1;
		}
		else
		{
			return(STATFLAG_FAILURE);
		}
	}
	return(ret);

error:

	npdisp_releaseScreen();
	return(STATFLAG_FAILURE);
}

#endif