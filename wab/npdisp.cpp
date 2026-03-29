/**
 * @file	npdisp.c
 * @brief	Implementation of the Neko Project II Display Adapter
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>

#include	"pccore.h"
#include	"wab.h"
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
#include	"npdisp_rle.h"
#include	"npdisp_mem.h"
#include	"npdisp_palette.h"

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
#if 0
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

static void npdisp_releaseScreen(void);
static void npdisp_createScreen(void);

NPDISP npdisp = { 0 };
NPDISP_WINDOWS npdispwin = { 0 };

static int npdisp_cs_initialized = 0;
static CRITICAL_SECTION npdisp_cs;
static CRITICAL_SECTION npdisp_cs_exception;
static int npdisp_cs_execflag = 0;

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


// パレット

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
	//memset(lpDevInfo, 0, sizeof(NPDISP_GDIINFO));
	
	lpDevInfo->dpVersion = 0x030A;
	lpDevInfo->dpTechnology = 1; // DT_RASDISPLAY
	lpDevInfo->dpHorzSize = 240 * npdisp.width / 640;
	lpDevInfo->dpVertSize = 150 * npdisp.height / 400;
	lpDevInfo->dpHorzRes = npdisp.width;
	lpDevInfo->dpVertRes = npdisp.height;
	lpDevInfo->dpNumBrushes = -1;
	lpDevInfo->dpNumPens = -1;// 16 * 5;
	lpDevInfo->futureuse = 0;
	lpDevInfo->dpNumFonts = 0;
	lpDevInfo->dpDEVICEsize = 64;// sizeof(NPDISP_PDEVICE);
	lpDevInfo->dpCurves = 0;
	lpDevInfo->dpLines = LC_POLYLINE | LC_STYLED;
	lpDevInfo->dpPolygonals = PC_SCANLINE;
	lpDevInfo->dpText = TC_RA_ABLE;// 0x0004 | 0x2000;
	lpDevInfo->dpClip = CP_RECTANGLE;
	lpDevInfo->dpRaster = RC_BITBLT | RC_BITMAP64 | RC_DI_BITMAP | RC_BIGFONT | RC_SAVEBITMAP | RC_DIBTODEV | RC_GDI20_OUTPUT | RC_OP_DX_OUTPUT; // 0x4699; // RC_BITBLT | RC_BITMAP64 | RC_SAVEBITMAP | RC_GDI20_OUTPUT | RC_DI_BITMAP;
	lpDevInfo->dpAspectX = 71;
	lpDevInfo->dpAspectY = 71;
	lpDevInfo->dpAspectXY = 100;
	lpDevInfo->dpStyleLen = lpDevInfo->dpAspectXY * 2;
	lpDevInfo->dpLogPixelsX = 96;
	lpDevInfo->dpLogPixelsY = 96;
	lpDevInfo->dpDCManage = 0x0004;
	lpDevInfo->dpMLoWin.x = lpDevInfo->dpHorzSize * 10;
	lpDevInfo->dpMLoWin.y = lpDevInfo->dpVertSize * 10;
	lpDevInfo->dpMLoVpt.x = npdisp.width;
	lpDevInfo->dpMLoVpt.y = -npdisp.height;
	lpDevInfo->dpMHiWin.x = lpDevInfo->dpHorzSize * 100;
	lpDevInfo->dpMHiWin.y = lpDevInfo->dpVertSize * 100;
	lpDevInfo->dpMHiVpt.x = npdisp.width;
	lpDevInfo->dpMHiVpt.y = -npdisp.height;
	lpDevInfo->dpELoWin.x = 375 * npdisp.width / 640;
	lpDevInfo->dpELoWin.y = 188 * npdisp.height / 400;
	lpDevInfo->dpELoVpt.x = 254 * npdisp.width / 640;
	lpDevInfo->dpELoVpt.y = -127 * npdisp.height / 400;
	lpDevInfo->dpEHiWin.x = 3750 * npdisp.width / 640;
	lpDevInfo->dpEHiWin.y = 1875 * npdisp.height / 400;
	lpDevInfo->dpEHiVpt.x = 254 * npdisp.width / 640;
	lpDevInfo->dpEHiVpt.y = -127 * npdisp.height / 400;
	lpDevInfo->dpTwpWin.x = 5400 * npdisp.width / 640;
	lpDevInfo->dpTwpWin.y = 2700 * npdisp.height / 400;
	lpDevInfo->dpTwpVpt.x = 254 * npdisp.width / 640;
	lpDevInfo->dpTwpVpt.y = -127 * npdisp.height / 400;

	//lpDevInfo->dpVersion = 0x030A;
	//lpDevInfo->dpTechnology = 1; // DT_RASDISPLAY
	//lpDevInfo->dpHorzRes = npdisp.width;
	//lpDevInfo->dpVertRes = npdisp.height;
	//lpDevInfo->dpNumBrushes = -1;
	//lpDevInfo->dpNumPens = -1;
	//lpDevInfo->dpNumFonts = 0;
	//lpDevInfo->dpDEVICEsize = sizeof(NPDISP_PDEVICE);
	//lpDevInfo->dpCurves = 0;
	//lpDevInfo->dpLines = 0;
	//lpDevInfo->dpPolygonals = 0;
	//lpDevInfo->dpText = 0;
	//lpDevInfo->dpClip = 0;
	//lpDevInfo->dpRaster = 0x0001 | 0x0008 | 0x0200;// | 0x0080; // RC_BITBLT | RC_BITMAP64 | RC_DIBTODEV | RC_DI_BITMAP
	//lpDevInfo->dpAspectX = 71;
	//lpDevInfo->dpAspectY = 71;
	//lpDevInfo->dpAspectXY = 100;
	//lpDevInfo->dpStyleLen = lpDevInfo->dpAspectXY * 2;
	//lpDevInfo->dpLogPixelsX = 96;
	//lpDevInfo->dpLogPixelsY = 96;
	//lpDevInfo->dpHorzSize = npdisp.width * 254 / lpDevInfo->dpLogPixelsX / 10;
	//lpDevInfo->dpVertSize = npdisp.height * 254 / lpDevInfo->dpLogPixelsY / 10;
	//lpDevInfo->dpDCManage = 0x0004;
	//lpDevInfo->dpMLoWin.x = 254;
	//lpDevInfo->dpMLoWin.y = 254;
	//lpDevInfo->dpMLoVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpMLoVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpMHiWin.x = 2540;
	//lpDevInfo->dpMHiWin.y = 2540;
	//lpDevInfo->dpMHiVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpMHiVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpELoWin.x = 100;
	//lpDevInfo->dpELoWin.y = 100;
	//lpDevInfo->dpELoVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpELoVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpEHiWin.x = 1000;
	//lpDevInfo->dpEHiWin.y = 1000;
	//lpDevInfo->dpEHiVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpEHiVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpTwpWin.x = 1440;
	//lpDevInfo->dpTwpWin.y = 1440;
	//lpDevInfo->dpTwpVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpTwpVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;

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
		lpDevInfo->dpNumColors = 256; // 20;
		// lpDevInfo->dpRaster |= 0x0100; // RC_PALETTE
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

	return sizeof(NPDISP_GDIINFO); // ドキュメントに書かれていないがサイズを返さないと駄目
}

static UINT32 npdisp_func_ColorInfo(NPDISP_PDEVICE* lpDestDev, UINT32 dwColorin, UINT32* lpPColor)
{
	int idx;
	UINT32 rgb;

	if (npdisp.bpp > 8) {
		*lpPColor = dwColorin;
		rgb = dwColorin;
	}
	else if (npdisp.bpp == 1) {
		*lpPColor = dwColorin;
		rgb = dwColorin;
	}
	else {
		if (lpPColor) {
			// 論理カラー値を最も近い物理デバイスカラー値へ変換　dwColorinは論理カラー値（RGB値）
			UINT8 r, g, b;
			r = (UINT8)(dwColorin & 0xFF);
			g = (UINT8)((dwColorin >> 8) & 0xFF);
			b = (UINT8)((dwColorin >> 16) & 0xFF);
			if (npdisp.bpp == 8) {
				idx = npdisp_FindNearest256(r, g, b);
			}
			else if (npdisp.bpp == 4) {
				idx = npdisp_FindNearest16(r, g, b);
			}
			else {
				//if (npdisp.bpp == 1 && dwColorin == 0xee) {
				//	*lpPColor = 0x01eeeeee;
				//	return 0x00000000;
				//}
				idx = npdisp_FindNearest2(r, g, b);
			}
			*lpPColor = (UINT32)idx;
		}
		else {
			// 物理デバイスカラー値を論理カラー値へ変換　dwColorinは物理デバイスカラー値（パレット番号など）
			idx = dwColorin;
			if (idx < 0 || (1 << npdisp.bpp) <= idx) {
				return 0;
			}
		}

		// 求めたカラー値のRGB表現
		if (npdisp.bpp == 2) {
			// 2色パレットセット
			rgb = ((UINT32)npdisp_palette_rgb2[idx].r) | ((UINT32)npdisp_palette_rgb2[idx].g << 8) | ((UINT32)npdisp_palette_rgb2[idx].b << 16);
		}
		else if (npdisp.bpp == 4) {
			// 16色パレットセット
			rgb = ((UINT32)npdisp_palette_rgb16[idx].r) | ((UINT32)npdisp_palette_rgb16[idx].g << 8) | ((UINT32)npdisp_palette_rgb16[idx].b << 16);
		}
		else if (npdisp.bpp == 8) {
			// 256色パレットセット
			rgb = ((UINT32)npdisp_palette_rgb256[idx].r) | ((UINT32)npdisp_palette_rgb256[idx].g << 8) | ((UINT32)npdisp_palette_rgb256[idx].b << 16);
		}
	}

	return rgb;
}

void npdisp_exec(void) {
	// 読み書き開始位置を先頭へ戻す
	npdisp_memory_resetposition();

	UINT16 version = npdisp_readMemory16(npdisp.dataAddr); // バージョンだけ取得
	npdispcs_enter_criticalsection();
	npdisp_cs_execflag = 1;

	//CPU_REMCLOCK -= 60000;
	if (version >= 1) {
		UINT16 retCode = NPDISP_RETCODE_SUCCESS;
		NPDISP_REQUEST req;
		npdisp_readMemory(&req, npdisp.dataAddr, sizeof(req)); // 全体読み込み
		switch (req.funcOrder) {
		case NPDISP_FUNCORDER_NP2INITIALIZE:
		{
			// 初期化
			npdisp.enabled = 0;
			np2wab.relaystateext = 0;
			np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			npdisp_releaseScreen();

			if (req.parameters.init.width) npdisp.width = req.parameters.init.width;
			if (req.parameters.init.height) npdisp.height = req.parameters.init.height;
			if (npdisp.width > WAB_MAX_WIDTH) npdisp.width = WAB_MAX_WIDTH;
			if (npdisp.height > WAB_MAX_HEIGHT) npdisp.height = WAB_MAX_HEIGHT;
			if (npdisp.width < 160) npdisp.width = 160;
			if (npdisp.height < 100) npdisp.height = 100;
			if (req.parameters.init.bpp) npdisp.bpp = req.parameters.init.bpp;
			if (npdisp.bpp <= 1) npdisp.bpp = 1;
			else if (npdisp.bpp <= 4) npdisp.bpp = 4;
			else if (npdisp.bpp <= 8) npdisp.bpp = 8;
			else if (npdisp.bpp <= 16) npdisp.bpp = 16;
			else if (npdisp.bpp <= 24) npdisp.bpp = 24;
			else if (npdisp.bpp <= 32) npdisp.bpp = 32;
			if (req.parameters.init.dpiX) npdisp.dpiX = req.parameters.init.dpiX;
			if (req.parameters.init.dpiY) npdisp.dpiY = req.parameters.init.dpiY;

			version = 1; // バージョンを返す
			npdisp_writeMemory16(version, npdisp.dataAddr);
			break;
		}
		case NPDISP_FUNCORDER_Enable:
		{
			UINT16 retValue = 0;
			TRACEOUT(("Enable"));
			retValue = npdisp_readMemory16(req.parameters.enable.lpRetValueAddr);
			if (req.parameters.enable.lpDevInfoAddr) {
				char* lpDestDevType;
				char* lpOutputFile;
				NPDISP_DEVMODE data;
				lpDestDevType = npdisp_readMemoryString(req.parameters.enable.lpDestDevTypeAddr);
				lpOutputFile = npdisp_readMemoryString(req.parameters.enable.lpOutputFileAddr);
				if (req.parameters.enable.lpDataAddr) {
					npdisp_readMemory(&data, req.parameters.enable.lpDataAddr, sizeof(data));
				}
				switch (req.parameters.enable.wStyle & 0x7fff) {
				case 0:
				{
					NPDISP_PDEVICE devInfo;
					npdisp_readMemory(&devInfo, req.parameters.enable.lpDevInfoAddr, sizeof(devInfo));
					retValue = npdisp_func_Enable_PDEVICE(&devInfo, req.parameters.enable.wStyle, lpDestDevType, lpOutputFile, req.parameters.enable.lpDataAddr ? &data : NULL);
					npdisp_writeMemory(&devInfo, req.parameters.enable.lpDevInfoAddr, 2);
					npdisp_createScreen();
					npdisp.enabled = 1;
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
					npdisp_readMemory(&gdiInfo, req.parameters.enable.lpDevInfoAddr, sizeof(gdiInfo));
					retValue = npdisp_func_Enable_GDIINFO(&gdiInfo, req.parameters.enable.wStyle, lpDestDevType, lpOutputFile, req.parameters.enable.lpDataAddr ? &data : NULL);
					npdisp_writeMemory(&gdiInfo, req.parameters.enable.lpDevInfoAddr, sizeof(gdiInfo));
					TRACEOUT(("Enable GDIINFO"));
					break;
				}
				}
				if (lpDestDevType) free(lpDestDevType);
				if (lpOutputFile) free(lpOutputFile);
			}
			npdisp_writeMemory16(retValue, req.parameters.enable.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Disable:
		{
			TRACEOUT(("Disable"));
			if (req.parameters.disable.lpDestDevAddr) {
				NPDISP_PDEVICE destDev;
				npdisp_readMemory(&destDev, req.parameters.disable.lpDestDevAddr, sizeof(destDev));
				req.parameters.disable.lpDestDevAddr = 0;
				npdisp.enabled = 0;
				np2wab.relaystateext = 0;
				np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			}
			break;
		}
		case NPDISP_FUNCORDER_GetDriverResourceID:
		{
			TRACEOUT(("GetDriverResourceID"));
			// DPI毎のリソース変換？
			SINT16 iResId = req.parameters.GetDriverResourceID.iResId;
			if (req.parameters.GetDriverResourceID.lpResTypeAddr) {
				if (req.parameters.GetDriverResourceID.lpResTypeAddr & 0xffff0000) {
					char* lpResType;
					lpResType = npdisp_readMemoryString(req.parameters.GetDriverResourceID.lpResTypeAddr);
					if (lpResType) free(lpResType);
				}
				else {
					// 上位が0の時はただの値
					SINT16 iResType = req.parameters.GetDriverResourceID.lpResTypeAddr;
				}
			}
			if (npdisp.dpiX >= 96 && (iResId > 32647 || iResId == 1 || iResId == 3)) {
				iResId += 2000;
			}
			npdisp_writeMemory16(iResId, req.parameters.GetDriverResourceID.lpRetValueAddr);
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
			npdisp_readMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
			retValue = npdisp_func_ColorInfo(&devInfo, req.parameters.ColorInfo.dwColorin, &pcolor);
			npdisp_writeMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
			npdisp_writeMemory32(retValue, req.parameters.ColorInfo.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_RealizeObject:
		{
			TRACEOUT(("RealizeObject"));
			// オブジェクト生成と破棄
			UINT32 retValue = 0;
			NPDISP_PDEVICE destDev;
			npdisp_readMemory(&destDev, req.parameters.RealizeObject.lpDestDevAddr, sizeof(destDev));
			if ((SINT16)req.parameters.RealizeObject.wStyle < 0) {
				// 削除
				retValue = 1; // 常に成功したことにする
				switch (-((SINT16)req.parameters.RealizeObject.wStyle)) {
				case 1: // OBJ_PEN
				{
					if (req.parameters.RealizeObject.lpInObjAddr) {
						// 指定されたキーのペンを削除
						NPDISP_PEN pen = { {0}, {NPDISP_PEN_STYLE_SOLID, {1, 0}, 0} };
						npdisp_readMemory(&pen, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_PEN));
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
						//npdisp_writeMemory(&pen, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_PEN));
					}
					TRACEOUT(("RealizeObject Release OBJ_PEN"));
					// サイズを返す
					retValue = sizeof(NPDISP_PEN);
					break;
				}
				case 2: // OBJ_BRUSH
				{
					if (req.parameters.RealizeObject.lpInObjAddr) {
						// 指定されたキーのブラシを削除
						NPDISP_BRUSH brush = { {0}, {NPDISP_BRUSH_STYLE_SOLID, 15, NPDISP_BRUSH_HATCH_HORIZONTAL, 15} };
						npdisp_readMemory(&brush, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_BRUSH));
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
						//npdisp_writeMemory(&brush, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_BRUSH));
					}
					TRACEOUT(("RealizeObject Release OBJ_BRUSH"));
					// サイズを返す
					retValue = sizeof(NPDISP_BRUSH);
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
				switch (req.parameters.RealizeObject.wStyle) {
				case 1: // OBJ_PEN
				{
					TRACEOUT(("RealizeObject Create OBJ_PEN"));
					if (req.parameters.RealizeObject.lpOutObjAddr) {
						// 作成
						NPDISP_PEN pen = { {0}, {NPDISP_PEN_STYLE_SOLID, {1, 0}, 0} };
						NPDISP_HOSTPEN hostpen = { 0 };
						if (req.parameters.RealizeObject.lpInObjAddr) {
							// 指定した設定で作る
							npdisp_readMemory(&(pen.lpen), req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_LPEN));
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
							hostpen.pen = CreatePen(pen.lpen.opnStyle, pen.lpen.lopnWidth.x, NPDISP_ADJUST_MONOCOLOR(pen.lpen.lopnColor));
							hostpen.refCount = 1;
							pen.key = npdispwin.pensIdx;
							npdispwin.pensIdx++;
							if (npdispwin.pensIdx == 0) npdispwin.pensIdx++; // 0は使わないことにする
							npdispwin.pens[pen.key] = hostpen;
						}

						// 書き込み
						npdisp_writeMemory(&pen, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_PEN));
					}
					// サイズを返す
					retValue = sizeof(NPDISP_PEN);
					break;
				}
				case 2: // OBJ_BRUSH
				{
					TRACEOUT(("RealizeObject Create OBJ_BRUSH"));
					if (req.parameters.RealizeObject.lpOutObjAddr) {
						// 作成
						NPDISP_BRUSH brush = { {0}, {NPDISP_BRUSH_STYLE_SOLID, 15, NPDISP_BRUSH_HATCH_HORIZONTAL, 15} };
						NPDISP_HOSTBRUSH hostbrush = { 0 };
						if (req.parameters.RealizeObject.lpInObjAddr) {
							// 指定した設定で作る
							npdisp_readMemory(&(brush.lbrush), req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_LBRUSH));
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
							if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_SOLID) {
								// 単色ブラシ生成
								if (brush.lbrush.lbColor & 0xff000000) {
									// パレットカラー
									hostbrush.brs = CreateSolidBrush(NPDISP_ADJUST_MONOCOLOR(brush.lbrush.lbColor));
								}
								else {
									// RGBカラー
									int physicalColor = npdisp_FindNearestColorUINT32(brush.lbrush.lbColor);
									if (physicalColor == brush.lbrush.lbColor) {
										// 純色
										hostbrush.brs = CreateSolidBrush(NPDISP_ADJUST_MONOCOLOR(brush.lbrush.lbColor));
									}
									else {
										// ディザ
										hostbrush.brs = CreatePaletteDitherBrush(brush.lbrush.lbColor);
									}
								}
								if (!hostbrush.brs) {
									TRACEOUT2(("RealizeObject Create OBJ_BRUSH SOLID ERROR!!!!!!!!!!!!!!"));
								}
								TRACEOUT((" -> Style:%d, Color:%08x", brush.lbrush.lbStyle, brush.lbrush.lbColor));
							}
							else if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
								// ハッチブラシ生成
								hostbrush.brs = CreateHatchBrush(brush.lbrush.lbHatch, NPDISP_ADJUST_MONOCOLOR(brush.lbrush.lbColor));
								if (!hostbrush.brs) {
									TRACEOUT2(("RealizeObject Create OBJ_BRUSH HATCHED ERROR!!!!!!!!!!!!!!"));
								}
								TRACEOUT((" -> Style:%d, Color:%08x", brush.lbrush.lbStyle, brush.lbrush.lbColor));
							}
							else if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
								// パターンブラシ生成
								NPDISP_PBITMAP patternBmp = { 0 };
								if (npdisp_readMemory(&patternBmp, brush.lbrush.lbColor, sizeof(patternBmp))) {
									NPDISP_WINDOWS_BMPHDC patternBmphdc = { 0 };
									if (npdisp_MakeBitmapFromPBITMAP(&patternBmp, &patternBmphdc, 0)) {
										if (patternBmp.bmHeight < 0) patternBmp.bmHeight = -patternBmp.bmHeight;
										HBITMAP hPatBmp = CreateBitmap(8, 8, 1, 1, NULL); // DDBでないとパターンにできない？
										HDC hdcPat = npdispwin.hdcCache[1];
										HGDIOBJ hOldBmp = SelectObject(hdcPat, hPatBmp);
										BitBlt(hdcPat, 0, 0, 8, 8, patternBmphdc.hdc, 0, 0, SRCCOPY);
										hostbrush.brs = CreatePatternBrush(hPatBmp);
										//RECT rrr = {0, 0, 300, 300};
										//FillRect(npdispwin.hdc, &rrr, hostbrush.brs);
										SelectObject(hdcPat, hOldBmp);
										DeleteObject(hPatBmp);
										npdisp_FreeBitmap(&patternBmphdc);

										//RECT rrr = {0, 0, 300, 300};
										//SetBkColor(npdispwin.hdc, 0x008000);
										//SetTextColor(npdispwin.hdc, 0x0080ff);
										////FillRect(npdispwin.hdc, &rrr, hostbrush.brs);
										//{
										//	//PAINTSTRUCT ps;
										//	//HBRUSH hBrush = CreateHatchBrush(HS_FDIAGONAL, RGB(255, 0, 0));
										//	//SetTextColor(npdispwin.hdc, RGB(0, 128, 255));
										//	//SetBkColor(npdispwin.hdc, RGB(0, 128, 88));
										//	//SetBkMode(npdispwin.hdc, OPAQUE);
										//	//SetROP2(npdispwin.hdc, R2_COPYPEN);
										//	FillRect(npdispwin.hdc, &rrr, hostbrush.brs);
										//	//DeleteObject(hBrush);
										//}
									}
									else {
										hostbrush.brs = CreateSolidBrush(NPDISP_ADJUST_MONOCOLOR(brush.lbrush.lbColor));
									}
								}
								else {
									hostbrush.brs = CreateSolidBrush(NPDISP_ADJUST_MONOCOLOR(brush.lbrush.lbColor));
								}
								if (!hostbrush.brs) {
									TRACEOUT2(("RealizeObject Create OBJ_BRUSH PATTERN ERROR!!!!!!!!!!!!!!"));
								}
								TRACEOUT((" -> Style:%d, Pattern Addr:%08x", brush.lbrush.lbStyle, brush.lbrush.lbColor));
							}
							else if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HOLLOW) {
								// 何もしないブラシ生成
								hostbrush.brs = NULL;
							}
							hostbrush.refCount = 1;
							brush.key = npdispwin.brushesIdx;
							npdispwin.brushesIdx++;
							if (npdispwin.brushesIdx == 0) npdispwin.brushesIdx++; // 0は使わないことにする
							npdispwin.brushes[brush.key] = hostbrush;
						}
						// 書き込み
						npdisp_writeMemory(&brush, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_BRUSH));
					}
					// サイズを返す
					retValue = sizeof(NPDISP_BRUSH);
					break;
				}
				case 3: // OBJ_FONT
				{
					TRACEOUT(("RealizeObject Create OBJ_FONT"));
					//if (req.parameters.RealizeObject.lpOutObjAddr) {
					//	// 作成
						// 失敗ということにして0を返す
					retValue = 0;
					//}
					//else {
					//	// サイズを返す
					//	retValue = sizeof(NPDISP_FONT);
					//}
					break;
				}
				case 5: // OBJ_PBITMAP
				{
					TRACEOUT(("RealizeObject Create OBJ_PBITMAP"));
					//if (req.parameters.RealizeObject.lpOutObjAddr) {
					//	// 作成
					//	NPDISP_PBITMAP inBmp = { 0 };
					//	NPDISP_PBITMAP outBmp = { 0 };
					//	if (req.parameters.RealizeObject.lpInObjAddr) {
					//		npdisp_readMemory(&inBmp, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_PBITMAP));
					//		npdisp_readMemory(&outBmp, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_PBITMAP));
					//	}
					//	// 書き込み
					//	npdisp_writeMemory(&inBmp, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_PBITMAP));
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
			npdisp_writeMemory32(retValue, req.parameters.RealizeObject.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Control:
		{
			TRACEOUT(("Control"));
			UINT16 retValue = 0;
			//if (req.parameters.Control.lpDestDevAddr) {
			//	NPDISP_PDEVICE destDev;
			//	npdisp_readMemory(&destDev, req.parameters.Control.lpDestDevAddr, sizeof(destDev));
			//	if (req.parameters.Control.wFunction == 8) {
			//		// QUERYESCSUPPORT
			//		UINT16 escNum = npdisp_readMemory16(req.parameters.Control.lpInDataAddr);
			//		if (escNum == 8) retValue = 1; // QUERYESCSUPPORTは必ずサポート
			//	}
			//	// 必要ならサポート
			//}
			npdisp_writeMemory16(retValue, req.parameters.Control.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_BitBlt:
		{
			//TRACEOUT(("BitBlt"));
			int dstDevType = 0;
			int srcDevType = 0;
			int hasDstDev = 0;
			int hasSrcDev = 0;
			UINT16 retValue = 0;
			if (req.parameters.BitBlt.lpDestDevAddr) {
				if (npdisp_readMemory(&dstDevType, req.parameters.BitBlt.lpDestDevAddr, 2)) {
					hasDstDev = 1;
				}
			}
			if (req.parameters.BitBlt.lpSrcDevAddr) {
				if (npdisp_readMemory(&srcDevType, req.parameters.BitBlt.lpSrcDevAddr, 2)) {
					hasSrcDev = 1;
				}
			}
			if (npdisp.longjmpnum == 0) {
				int srcBeginLine = req.parameters.BitBlt.wSrcY;
				int srcNumLines = req.parameters.BitBlt.wYext;
				int dstBeginLine = req.parameters.BitBlt.wDestY;
				int dstNumLines = req.parameters.BitBlt.wYext;
				if (srcBeginLine < 0) {
					srcNumLines += srcBeginLine;
					srcBeginLine = 0;
				}
				if (dstBeginLine < 0) {
					dstNumLines += dstBeginLine;
					dstBeginLine = 0;
				}
				if (dstDevType != 0 && srcDevType != 0) {
					// VRAM -> VRAM
					TRACEOUT_BITBLT(("BitBlt VRAM -> VRAM DEST X:%d Y:%d W:%d H:%d)", req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext));
					if (req.parameters.BitBlt.lpPBrushAddr) {
						// ブラシがあれば選択
						NPDISP_BRUSH brush = { 0 };
						if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
							if (brush.key != 0) {
								auto it = npdispwin.brushes.find(brush.key);
								if (it != npdispwin.brushes.end()) {
									NPDISP_HOSTBRUSH value = it->second;
									if (value.brs) {
										if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
											TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
										}
										NPDISP_DRAWMODE drawMode = { 0 };
										int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
										if (npdisp.bpp == 1) {
											drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
											drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
										}
										SelectObject(npdispwin.hdc, value.brs);
										if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
											SetBkColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
										}
										if (hasDrawMode) {
											SetBkColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
											SetTextColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
											if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
											}
											SetBkMode(npdispwin.hdc, drawMode.bkMode);
											SetROP2(npdispwin.hdc, drawMode.Rop2);
										}
									}
								}
							}
						}
					}
					if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
						TRACEOUT2(("BitBlt Check VRAM => VRAM ROP:%08x", req.parameters.BitBlt.Rop3));
					}
					int srcX = req.parameters.BitBlt.wSrcX;
					int srcY = req.parameters.BitBlt.wSrcY;
					int destX = req.parameters.BitBlt.wDestX;
					int destY = req.parameters.BitBlt.wDestY;
					int w = req.parameters.BitBlt.wXext;
					int h = req.parameters.BitBlt.wYext;
					if (!(destX >= srcX + w || destX + w <= srcX || destY >= srcY + h || destY + h <= srcY))
					{
						// 重なっているのでバッファ経由
						BitBlt(npdispwin.hdcBltBuf, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, SRCCOPY);
						BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdcBltBuf, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.Rop3);
					}
					else {
						// 重なっていないので直接転送
						BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
					}
					SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
					npdisp.updated = 1;
					retValue = 1; // 成功
				}
				else if (dstDevType != 0 && srcDevType == 0) {
					// MEM -> VRAM
					TRACEOUT_BITBLT(("BitBlt MEM -> VRAM DEST X:%d Y:%d W:%d H:%d)", req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext));
					NPDISP_PBITMAP srcPBmp;
					if (req.parameters.BitBlt.lpSrcDevAddr && npdisp_readMemory(&srcPBmp, req.parameters.BitBlt.lpSrcDevAddr, sizeof(NPDISP_PBITMAP))) {
						NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
						npdisp_PreloadBitmapFromPBITMAP(&srcPBmp, 0, srcBeginLine, srcNumLines);
						if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&srcPBmp, &bmphdc, 0, srcBeginLine, srcNumLines)) {
							//if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
								TRACEOUT2(("BitBlt Check %08x => VRAM ROP:%08x", req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.Rop3));
							//}
							if (req.parameters.BitBlt.lpPBrushAddr) {
								// ブラシがあれば選択
								NPDISP_BRUSH brush = { 0 };
								if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
									if (brush.key != 0) {
										auto it = npdispwin.brushes.find(brush.key);
										if (it != npdispwin.brushes.end()) {
											NPDISP_HOSTBRUSH value = it->second;
											if (value.brs) {
												if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
													TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
												}
												NPDISP_DRAWMODE drawMode = { 0 };
												int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												if (npdisp.bpp == 1) {
													drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
													drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
												}
												SelectObject(npdispwin.hdc, value.brs);
												if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
													SetBkColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
												}
												if (hasDrawMode) {
													SetBkColor(npdispwin.hdc, drawMode.LbkColor);
													SetTextColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
													if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
													}
													SetBkMode(npdispwin.hdc, drawMode.bkMode);
													SetROP2(npdispwin.hdc, drawMode.Rop2);
												}
											}
										}
									}
								}
							}
							switch (req.parameters.BitBlt.Rop3) {
							case 0x00CC0020: // SRCCOPY
							case 0x00EE0086: // SRCPAINT
							case 0x008800C6: // SRCAND
							case 0x00660046: // SRCINVERT
							case 0x00440328: // SRCERASE 
							case 0x00330008: // NOTSRCCOPY
							case 0x001100A6: // NOTSRCERASE
							case 0x00C000CA: // MERGECOPY
							case 0x00BB0226: // MERGEPAINT
							case 0x00F00021: // PATCOPY 
							case 0x00FB0A09: // PATPAINT 
							case 0x005A0049: // PATINVERT 
							case 0x00550009: // DSTINVERT
								if (hasDstDev) {
									//BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
									//if (req.parameters.BitBlt.lpSrcDevAddr == 0x112f0000) {
									//	//char test[16];
									//	BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
									//	//sprintf(test, "%08x", req.parameters.BitBlt.lpSrcDevAddr);
									//	//SetBkColor(npdispwin.hdc, 0xffffff);
									//	//SetTextColor(npdispwin.hdc, 0x000000);
									//	//SetBkMode(npdispwin.hdc, OPAQUE);
									//	//TextOutA(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, test, strlen(test));
									//}
									//else {
										BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);

									//}
									//if (req.parameters.BitBlt.lpSrcDevAddr == 0x1f870000) {
									//	BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
									//}
								}
								break;
							case 0x00000042: // BLACKNESS
								if (hasDstDev) {
									BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
								}
								break;
							case 0x00FF0062: // WHITENESS
								if (hasDstDev) {
									BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
								}
								break;
							default:
								if (hasDstDev) {
									BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
								}
								break;
							}
							SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
							npdisp.updated = 1;
							retValue = 1; // 成功

							npdisp_FreeBitmap(&bmphdc);
						}
					}
					else if (req.parameters.BitBlt.lpPBrushAddr) {
						NPDISP_BRUSH brush = { 0 };
						TRACEOUT_BITBLT(("-> BRUSH"));
						if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
							if (brush.key != 0) {
								auto it = npdispwin.brushes.find(brush.key);
								if (it != npdispwin.brushes.end()) {
									NPDISP_HOSTBRUSH value = it->second;
									if (value.brs) {
										if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
											TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
										}
										NPDISP_DRAWMODE drawMode = { 0 };
										int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
										SelectObject(npdispwin.hdc, value.brs);
										if (npdisp.bpp == 1) {
											if (req.parameters.BitBlt.wXext == 240 && req.parameters.BitBlt.wYext == 81) {
												TRACEOUT_BITBLT(("CHECK"));
											}
											drawMode.LbkColor = drawMode.bkColor ? 0xffffff : 0;
											drawMode.LTextColor = drawMode.TextColor ? 0xffffff : 0;
											//if (req.parameters.BitBlt.wXext == 237 && req.parameters.BitBlt.wYext == 140) {
											//	req.parameters.BitBlt.wYext = 100;
											//}
											if (req.parameters.BitBlt.Rop3 == PATCOPY) {
												//if (drawMode.LbkColor) {
													//SelectObject(npdispwin.hdc, GetStockObject(WHITE_BRUSH));
												//}
												//else {
												//	SelectObject(npdispwin.hdc, GetStockObject(BLACK_BRUSH));
												//}
												//if (drawMode.bkColor == 0x01eeeeee) {
												//	SelectObject(npdispwin.hdc, npdispwin.hEEBrush);
												//}
												//SelectObject(npdispwin.hdc, GetStockObject(BLACK_BRUSH));
												//SelectObject(npdispwin.hdc, GetStockObject(WHITE_BRUSH));
											//}
											//else if (req.parameters.BitBlt.Rop3 == PATINVERT) {
											//	req.parameters.BitBlt.Rop3 = PATCOPY;
											//	SelectObject(npdispwin.hdc, GetStockObject(WHITE_BRUSH));
											//	//SelectObject(npdispwin.hdc, GetStockObject(WHITE_BRUSH));
											}
										}
										if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
											SetBkColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
										}
										if (hasDrawMode) {
											SetBkColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
											SetTextColor(npdispwin.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
											//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
											//}
											SetBkMode(npdispwin.hdc, drawMode.bkMode);
											SetROP2(npdispwin.hdc, drawMode.Rop2);
										}
										if (npdisp.bpp == 1) {
											//if (req.parameters.BitBlt.Rop3 == PATCOPY) {
												PatBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3);
											//}
											//else {
											//	BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, 0, 0, DSTINVERT);
											//}
										}
										else {
											//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
											PatBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3);
											//}
											//else {
											//	Rectangle(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wDestX + req.parameters.BitBlt.wXext, req.parameters.BitBlt.wDestY + req.parameters.BitBlt.wYext);
											//}
											//BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
										}
										npdisp.updated = 1;

										SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
									}
								}
							}
						}
					}
					retValue = 1; // 成功
				}
				else if (dstDevType == 0 && srcDevType != 0) {
					// VRAM -> MEM
					TRACEOUT_BITBLT(("BitBlt VRAM -> MEM DEST X:%d Y:%d W:%d H:%d)", req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext));
					NPDISP_PBITMAP dstPBmp;
					if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
						NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
						npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0, dstBeginLine, dstNumLines);
						if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0, dstBeginLine, dstNumLines)) {
							if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
								TRACEOUT2(("BitBlt Check VRAM BRUSH -> %08x", req.parameters.BitBlt.lpDestDevAddr));
							}
							if (req.parameters.BitBlt.lpPBrushAddr) {
								// ブラシがあれば選択
								NPDISP_BRUSH brush = { 0 };
								if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
									if (brush.key != 0) {
										auto it = npdispwin.brushes.find(brush.key);
										if (it != npdispwin.brushes.end()) {
											NPDISP_HOSTBRUSH value = it->second;
											if (value.brs) {
												if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
													TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
												}
												NPDISP_DRAWMODE drawMode = { 0 };
												int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												if (npdisp.bpp == 1) {
													drawMode.LbkColor = drawMode.bkColor ? 0xffffff : 0;
													drawMode.LTextColor = drawMode.TextColor ? 0xffffff : 0;
													if (req.parameters.BitBlt.Rop3 == PATCOPY) {
														if (drawMode.LbkColor) {
															SelectObject(bmphdc.hdc, GetStockObject(WHITE_BRUSH));
														}
														else {
															SelectObject(bmphdc.hdc, GetStockObject(BLACK_BRUSH));
														}
													}
												}
												SelectObject(bmphdc.hdc, value.brs);
												if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
													SetBkColor(bmphdc.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
												}
												if (hasDrawMode) {
													SetBkColor(bmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
													SetTextColor(bmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
													if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
													}
													SetBkMode(bmphdc.hdc, drawMode.bkMode);
													SetROP2(bmphdc.hdc, drawMode.Rop2);
												}
											}
										}
									}
								}
							}
							switch (req.parameters.BitBlt.Rop3) {
							case 0x00CC0020: // SRCCOPY
							case 0x00EE0086: // SRCPAINT
							case 0x008800C6: // SRCAND
							case 0x00660046: // SRCINVERT
							case 0x00440328: // SRCERASE 
							case 0x00330008: // NOTSRCCOPY
							case 0x001100A6: // NOTSRCERASE
							case 0x00C000CA: // MERGECOPY
							case 0x00BB0226: // MERGEPAINT
							case 0x00F00021: // PATCOPY 
							case 0x00FB0A09: // PATPAINT 
							case 0x005A0049: // PATINVERT 
							case 0x00550009: // DSTINVERT
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
								}
								break;
							case 0x00000042: // BLACKNESS
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
								}
								break;
							case 0x00FF0062: // WHITENESS
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
								}
								break;
							default:
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
								}
								break;
							}
							SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
							npdisp.updated = 1;
							retValue = 1; // 成功

							npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc, dstBeginLine, dstNumLines);

							npdisp_FreeBitmap(&bmphdc);
						}
					}
					retValue = 1; // 成功
				}
				else if (dstDevType == 0 && srcDevType == 0) {
					// MEM -> MEM
					TRACEOUT_BITBLT(("BitBlt MEM -> MEM DEST X:%d Y:%d W:%d H:%d)", req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext));
					NPDISP_PBITMAP dstPBmp;
					retValue = 1;
					if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
						if (req.parameters.BitBlt.lpSrcDevAddr) {
							NPDISP_PBITMAP srcPBmp;
							if (npdisp_readMemory(&srcPBmp, req.parameters.BitBlt.lpSrcDevAddr, sizeof(NPDISP_PBITMAP))) {
								npdisp_PreloadBitmapFromPBITMAP(&srcPBmp, 0, srcBeginLine, srcNumLines);
								npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 1, dstBeginLine, dstNumLines);
								NPDISP_WINDOWS_BMPHDC srcbmphdc = { 0 };
								if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&srcPBmp, &srcbmphdc, 0, srcBeginLine, srcNumLines)) {
									NPDISP_WINDOWS_BMPHDC dstbmphdc = { 0 };
									if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &dstbmphdc, 1, dstBeginLine, dstNumLines)) {
										if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
											//BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
											TRACEOUT2(("BitBlt Check %08x -> %08x ROP:%08x", req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.lpDestDevAddr, req.parameters.BitBlt.Rop3));
										}
										if (req.parameters.BitBlt.lpPBrushAddr) {
											// ブラシがあれば選択
											NPDISP_BRUSH brush = { 0 };
											if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
												if (brush.key != 0) {
													auto it = npdispwin.brushes.find(brush.key);
													if (it != npdispwin.brushes.end()) {
														NPDISP_HOSTBRUSH value = it->second;
														if (value.brs) {
															if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
																TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
															}
															NPDISP_DRAWMODE drawMode = { 0 };
															int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
															if (npdisp.bpp == 1) {
																drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
																drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
																if (req.parameters.BitBlt.Rop3 == PATCOPY) {
																	if (drawMode.LbkColor) {
																		SelectObject(dstbmphdc.hdc, GetStockObject(WHITE_BRUSH));
																	}
																	else {
																		SelectObject(dstbmphdc.hdc, GetStockObject(BLACK_BRUSH));
																	}
																}
															}
															SelectObject(dstbmphdc.hdc, value.brs);
															if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
																SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
															}
															if (hasDrawMode) {
																SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
																SetTextColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
																if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
																}
																SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
																SetROP2(dstbmphdc.hdc, drawMode.Rop2);
															}
														}
													}
												}
											}
										}
										//BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
										switch (req.parameters.BitBlt.Rop3) {
										case 0x00CC0020: // SRCCOPY
										case 0x00EE0086: // SRCPAINT
										case 0x008800C6: // SRCAND
										case 0x00660046: // SRCINVERT
										case 0x00440328: // SRCERASE 
										case 0x00330008: // NOTSRCCOPY
										case 0x001100A6: // NOTSRCERASE
										case 0x00C000CA: // MERGECOPY
										case 0x00BB0226: // MERGEPAINT
										case 0x00F00021: // PATCOPY 
										case 0x00FB0A09: // PATPAINT 
										case 0x005A0049: // PATINVERT 
										case 0x00550009: // DSTINVERT
											if (hasDstDev && hasSrcDev) {
												//	NPDISP_DRAWMODE drawMode = { 0 };
												//	int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												//if (req.parameters.BitBlt.Rop3 == 0x008800c6) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
												//}
												//if (req.parameters.BitBlt.lpSrcDevAddr == 0x16670000) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
												//	//char test[16];
												//	//SelectObject(dstbmphdc.hdc, npdispwin.hFont); // DEBUG
												//	//sprintf(test, "%08x", req.parameters.BitBlt.lpSrcDevAddr);
												//	//SetBkColor(dstbmphdc.hdc, 0xffffff);
												//	//SetTextColor(dstbmphdc.hdc, 0x000000);
												//	//SetBkMode(dstbmphdc.hdc, OPAQUE);
												//	//TextOutA(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, test, strlen(test));
												//}
												//else if (req.parameters.BitBlt.lpSrcDevAddr == 0x166f0000) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
												//	//char test[16];
												//	//SelectObject(dstbmphdc.hdc, npdispwin.hFont); // DEBUG
												//	//sprintf(test, "%08x", req.parameters.BitBlt.lpSrcDevAddr);
												//	//SetBkColor(dstbmphdc.hdc, 0xffffff);
												//	//SetTextColor(dstbmphdc.hdc, 0x000000);
												//	//SetBkMode(dstbmphdc.hdc, OPAQUE);
												//	//TextOutA(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, test, strlen(test));
												//}
												//else {
													BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
												//}
											}
											break;
										case 0x00000042: // BLACKNESS
											if (hasDstDev) {
												BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
											}
											break;
										case 0x00FF0062: // WHITENESS
											if (hasDstDev) {
												BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
											}
											break;
										default:
											if (hasDstDev && hasSrcDev) {
												//	NPDISP_DRAWMODE drawMode = { 0 };
												//	int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												//if (req.parameters.BitBlt.Rop3 == 0x008800c6) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
												//}
												if (!BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3)) {
													TRACEOUT2(("BitBlt Error"));
												}
											}
											break;
										}
										SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
										npdisp.updated = 1;
										retValue = 1; // 成功

										npdisp_WriteBitmapToPBITMAP(&dstPBmp, &dstbmphdc, dstBeginLine, dstNumLines);

										npdisp_FreeBitmap(&dstbmphdc);
									}
									npdisp_FreeBitmap(&srcbmphdc);
								}
							}
						}
						else if (req.parameters.BitBlt.lpPBrushAddr)
						{
							TRACEOUT_BITBLT(("-> BRUSH"));
							NPDISP_BRUSH brush = { 0 };
							if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
								if (brush.key != 0) {
									auto it = npdispwin.brushes.find(brush.key);
									if (it != npdispwin.brushes.end()) {
										NPDISP_HOSTBRUSH value = it->second;
										if (value.brs) {
											NPDISP_WINDOWS_BMPHDC dstbmphdc = { 0 };
											npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0, dstBeginLine, dstNumLines);
											if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &dstbmphdc, 0, dstBeginLine, dstNumLines)) {
												if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
													TRACEOUT2(("BitBlt Check MEM BRUSH %08x -> %08x", req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.lpDestDevAddr));
												}
												NPDISP_DRAWMODE drawMode = { 0 };
												int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												if (npdisp.bpp == 1) {
													drawMode.LbkColor = drawMode.bkColor ? 0xffffff : 0;
													drawMode.LTextColor = drawMode.TextColor ? 0xffffff : 0;
													if (req.parameters.BitBlt.Rop3 == PATCOPY) {
														if (drawMode.LbkColor) {
															SelectObject(dstbmphdc.hdc, GetStockObject(WHITE_BRUSH));
														}
														else {
															SelectObject(dstbmphdc.hdc, GetStockObject(BLACK_BRUSH));
														}
													}
												}

												HBRUSH oldBrush = (HBRUSH)SelectObject(dstbmphdc.hdc, value.brs);
												if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
													SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(brush.lbrush.lbBkColor));
												}
												if (hasDrawMode) {
													SetBkColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
													SetTextColor(dstbmphdc.hdc, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
													if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {

													}
													SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
													SetROP2(dstbmphdc.hdc, drawMode.Rop2);
												}
												PatBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3);
												SelectObject(dstbmphdc.hdc, oldBrush);
												npdisp.updated = 1;

												npdisp_WriteBitmapToPBITMAP(&dstPBmp, &dstbmphdc, dstBeginLine, dstNumLines);

												npdisp_FreeBitmap(&dstbmphdc);
											}
										}
									}
								}
							}
						}
					}
				}
			}
			else {
				retValue = 1; // 成功したことにする
			}
			npdisp_writeMemory16(retValue, req.parameters.BitBlt.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_DeviceBitmapBits:
		{
			TRACEOUT(("DeviceBitmapBits"));
			UINT16 retValue = 0;
			if (req.parameters.DeviceBitmapBits.cScans == 24 && req.parameters.DeviceBitmapBits.iStart == 0) {
				TRACEOUTF(("CHECK"));
			}
			if (req.parameters.DeviceBitmapBits.lpBitmapAddr) {
				NPDISP_PBITMAP tgtPBmp;
				if (npdisp_readMemory(&tgtPBmp, req.parameters.DeviceBitmapBits.lpBitmapAddr, sizeof(NPDISP_PBITMAP))) {
					BITMAPINFOHEADER biHeader = { 0 };
					npdisp_readMemory(&biHeader, req.parameters.DeviceBitmapBits.lpBitmapInfoAddr, sizeof(BITMAPINFOHEADER));
					if (biHeader.biPlanes == 1 && (biHeader.biBitCount == 1 || biHeader.biBitCount == 4 || biHeader.biBitCount == 8 || biHeader.biBitCount == 24 || biHeader.biBitCount == 32) && biHeader.biHeight > req.parameters.DeviceBitmapBits.iStart) {
						NPDISP_WINDOWS_BMPHDC tgtbmphdc = { 0 };
						int stride = ((biHeader.biWidth * biHeader.biBitCount + 31) / 32) * 4;
						int height = req.parameters.DeviceBitmapBits.cScans;
						int beginLine = biHeader.biHeight - height - req.parameters.DeviceBitmapBits.iStart;
						int lpbiLen = 0;
						if (biHeader.biBitCount <= 8) {
							lpbiLen = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << biHeader.biBitCount);
						}
						else {
							lpbiLen = sizeof(BITMAPINFO);
						}
						BITMAPINFO* lpbi;
						lpbi = (BITMAPINFO*)malloc(lpbiLen);
						if (lpbi) {
							npdisp_readMemory(lpbi, req.parameters.DeviceBitmapBits.lpBitmapInfoAddr, lpbiLen);
							npdisp_PreloadBitmapFromPBITMAP(&tgtPBmp, 0, beginLine, height);
							npdisp_preloadMemory(req.parameters.DeviceBitmapBits.lpDIBitsAddr, stride * height);
							if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&tgtPBmp, &tgtbmphdc, 0, beginLine, height)) {
								void* pBits = NULL;
								int i;
								if (req.parameters.DeviceBitmapBits.iStart + height > biHeader.biHeight) {
									height = biHeader.biHeight - req.parameters.DeviceBitmapBits.iStart;
								}
								lpbi->bmiHeader.biHeight = height;
								if (lpbi->bmiHeader.biSizeImage == 0) {
									// XXX: 画像データがなければパレットセット、あればそのまま　根拠無し
									if (lpbi->bmiHeader.biBitCount == 1) {
										// 2色パレットセット
										for (i = 0; i < NELEMENTS(npdisp_palette_rgb2); i++) {
											if (lpbi->bmiColors[i].rgbReserved) {
												lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb2[i].r;
												lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb2[i].g;
												lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb2[i].b;
												lpbi->bmiColors[i].rgbReserved = 0;
											}
										}
									}
									else if (lpbi->bmiHeader.biBitCount == 4) {
										// 16色パレットセット
										for (i = 0; i < NELEMENTS(npdisp_palette_rgb16); i++) {
											if (lpbi->bmiColors[i].rgbReserved) {
												lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb16[i].r;
												lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb16[i].g;
												lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb16[i].b;
												lpbi->bmiColors[i].rgbReserved = 0;
											}
										}
									}
									else if (lpbi->bmiHeader.biBitCount == 8) {
										// 256色パレットセット
										for (i = 0; i < NELEMENTS(npdisp_palette_rgb256); i++) {
											if (lpbi->bmiColors[i].rgbReserved) {
												lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb256[i].r;
												lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb256[i].g;
												lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb256[i].b;
												lpbi->bmiColors[i].rgbReserved = 0;
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
									if (biCompression == BI_RGB) {
										npdisp_readMemory(pBits, req.parameters.DeviceBitmapBits.lpDIBitsAddr, stride * height);
									}
									else {
										UINT8* cdata = (UINT8*)malloc(lpbi->bmiHeader.biSizeImage);
										if (cdata) {
											BITMAPINFOHEADER bmiHeaderRLE = lpbi->bmiHeader;
											bmiHeaderRLE.biCompression = biCompression;
											if (bmiHeaderRLE.biHeight > 0) {
												bmiHeaderRLE.biHeight = -bmiHeaderRLE.biHeight; // 逆さで出力
											}
											npdisp_readMemory(cdata, req.parameters.DeviceBitmapBits.lpDIBitsAddr, lpbi->bmiHeader.biSizeImage);
											DecompressRLEBMP(&bmiHeaderRLE, cdata, lpbi->bmiHeader.biSizeImage, (UINT8*)pBits);
											free(cdata);
										}
									}
									if (req.parameters.DeviceBitmapBits.fGet) {
										// Get Bits
										//BitBlt(hdc, 0, req.parameters.DeviceBitmapBits.iStart, biHeader.biWidth, req.parameters.DeviceBitmapBits.cScans, NULL, 0, 0, WHITENESS);
										BitBlt(hdc, 0, 0, biHeader.biWidth, height, tgtbmphdc.hdc, 0, biHeader.biHeight - height - req.parameters.DeviceBitmapBits.iStart, SRCCOPY);
										retValue = height;
										if (biCompression == BI_RGB) {
											npdisp_writeMemory(pBits, req.parameters.DeviceBitmapBits.lpDIBitsAddr, stride * height);
										}
										else {
											// サポートしない
											//UINT8* cdata = (UINT8*)malloc(lpbi->bmiHeader.biSizeImage);
											//if (cdata) {

											//	npdisp_writeMemory(cdata, req.parameters.DeviceBitmapBits.lpDIBitsAddr, lpbi->bmiHeader.biSizeImage);
											//	free(cdata);
											//}
										}
										if (tgtbmphdc.lpbi->bmiHeader.biWidth == 32 && tgtbmphdc.lpbi->bmiHeader.biHeight == 32) {
											TRACEOUT2(("DeviceBitmapBits Check Get %08x", req.parameters.DeviceBitmapBits.lpBitmapAddr));
										}
									}
									else {
										// Set Bits
										//BitBlt(tgtbmphdc.hdc, 0, req.parameters.DeviceBitmapBits.iStart, biHeader.biWidth, req.parameters.DeviceBitmapBits.cScans, NULL, 0, 0, WHITENESS);
										BitBlt(tgtbmphdc.hdc, 0, biHeader.biHeight - height - req.parameters.DeviceBitmapBits.iStart, biHeader.biWidth, height, hdc, 0, 0, SRCCOPY);
										retValue = height;
										npdisp.updated = 1;
										npdisp_WriteBitmapToPBITMAP(&tgtPBmp, &tgtbmphdc, beginLine, height);

										if (tgtbmphdc.lpbi->bmiHeader.biWidth == 32 && tgtbmphdc.lpbi->bmiHeader.biHeight == 32) {
											TRACEOUT2(("DeviceBitmapBits Check Set %08x", req.parameters.DeviceBitmapBits.lpBitmapAddr));
										}
									}

									SelectObject(hdc, hOldBmp);
									DeleteObject(hBmp);
								}
								npdisp_FreeBitmap(&tgtbmphdc);
							}
							free(lpbi);
						}
						else {
							retValue = 0;
						}
					}
				}
			}
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
			UINT32 retValue = 0;
			UINT8* lpText;
			if (req.parameters.extTextOut.wCount != 0) {
				lpText = (UINT8*)npdisp_readMemoryStringWithCount(req.parameters.extTextOut.lpStringAddr, req.parameters.extTextOut.wCount < 0 ? -req.parameters.extTextOut.wCount : req.parameters.extTextOut.wCount);
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
					npdisp_readMemory(&rectTmp, req.parameters.extTextOut.lpClipRectAddr, sizeof(NPDISP_RECT));
					if (req.parameters.extTextOut.lpOpaqueRectAddr) npdisp_readMemory(&opaquerect, req.parameters.extTextOut.lpOpaqueRectAddr, sizeof(NPDISP_RECT));
					cliprect.top = rectTmp.top;
					cliprect.left = rectTmp.left;
					cliprect.bottom = rectTmp.bottom;
					cliprect.right = rectTmp.right;
					NPDISP_DRAWMODE drawMode = { 0 };
					int hasDrawMode = 0;
					if (req.parameters.extTextOut.lpDrawModeAddr) hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.extTextOut.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
					if (npdisp.bpp == 1) {
						drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
						drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
					}
					//if (drawMode.LTextColor != 0x000000 && drawMode.LTextColor != 0xffffff && drawMode.LTextColor != 0xffffffff) {
					//	drawMode.LTextColor = 0x00ff0090;
					//}
					UINT32 bkColor = 0xffffff;
					UINT32 textColor = 0x000000;
					if (hasDrawMode) {
						if (npdisp.bpp == 1) {
							bkColor = NPDISP_ADJUST_MONOCOLOR(drawMode.bkColor);
							textColor = NPDISP_ADJUST_MONOCOLOR(drawMode.TextColor);
						}
						else {
							bkColor = drawMode.LbkColor;
							textColor = drawMode.LTextColor;
						}
					}
					//HGDIOBJ oldFont = SelectObject(tgtDC, npdispwin.hFont);
					NPDISP_FONTINFO fontInfo;
					if (npdisp_readMemory(&fontInfo, req.parameters.extTextOut.lpFontInfoAddr, sizeof(NPDISP_FONTINFO))) {
						SIZE sz = { 0, fontInfo.dfPixHeight };
						int loopLen = req.parameters.extTextOut.wCount >= 0 ? req.parameters.extTextOut.wCount : -req.parameters.extTextOut.wCount;
						for (i = 0; i < loopLen; i++) {
							NPDISP_FONTCHARINFO3 charInfo;
							int charIdx = (int)lpText[i] - fontInfo.dfFirstChar;
							if (fontInfo.dfLastChar < charIdx) {
								charIdx = fontInfo.dfDefaultChar;
							}
							if (req.parameters.extTextOut.lpCharWidthsAddr) {
								sz.cx += npdisp_readMemory16(req.parameters.extTextOut.lpCharWidthsAddr + i * 2);
							}
							else {
								if (npdisp_readMemory(&charInfo, req.parameters.extTextOut.lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
									sz.cx += charInfo.width;
								}
							}
						}
						//GetTextExtentPoint32A(tgtDC, "NEKO", strlen("NEKO"), &sz);
						retValue = (sz.cx) | (sz.cy << 16);
						if (req.parameters.extTextOut.wCount < 0) {
							// nothing to do
						}
						else if (req.parameters.extTextOut.wCount == 0) {
							// 塗りつぶし
							TRACEOUT(("-> FILL"));
							if (req.parameters.extTextOut.wOptions & 2) {
								NPDISP_PBITMAP dstPBmp;
								NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
								int dstDevType = 0;
								HDC tgtDC = npdispwin.hdc;
								npdisp_readMemory(&dstDevType, req.parameters.extTextOut.lpDestDevAddr, 2);
								if (dstDevType == 0) {
									// memory 
									if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
										if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
											tgtDC = bmphdc.hdc;
										}
									}
								}
								if (npdisp.longjmpnum == 0) {
									if (req.parameters.extTextOut.lpOpaqueRectAddr && (req.parameters.extTextOut.wOptions & 2)) {
										HRGN hRgn = req.parameters.extTextOut.lpClipRectAddr ? CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom) : NULL;
										if (hRgn) {
											SelectClipRgn(tgtDC, hRgn);
										}
										RECT gdiopaquerect = { 0 };
										gdiopaquerect.top = opaquerect.top;
										gdiopaquerect.left = opaquerect.left;
										gdiopaquerect.bottom = opaquerect.bottom;
										gdiopaquerect.right = opaquerect.right; 
										HBRUSH hBrush = CreateSolidBrush(NPDISP_ADJUST_MONOCOLOR(drawMode.LbkColor));
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
											if (npdisp_readMemory(&charInfo, req.parameters.extTextOut.lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
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
											if (req.parameters.extTextOut.lpCharWidthsAddr) {
												int curCharWidth = npdisp_readMemory16(req.parameters.extTextOut.lpCharWidthsAddr + i * 2);
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
													SetBkColor(hdcText, NPDISP_ADJUST_COLORREF(bkColor));
													SetTextColor(hdcText, NPDISP_ADJUST_COLORREF(textColor));
													BitBlt(hdcTextInv, 0, 0, sz.cx, sz.cy, hdcText, 0, 0, NOTSRCCOPY);
													//SetBkMode(hdcText, TRANSPARENT);
													NPDISP_PBITMAP dstPBmp;
													NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
													int dstDevType = 0;
													HDC tgtDC = npdispwin.hdc;
													npdisp_readMemory(&dstDevType, req.parameters.extTextOut.lpDestDevAddr, 2);
													if (dstDevType == 0) {
														// memory 
														if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
															if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
																tgtDC = bmphdc.hdc;
															}
														}
													}
													if (npdisp.longjmpnum == 0) {
														HRGN hRgn = req.parameters.extTextOut.lpClipRectAddr ? CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom) : NULL;
														if (hRgn) {
															SelectClipRgn(tgtDC, hRgn);
														}
														if (req.parameters.extTextOut.lpOpaqueRectAddr && (req.parameters.extTextOut.wOptions & 2)) {
															RECT gdiopaquerect = { 0 };
															gdiopaquerect.top = opaquerect.top;
															gdiopaquerect.left = opaquerect.left;
															gdiopaquerect.bottom = opaquerect.bottom;
															gdiopaquerect.right = opaquerect.right;
															HBRUSH hBrush = CreateSolidBrush(NPDISP_ADJUST_MONOCOLOR(bkColor));
															HGDIOBJ oldBrush = SelectObject(tgtDC, hBrush);
															FillRect(tgtDC, &gdiopaquerect, hBrush);
															SelectObject(tgtDC, oldBrush);
															DeleteObject(hBrush);
															TRACEOUT(("-> HAS BACKGROUND"));
														}
														SetROP2(tgtDC, R2_COPYPEN);
														HDC hdcTextTgt = hdcText;
														//if (npdisp.bpp == 1 && NPDISP_ADJUST_MONOCOLOR(textColor) != 0) {
														//	hdcTextTgt = hdcTextInv;
														//}
														if ((drawMode.bkMode == 1 || drawMode.bkMode == 4)) {
															// 背景透過
															TRACEOUT(("FG:%08x BG:TRANS", textColor));
															SetBkMode(tgtDC, OPAQUE);
															SetBkColor(tgtDC, 0x000000);
															SetTextColor(tgtDC, 0xffffff);
															BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCAND);
															SetBkColor(tgtDC, NPDISP_ADJUST_COLORREF(textColor));
															SetTextColor(tgtDC, 0x000000);
															BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCPAINT);
															//SetBkColor(tgtDC, NPDISP_ADJUST_COLORREF(textColor));
															//SetTextColor(tgtDC, NPDISP_ADJUST_COLORREF(bkColor));
															//BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCCOPY);
														}
														else {
															// 背景不透明
															TRACEOUT(("FG:%08x BG:%08x", textColor, bkColor));
															SetBkColor(tgtDC, NPDISP_ADJUST_COLORREF(textColor));
															SetTextColor(tgtDC, NPDISP_ADJUST_COLORREF(bkColor));
															BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcTextTgt, 0, 0, SRCCOPY);
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
			npdisp_writeMemory32(retValue, req.parameters.extTextOut.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetDIBitsToDevice:
		{
			TRACEOUT(("SetDIBitsToDevice"));
			int dstDevType = 0;
			UINT16 retValue = 0;
			if (req.parameters.SetDIBitsToDevice.lpDestDevAddr) {
				if (npdisp_readMemory(&dstDevType, req.parameters.SetDIBitsToDevice.lpDestDevAddr, 2)) {
					BITMAPINFOHEADER biHeader = { 0 };
					if (npdisp_readMemory(&biHeader, req.parameters.SetDIBitsToDevice.lpBitmapInfoAddr, sizeof(BITMAPINFOHEADER))) {
						int stride = ((biHeader.biWidth * biHeader.biBitCount + 31) / 32) * 4;
						int height = biHeader.biHeight >= 0 ? biHeader.biHeight : -biHeader.biHeight;
						int lpbiLen = 0;
						if (biHeader.biBitCount <= 8) {
							lpbiLen = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << biHeader.biBitCount);
						}
						else {
							lpbiLen = sizeof(BITMAPINFO);
						}
						BITMAPINFO* lpbi;
						lpbi = (BITMAPINFO*)malloc(lpbiLen);
						if (lpbi) {
							npdisp_readMemory(lpbi, req.parameters.SetDIBitsToDevice.lpBitmapInfoAddr, lpbiLen);
							UINT8* pBits = (UINT8*)malloc(stride * height);
							if (pBits) {
								npdisp_readMemory(pBits, req.parameters.SetDIBitsToDevice.lpDIBitsAddr, stride * height);
								HDC tgtDC = npdispwin.hdc;
								if (height > req.parameters.SetDIBitsToDevice.iScan) {
									NPDISP_DRAWMODE drawMode = { 0 };
									if (npdisp_readMemory(&drawMode, req.parameters.SetDIBitsToDevice.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE))) {
										if (npdisp.bpp == 1) {
											drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
											drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
										}
										SetBkColor(tgtDC, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
										SetTextColor(tgtDC, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
										SetBkMode(tgtDC, drawMode.bkMode);
										SetROP2(tgtDC, drawMode.Rop2);
									}
									HRGN hRgn = NULL;
									if (req.parameters.SetDIBitsToDevice.lpClipRectAddr) {
										RECT cliprect = { 0 };
										NPDISP_RECT rectTmp = { 0 };
										npdisp_readMemory(&rectTmp, req.parameters.SetDIBitsToDevice.lpClipRectAddr, sizeof(NPDISP_RECT));
										cliprect.top = rectTmp.top;
										cliprect.left = rectTmp.left;
										cliprect.bottom = rectTmp.bottom;
										cliprect.right = rectTmp.right;
										hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
										SelectClipRgn(tgtDC, hRgn);
									}

									if (req.parameters.SetDIBitsToDevice.iScan + req.parameters.SetDIBitsToDevice.cScans > height) {
										req.parameters.SetDIBitsToDevice.cScans = height - req.parameters.SetDIBitsToDevice.iScan;
									}

									if (SetDIBitsToDevice(tgtDC, req.parameters.SetDIBitsToDevice.X, req.parameters.SetDIBitsToDevice.Y, biHeader.biWidth, height, 0, 0,
										req.parameters.SetDIBitsToDevice.iScan, req.parameters.SetDIBitsToDevice.cScans, pBits, lpbi, DIB_RGB_COLORS)==0) {
										TRACEOUTF(("ERROR"));

									}

									if (hRgn) {
										SelectClipRgn(tgtDC, NULL);
										DeleteObject(hRgn);
									}

									npdisp.updated = 1;
									retValue = req.parameters.SetDIBitsToDevice.cScans;
									if (req.parameters.SetDIBitsToDevice.cScans == 24 && req.parameters.SetDIBitsToDevice.iScan == 0) {
										TRACEOUTF(("OK"));
									}
								}
								free(pBits);
							}
							free(lpbi);
						}
					}
				}
			}
			if (req.parameters.SetDIBitsToDevice.cScans == 24 && req.parameters.SetDIBitsToDevice.iScan == 0) {
				TRACEOUTF(("CHECK"));
			}
			npdisp_writeMemory16(retValue, req.parameters.SetDIBitsToDevice.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SaveScreenBitmap:
		{
			TRACEOUT(("SaveScreenBitmap"));
			UINT16 retValue = 0;
			NPDISP_RECT rect = { 0 };
			if (npdisp_readMemory(&rect, req.parameters.SaveScreenBitmap.lpRect, sizeof(rect)) && npdisp.longjmpnum == 0) {
				if (req.parameters.SaveScreenBitmap.wCommand == 0) {
					BitBlt(npdispwin.hdcShadow, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, npdispwin.hdc, rect.left, rect.top, SRCCOPY);
					npdispwin.rectShadow.left = rect.left;
					npdispwin.rectShadow.right = rect.right;
					npdispwin.rectShadow.top = rect.top;
					npdispwin.rectShadow.bottom = rect.bottom;
					retValue = 1;
				}
				else if (req.parameters.SaveScreenBitmap.wCommand == 1) {
					if (npdispwin.rectShadow.left == rect.left && npdispwin.rectShadow.right == rect.right && npdispwin.rectShadow.top == rect.top && npdispwin.rectShadow.bottom == rect.bottom) {
						BitBlt(npdispwin.hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, npdispwin.hdcShadow, rect.left, rect.top, SRCCOPY);
						npdisp.updated = 1;
						retValue = 1;
					}
					else {
						retValue = 0;
					}
				}
				else if (req.parameters.SaveScreenBitmap.wCommand == 2) {
					BitBlt(npdispwin.hdcShadow, 0, 0, npdisp.width, npdisp.height, NULL, 0, 0, BLACKNESS);
					npdispwin.rectShadow.left = 0;
					npdispwin.rectShadow.right = 0;
					npdispwin.rectShadow.top = 0;
					npdispwin.rectShadow.bottom = 0;
					retValue = 1;
				}
			}
			npdisp_writeMemory16(retValue, req.parameters.SaveScreenBitmap.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetCursor:
		{
			TRACEOUT(("SetCursor"));
			NPDISP_CURSORSHAPE cursorShape = { 0 };
			if (npdisp_readMemory(&cursorShape, req.parameters.SetCursor.lpCursorShapeAddr, sizeof(cursorShape)) && npdisp.longjmpnum == 0) {
				int cursorDataStride = cursorShape.csWidthBytes;
				int stride = ((cursorShape.csWidth + 7) / 8 + 1) / 2 * 2;
				if (cursorShape.csWidth > 0 && cursorShape.csHeight > 0) {
					void* pBits = (char*)malloc(stride * cursorShape.csHeight);
					if (pBits) {
						int x, y;
						HBITMAP hBmp;
						UINT8* pBits8 = (UINT8*)pBits;
						UINT32 pixelBufAddr = req.parameters.SetCursor.lpCursorShapeAddr + sizeof(cursorShape);
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
						hBmp = CreateBitmap(cursorShape.csWidth, cursorShape.csHeight, 1, 1, pBits);
						if (hBmp) {
							if (npdispwin.hBmpCursorMask) {
								SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursorMask);
								DeleteObject(npdispwin.hBmpCursorMask);
								npdispwin.hBmpCursorMask = NULL;
							}
							npdispwin.hOldBmpCursorMask = (HBITMAP)SelectObject(npdispwin.hdcCursorMask, hBmp);
							npdispwin.hBmpCursorMask = hBmp;
						}
						pBits8 = (UINT8*)pBits; // 戻す
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
						hBmp = CreateBitmap(cursorShape.csWidth, cursorShape.csHeight, 1, 1, pBits);
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

						npdisp.updated = 1;

						free(pBits);
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
			break;
		}
		case NPDISP_FUNCORDER_MoveCursor:
		{
			//TRACEOUT(("MoveCursor"));
			npdisp.cursorX = req.parameters.MoveCursor.wAbsX;
			npdisp.cursorY = req.parameters.MoveCursor.wAbsY;
			npdisp.updated = 1;
			//retCode = NPDISP_RETCODE_FAILED;
			break;
		}
		case NPDISP_FUNCORDER_CheckCursor:
		{
			// 何もすることがない;
			break;
		}
		case NPDISP_FUNCORDER_FastBorder:
		{
			TRACEOUT(("FastBorder"));
			UINT16 retValue = 0; 
			int dstDevType = 0;
			HDC tgtDC = npdispwin.hdc;
			npdisp_readMemory(&dstDevType, req.parameters.fastBorder.lpDestDevAddr, 2);
			if (dstDevType != 0) {
				// PDEVICE
				if (req.parameters.fastBorder.lpPBrushAddr) {
					// ブラシがあれば選択
					NPDISP_BRUSH brush = { 0 };
					if (npdisp_readMemory(&brush, req.parameters.fastBorder.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
						if (brush.key != 0) {
							auto it = npdispwin.brushes.find(brush.key);
							if (it != npdispwin.brushes.end()) {
								NPDISP_HOSTBRUSH value = it->second;
								if (value.brs) {
									SelectObject(tgtDC, value.brs);
								}
							}
						}
					}
				}
				NPDISP_DRAWMODE drawMode = { 0 };
				if (npdisp_readMemory(&drawMode, req.parameters.fastBorder.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE))) {
					if (npdisp.bpp == 1) {
						drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
						drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
					}
					SetBkColor(tgtDC, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
					SetTextColor(tgtDC, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
					SetBkMode(tgtDC, drawMode.bkMode);
					SetROP2(tgtDC, drawMode.Rop2);
				}
				HRGN hRgn = NULL;
				if (req.parameters.fastBorder.lpClipRectAddr) {
					RECT cliprect = { 0 };
					NPDISP_RECT rectTmp = { 0 };
					npdisp_readMemory(&rectTmp, req.parameters.fastBorder.lpClipRectAddr, sizeof(NPDISP_RECT));
					cliprect.top = rectTmp.top;
					cliprect.left = rectTmp.left;
					cliprect.bottom = rectTmp.bottom;
					cliprect.right = rectTmp.right;
					hRgn = CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom);
					SelectClipRgn(tgtDC, hRgn);
				}

				NPDISP_RECT rectBdr = { 0 };
				npdisp_readMemory(&rectBdr, req.parameters.fastBorder.lpRectAddr, sizeof(NPDISP_RECT));

				int tx = req.parameters.fastBorder.wVertBorderThick;
				int ty = req.parameters.fastBorder.wHorizBorderThick;
				PatBlt(tgtDC, rectBdr.left, rectBdr.top, rectBdr.right - rectBdr.left, ty, req.parameters.fastBorder.dwRasterOp);
				PatBlt(tgtDC, rectBdr.left, rectBdr.top + ty, tx, (rectBdr.bottom - rectBdr.top) - ty * 2, req.parameters.fastBorder.dwRasterOp);
				PatBlt(tgtDC, rectBdr.right - tx, rectBdr.top + ty, tx, (rectBdr.bottom - rectBdr.top) - ty * 2, req.parameters.fastBorder.dwRasterOp);
				PatBlt(tgtDC, rectBdr.left, rectBdr.bottom - ty, rectBdr.right - rectBdr.left, ty, req.parameters.fastBorder.dwRasterOp);
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

			npdisp_writeMemory16(retValue, req.parameters.fastBorder.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Output:
		{
			UINT16 retValue = 0xffff;
			NPDISP_PBITMAP dstPBmp;
			NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
			int dstDevType = 0;
			HDC tgtDC = npdispwin.hdc;
			npdisp_readMemory(&dstDevType, req.parameters.output.lpDestDevAddr, 2);
			if (dstDevType == 0) {
				// memory 
				if (req.parameters.output.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.output.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
					if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
						tgtDC = bmphdc.hdc;
					}
				}
			}
			if (npdisp.longjmpnum == 0) {
				//NPDISP_HOSTBRUSH curHostBrush;
				HBRUSH curBrush = NULL;
				if (req.parameters.output.lpPPenAddr) {
					// ペンがあれば選択
					NPDISP_PEN pen = { 0 };
					if (npdisp_readMemory(&pen, req.parameters.output.lpPPenAddr, sizeof(NPDISP_PEN))) {
						if (pen.key != 0) {
							auto it = npdispwin.pens.find(pen.key);
							if (it != npdispwin.pens.end()) {
								NPDISP_HOSTPEN value = it->second;
								if (value.pen) {
									SelectObject(tgtDC, value.pen);
								}
							}
						}
					}
				}
				if (req.parameters.output.lpPBrushAddr) {
					// ブラシがあれば選択
					NPDISP_BRUSH brush = { 0 };
					if (npdisp_readMemory(&brush, req.parameters.output.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
						if (brush.key != 0) {
							auto it = npdispwin.brushes.find(brush.key);
							if (it != npdispwin.brushes.end()) {
								NPDISP_HOSTBRUSH value = it->second;
								if (value.brs) {
									SelectObject(tgtDC, value.brs);
									curBrush = value.brs;
									//curHostBrush = value;
								}
							}
						}
					}
				}
				NPDISP_DRAWMODE drawMode = { 0 };
				if (npdisp_readMemory(&drawMode, req.parameters.output.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE))) {
					if (npdisp.bpp == 1) {
						drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
						drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
					}
					SetBkColor(tgtDC, NPDISP_ADJUST_COLORREF(drawMode.LbkColor));
					SetTextColor(tgtDC, NPDISP_ADJUST_COLORREF(drawMode.LTextColor));
					SetBkMode(tgtDC, drawMode.bkMode);
					SetROP2(tgtDC, drawMode.Rop2);
				}
				HRGN hRgn = NULL;
				if (req.parameters.output.lpClipRectAddr) {
					RECT cliprect = { 0 };
					NPDISP_RECT rectTmp = { 0 };
					npdisp_readMemory(&rectTmp, req.parameters.output.lpClipRectAddr, sizeof(NPDISP_RECT));
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
				switch (req.parameters.output.wStyle) {
				case 18: // OS_POLYLINE
				{
					UINT32 lpPointsAddr = req.parameters.output.lpPointsAddr;
					POINT* gdiPoints = (POINT*)malloc(req.parameters.output.wCount * sizeof(POINT));
					if (gdiPoints) {
						for (int i = 0; i < req.parameters.output.wCount; i++) {
							NPDISP_POINT pt;
							if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
								gdiPoints[i].x = pt.x;
								gdiPoints[i].y = pt.y;
							}
							else {
								break;
							}
							lpPointsAddr += sizeof(NPDISP_POINT);
						}
						Polyline(tgtDC, gdiPoints, req.parameters.output.wCount);
						if (bmphdc.hdc) {
							// 書き戻し
							npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);
						}
						free(gdiPoints);
					}
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
					UINT32 lpPointsAddr = req.parameters.output.lpPointsAddr;
					NPDISP_POINT pt;
					if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
						int beginY = pt.y;
						lpPointsAddr += sizeof(NPDISP_POINT);
						for (int i = 1; i < req.parameters.output.wCount; i++) {
							if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
								if (curBrush) {
									RECT rect;
									rect.left = pt.x;
									rect.right = pt.y;
									rect.top = beginY;
									rect.bottom = rect.top + 1;
									FillRect(tgtDC, &rect, curBrush);
								}
								else {
									MoveToEx(tgtDC, pt.x, beginY, NULL);
									LineTo(tgtDC, pt.x, beginY);
								}
							}
							else {
								break;
							}
							lpPointsAddr += sizeof(NPDISP_POINT);
						}
						if (bmphdc.hdc) {
							// 書き戻し
							npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);
						}
					}
					retValue = 1;
					break;
				}
				default:
				{
					TRACEOUTF(("Unsupported Output: %d", req.parameters.output.wStyle));
					break;
				}
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
			npdisp_writeMemory16(retValue, req.parameters.output.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Pixel:
		{
			TRACEOUT(("Pixel"));
			UINT32 retValue = 0x80000000L;
			int dstDevType = 0;
			HDC tgtDC = npdispwin.hdc;
			npdisp_readMemory(&dstDevType, req.parameters.pixel.lpDestDevAddr, 2);
			NPDISP_PBITMAP dstPBmp;
			NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
			if (dstDevType == 0) {
				// memory 
				if (req.parameters.pixel.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.pixel.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
					if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
						tgtDC = bmphdc.hdc;
					}
				}
			}
			NPDISP_DRAWMODE drawMode = { 0 };
			int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.pixel.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
			if (hasDrawMode) {
				if (SetPixel(tgtDC, req.parameters.pixel.X, req.parameters.pixel.Y, req.parameters.pixel.dwPhysColor) != -1) {
					retValue = 1;
				}
				npdisp.updated = 1;
			}
			else {
				retValue = GetPixel(tgtDC, req.parameters.pixel.X, req.parameters.pixel.Y);
			}
			if (bmphdc.hdc) {
				npdisp_FreeBitmap(&bmphdc);
			}

			npdisp_writeMemory32(retValue, req.parameters.scanLR.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_ScanLR:
		{
			TRACEOUT(("ScanLR"));
			UINT16 retValue = 0xffff;
			int dstDevType = 0;
			HDC tgtDC = npdispwin.hdc;
			npdisp_readMemory(&dstDevType, req.parameters.scanLR.lpDestDevAddr, 2);
			NPDISP_PBITMAP dstPBmp;
			NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
			BITMAPINFOHEADER* lpBiHeader = &(npdispwin.bi.bmiHeader);
			UINT8* lpBits = (UINT8*)npdispwin.pBits;
			if (dstDevType == 0) {
				// memory 
				if (req.parameters.scanLR.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.scanLR.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
					if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
						tgtDC = bmphdc.hdc;
						lpBiHeader = &(bmphdc.lpbi->bmiHeader);
						lpBits = (UINT8*)bmphdc.pBits;
					}
				}
			}
			UINT32 devColor = req.parameters.scanLR.dwPhysColor;
			//if ((devColor >> 24) == 0) {
				const UINT8 r = (UINT8)(devColor & 0xFF);
				const UINT8 g = (UINT8)((devColor >> 8) & 0xFF);
				const UINT8 b = (UINT8)((devColor >> 16) & 0xFF);
				if (npdisp.bpp == 16) {
					const UINT8 r5 = r >> 3;
					const UINT8 g5 = g >> 3;
					const UINT8 b5 = b >> 3;
					devColor = (r5 << 10) | (g5 << 5) | b5;
				}
				if (npdisp.bpp == 1) {
					// モノクロはColorInfoで色を素通ししているのでここで変換
					devColor = npdisp_FindNearest2(r, g, b);
				}
				//else if (npdisp.bpp == 8) {
				//	devColor = npdisp_FindNearest256(r, g, b);
				//}
				//else if (npdisp.bpp == 4) {
				//	devColor = npdisp_FindNearest16(r, g, b);
				//}
				//else if (npdisp.bpp == 1) {
				//	devColor = npdisp_FindNearest2(r, g, b);
				//}
			//}
			//else {
			//	devColor = devColor & 0xffffff;
			//}
				//req.parameters.scanLR.Y++;
			int w = lpBiHeader->biWidth;
			int h = lpBiHeader->biHeight;
			UINT32 compMask = (1 << lpBiHeader->biBitCount) - 1;
			UINT32 stepBit = lpBiHeader->biBitCount;
			if (h < 0) h = -h;
			if (req.parameters.scanLR.Y < h && req.parameters.scanLR.X < w) {
				int stride = ((lpBiHeader->biWidth * lpBiHeader->biBitCount + 31) / 32) * 4;
				int x;
				if (req.parameters.scanLR.Style & 2) {
					// 左へスキャン
					lpBits += stride * req.parameters.scanLR.Y;
					for (x = req.parameters.scanLR.X; x >= 0; x--) {
						UINT8* p = lpBits + x * stepBit / 8;
						if (lpBiHeader->biBitCount > 16) {
							if (((*((UINT32*)p) & compMask) == devColor) == !!(req.parameters.scanLR.Style & 0x1)) {
								break;
							}
						}
						else if (lpBiHeader->biBitCount > 8) {
							if (((*((UINT16*)p) & compMask) == devColor) == !!(req.parameters.scanLR.Style & 0x1)) {
								break;
							}
						}
						else {
							int bitPos = (x * stepBit) % 8;
							bitPos = 7 - bitPos - (stepBit - 1); // 並びを反転
							if ((((*p >> bitPos) & compMask) == devColor) == !!(req.parameters.scanLR.Style & 0x1)) {
								break;
							}
						}
					}
					if (x == -1) {
						retValue = -1; // 端まで到達
					}
					else {
						retValue = x;
						//if (!(req.parameters.scanLR.Style & 0x1))retValue++;
					}
				}
				else {
					// 右へスキャン
					lpBits += stride * req.parameters.scanLR.Y;
					for (x = req.parameters.scanLR.X; x < w; x++) {
						UINT8* p = lpBits + x * stepBit / 8;
						if (lpBiHeader->biBitCount > 16) {
							if (((*((UINT32*)p) & compMask) == devColor) == !!(req.parameters.scanLR.Style & 0x1)) {
								break;
							}
						}
						else if (lpBiHeader->biBitCount > 8) {
							if (((*((UINT16*)p) & compMask) == devColor) == !!(req.parameters.scanLR.Style & 0x1)) {
								break;
							}
						}
						else {
							int bitPos = (x * stepBit) % 8;
							bitPos = 7 - bitPos - (stepBit - 1); // 並びを反転
							if ((((*p >> bitPos) & compMask) == devColor) == !!(req.parameters.scanLR.Style & 0x1)) {
								break;
							}
						}
					}
					if (x == w) {
						retValue = -1; // 端まで到達
					}
					else {
						retValue = x;
						//if (!(req.parameters.scanLR.Style & 0x1))retValue--;
					}
				}
			}
			if (bmphdc.hdc) {
				npdisp_FreeBitmap(&bmphdc);
			}

			npdisp_writeMemory16(retValue, req.parameters.scanLR.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_EnumObj:
		{
			TRACEOUT(("EnumObj"));
			UINT16 retValue = 0;
			int dstDevType = 0;
			npdisp_readMemory(&dstDevType, req.parameters.enumObj.lpDestDevAddr, 2);
			if (dstDevType != 0) {
				UINT16 idx = req.parameters.enumObj.enumIdx;
				if (req.parameters.enumObj.wStyle == 1) {
					// pen
					NPDISP_LPEN pen;
					pen.lopnWidth.x = 1;
					pen.lopnWidth.y = 0;
					pen.opnStyle = 0;
					pen.lopnColor = npdisp_ObjIdxToColor(idx);
					if (pen.lopnColor != -1) {
						retValue = 1;
					}
					npdisp_writeMemory(&pen, req.parameters.enumObj.lpLogObjAddr, sizeof(pen));
				}
				else if (req.parameters.enumObj.wStyle == 2) {
					// brush
					NPDISP_LBRUSH brush;
					brush.lbBkColor = 1;
					brush.lbHatch = 0;
					brush.lbStyle = 0;
					brush.lbColor = npdisp_ObjIdxToColor(idx);
					if (brush.lbColor != -1) {
						retValue = 1;
					}
					npdisp_writeMemory(&brush, req.parameters.enumObj.lpLogObjAddr, sizeof(brush));
				}
			}
			npdisp_writeMemory16(retValue, req.parameters.enumObj.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_INT2Fh:
		{
			if (req.parameters.INT2Fh.ax == 0x4001) {
				// DOS窓全画面モード設定
				np2wab.relaystateext = 0;
				np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
				npdisp.enabled = 0;
			}
			else if (req.parameters.INT2Fh.ax == 0x4002) {
				// DOS窓全画面モード解除
				npdisp.enabled = 1;
				npdisp.updated = 1;
				np2wab.relaystateext = 3;
				np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			}
			break;
		}
		case NPDISP_FUNCORDER_WEP:
		{
			TRACEOUT(("WEP"));
			// Windows終了
			npdisp.enabled = 0;
			np2wab.relaystateext = 0;
			np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			npdisp_releaseScreen();
			break;
		}
		default:
			TRACEOUT(("Function %d", req.funcOrder));
			retCode = NPDISP_RETCODE_FAILED;
			break;
		}
		npdisp_writeReturnCode(&req, npdisp.dataAddr, retCode); // ReturnCode書き込み
	}
	npdisp_cs_execflag = 0;
	npdispcs_leave_criticalsection();
	if (npdisp.longjmpnum) {
		if (npdisp_memory_hasNewCacheData()) {
			CPU_STAT_EXCEPTION_COUNTER_CLEAR(); // 読み書きが進んでいたら例外繰り返しではない
		}
		int longjmpnum = npdisp.longjmpnum;
		//PICITEM pi = &pic.pi[1];
		//pi->imr |= PIC_INT6;
		//if (CPU_STAT_PM) {
		//	CPU_EFLAG |= VIF_FLAG;
		//}
		//else {
		//	CPU_EFLAG |= I_FLAG;
		//}
		siglongjmp(exec_1step_jmpbuf, longjmpnum); // 転送
	}
	// 例外発生せずに全部送れたらリセットする
	CPU_REMCLOCK -= 2 * (npdisp_memory_getTotalReadSize() + npdisp_memory_getTotalWriteSize()); // メモリアクセスあたり2clock
	npdisp_memory_clearpreload();
	//PICITEM pi = &pic.pi[1];
	//pi->imr &= ~PIC_INT6;
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

static int lastID = 0;
static void IOOUTCALL npdisp_o7e8(UINT port, REG8 dat)
{
	npdisp.dataAddr = (dat << 24) | (npdisp.dataAddr >> 8);
	if (npdisp_debug_seqCounter >= 4) {
		TRACEOUT(("ADDRESS ERROR! %d %08x %08x", npdisp_debug_seqCounter, CPU_SS, lastID));
	}
	else {
		//TRACEOUT(("ADDRESS %d %08x", npdisp_debug_seqCounter, CPU_SS));
	}
	lastID = CPU_SS;
	npdisp_debug_seqCounter++;
	(void)port;
}

static void IOOUTCALL npdisp_o7e9(UINT port, REG8 dat)
{
	int i;
	//npdispcs_enter_exception_criticalsection();
	//if (npdisp_cs_execflag) {
	//	npdisp_cs_execflag = 0;
	//	npdispcs_leave_criticalsection();
	//}
	//npdispcs_leave_exception_criticalsection();

	if (npdisp.cmdBuf != 0x3132504e || dat != '1') { // 例外復帰の再実行を認める
		npdisp.cmdBuf = (dat << 24) | (npdisp.cmdBuf >> 8);
		if (npdisp.longjmpnum && npdisp_memory_getLastEIP() != CPU_EIP) {
			// 例外処理中に他が来た場合は放棄
			npdisp.longjmpnum = 0;
			npdisp_memory_clearpreload();
			TRACEOUTF(("NO! %c %08x", (char)dat, CPU_EIP));
		}
	}
	else {
		//i = 0;
		TRACEOUTF(("EXCEPTION!!!!!!!!!!!!: %c", (char)dat));
		//npdisp.cmdBuf = (dat << 24) | (npdisp.cmdBuf >> 8);
	}
	//if (npdisp_debug_seqCounter != 4) {
	//	TRACEOUT(("EXECUTE ERROR! %d %08x", npdisp_debug_seqCounter, CPU_SS));
	//}
	if (npdisp.cmdBuf == 0x3132504e) {
		//TRACEOUT(("EXECUTE %d %08x", npdisp_debug_seqCounter, CPU_SS));
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
	HDC hdc = np2wabwnd.hDCBuf;

	if (!npdispwin.hdc) return 0;

	np2wab.realWidth = npdisp.width;
	np2wab.realHeight = npdisp.height;

	updated = npdisp.updated;
	npdisp.updated = 0;

	if (!updated) return 0;

	//if (!npdispcs_tryenter_criticalsection()) {
	//	npdisp.updated = 1;
	//	return 0;
	//}
	npdispcs_enter_criticalsection();
	BitBlt(hdc, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, SRCCOPY);
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
		//SelectObject(npdispwin.hdc, npdispwin.hOldPalette);
		//DeleteObject(npdispwin.hPalette);
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

		for (int i = 0; i < NELEMENTS(npdispwin.hdcCache); i++) {
			if (npdispwin.hdcCache[i]) {
				DeleteDC(npdispwin.hdcCache[i]);
				npdispwin.hdcCache[i] = NULL;
			}
		}

		if (npdispwin.hEEBrush) {
			DeleteObject(npdispwin.hEEBrush);
			npdispwin.hEEBrush = NULL;
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

	//LOGPALETTE* lpPalette;
	int colors = (npdisp.bpp <= 8) ? (1 << npdisp.bpp) : 0;
	//lpPalette = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) + colors * sizeof(PALETTEENTRY));
	//lpPalette->palVersion = 0x0300;
	//lpPalette->palNumEntries = colors;
	//// 求めたカラー値のRGB表現
	//if (colors == 2) {
	//	// 2色パレットセット
	//	for (i = 0; i < NELEMENTS(npdisp_palette_rgb2); i++) {
	//		lpPalette->palPalEntry[i].peRed = npdisp_palette_rgb2[i & 0xf].r;
	//		lpPalette->palPalEntry[i].peGreen = npdisp_palette_rgb2[i & 0xf].g;
	//		lpPalette->palPalEntry[i].peBlue = npdisp_palette_rgb2[i & 0xf].b;
	//		lpPalette->palPalEntry[i].peFlags = 0;
	//	}
	//}
	//else if (colors == 16) {
	//	// 16色パレットセット
	//	for (i = 0; i < NELEMENTS(npdisp_palette_rgb16); i++) {
	//		lpPalette->palPalEntry[i].peRed = npdisp_palette_rgb16[i & 0xf].r;
	//		lpPalette->palPalEntry[i].peGreen = npdisp_palette_rgb16[i & 0xf].g;
	//		lpPalette->palPalEntry[i].peBlue = npdisp_palette_rgb16[i & 0xf].b;
	//		lpPalette->palPalEntry[i].peFlags = 0;
	//	}
	//}
	//else if (colors == 256) {
	//	// 256色パレットセット
	//	for (i = 0; i < NELEMENTS(npdisp_palette_rgb256); i++) {
	//		lpPalette->palPalEntry[i].peRed = npdisp_palette_rgb256[i & 0xf].r;
	//		lpPalette->palPalEntry[i].peGreen = npdisp_palette_rgb256[i & 0xf].g;
	//		lpPalette->palPalEntry[i].peBlue = npdisp_palette_rgb256[i & 0xf].b;
	//		lpPalette->palPalEntry[i].peFlags = 0;
	//	}
	//}
	//npdispwin.hPalette = CreatePalette(lpPalette);
	//free(lpPalette);
	//npdispwin.hOldPalette = SelectPalette(npdispwin.hdc, npdispwin.hPalette, FALSE);
	//RealizePalette(npdispwin.hdc);

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
		for (i = 0; i < NELEMENTS(npdisp_palette_rgb2); i++) {
			npdispwin.bi.bmiColors[i].rgbRed = npdisp_palette_rgb2[i & 0xf].r;
			npdispwin.bi.bmiColors[i].rgbGreen = npdisp_palette_rgb2[i & 0xf].g;
			npdispwin.bi.bmiColors[i].rgbBlue = npdisp_palette_rgb2[i & 0xf].b;
			npdispwin.bi.bmiColors[i].rgbReserved = 0;
		}
	}
	else if (colors == 16) {
		// 16色パレットセット
		for (i = 0; i < NELEMENTS(npdisp_palette_rgb16); i++) {
			npdispwin.bi.bmiColors[i].rgbRed = npdisp_palette_rgb16[i].r;
			npdispwin.bi.bmiColors[i].rgbGreen = npdisp_palette_rgb16[i].g;
			npdispwin.bi.bmiColors[i].rgbBlue = npdisp_palette_rgb16[i].b;
			npdispwin.bi.bmiColors[i].rgbReserved = 0;
		}
	}
	else if (colors == 256) {
		// 256色パレットセット
		for (i = 0; i < NELEMENTS(npdisp_palette_rgb256); i++) {
			npdispwin.bi.bmiColors[i].rgbRed = npdisp_palette_rgb256[i].r;
			npdispwin.bi.bmiColors[i].rgbGreen = npdisp_palette_rgb256[i].g;
			npdispwin.bi.bmiColors[i].rgbBlue = npdisp_palette_rgb256[i].b;
			npdispwin.bi.bmiColors[i].rgbReserved = 0;
		}
	}

	npdispwin.hBmp = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBits, NULL, 0);
	if (!npdispwin.hBmp || !npdispwin.pBits) {
		SelectPalette(npdispwin.hdc, npdispwin.hOldPalette, FALSE);
		DeleteDC(npdispwin.hdc);
		DeleteObject(npdispwin.hPalette);
		npdispwin.hdc = NULL;
		return;
	}
	npdispwin.hBmpShadow = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBitsShadow, NULL, 0);
	if (!npdispwin.hBmpShadow || !npdispwin.pBitsShadow) {
		DeleteObject(npdispwin.hBmp);
		SelectPalette(npdispwin.hdc, npdispwin.hOldPalette, FALSE);
		DeleteDC(npdispwin.hdc);
		DeleteObject(npdispwin.hPalette);
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

	LOGBRUSH lbrush = { 0 };
	static UINT16 patternBits[8] = {
		0xAAAA, // 1010101010101010
		0x5555, // 0101010101010101
		0xAAAA,
		0x5555,
		0xAAAA,
		0x5555,
		0xAAAA,
		0x5555
	};
	HBITMAP hbm = CreateBitmap(8, 8, 1, 1, patternBits);
	if (hbm) {
		npdispwin.hEEBrush = CreatePatternBrush(hbm);
		DeleteObject(hbm); // ブラシがコピーするので削除OK
	}

	npdispwin.rectShadow.left = 0;
	npdispwin.rectShadow.right = 0;
	npdispwin.rectShadow.top = 0;
	npdispwin.rectShadow.bottom = 0;

	npdispwin.scanlineBrush = NULL;

	npdispwin.hdcCursor = CreateCompatibleDC(NULL);
	npdispwin.hdcCursorMask = CreateCompatibleDC(NULL);

	BitBlt(npdispwin.hdcShadow, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, BLACKNESS);
	BitBlt(npdispwin.hdcBltBuf, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, BLACKNESS);

	LOGFONT lf = { 0 };
	lf.lfHeight = -8;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	lstrcpy(lf.lfFaceName, _T("MS Gothic"));
	npdispwin.hFont = CreateFontIndirect(&lf);
	SelectObject(npdispwin.hdc, npdispwin.hFont); // DEBUG
}

void npdisp_reset(const NP2CFG* pConfig)
{
	int i;
	npdispcs_initialize();

	npdisp_palette_makeTable();

	npdisp_releaseScreen();

	npdisp.enabled = 0;
	npdisp.width = 1024;
	npdisp.height = 720;
	npdisp.bpp = 24;
	npdisp.dpiX = 96;
	npdisp.dpiY = 96;
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

	npdispwin.hEEBrush = NULL;

	npdisp.cursorHotSpotX = 0;
	npdisp.cursorHotSpotY = 0;
	npdisp.cursorWidth = 0;
	npdisp.cursorHeight = 0;

	npdisp_memory_clearpreload();
}
void npdisp_bind(void)
{
	iocore_attachout(0x07e7, npdisp_o7e7);
	iocore_attachout(0x07e8, npdisp_o7e8);
	iocore_attachout(0x07e9, npdisp_o7e9);
	iocore_attachinp(0x07e8, npdisp_i7e8);
	iocore_attachinp(0x07e9, npdisp_i7e9);
}
void npdisp_unbind(void)
{

}

void npdisp_shutdown()
{
	//npdispcs_enter_exception_criticalsection();
	//if (npdisp_cs_execflag) {
	//	npdisp_cs_execflag = 0;
	//	npdispcs_leave_criticalsection();
	//}
	//npdispcs_leave_exception_criticalsection();
	npdisp_releaseScreen();
	npdispcs_shutdown();
}

#endif