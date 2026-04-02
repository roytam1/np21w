/**
 * @file	npdisp_gdioutput.c
 * @brief	Implementation of the Neko Project II Display Adapter GDI Output Functions
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
#include	"npdisp_gdioutput.h"

bool npdisp_func_Output_POLYLINE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC *bmphdc, NPDISP_PBITMAP *dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	POINT* gdiPoints = (POINT*)malloc(wCount * sizeof(POINT));
	if (gdiPoints) {
		for (int i = 0; i < wCount; i++) {
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
		Polyline(tgtDC, gdiPoints, wCount);
		free(gdiPoints);
		return true;
	}
	return false;
}
bool npdisp_func_Output_GetYRange_POLYLINE(int*lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = (lp.lopnWidth.x + 1) / 2;
	}
	int minY = SHRT_MAX;
	int maxY = 0;
	for (int i = 0; i < wCount; i++) {
		NPDISP_POINT pt;
		if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt.y < minY) minY = pt.y;
			if (pt.y > maxY) maxY = pt.y;
		}
		else {
			break;
		}
		lpPointsAddr += sizeof(NPDISP_POINT);
	}
	if (minY <= maxY) {
		*lineBegin = minY - penWidthOffset;
		*numLines = maxY - minY + 1 + penWidthOffset * 2;
		return true;
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_SCANLINES(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	NPDISP_POINT pt;
	if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
		int beginY = pt.y;
		lpPointsAddr += sizeof(NPDISP_POINT);
		for (int i = 1; i < wCount; i++) {
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
		return true;
	}
	return false;
}
bool npdisp_func_Output_GetYRange_SCANLINES(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	NPDISP_POINT pt;
	if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
		*lineBegin = pt.y;
		*numLines = 1;
		return true;
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_RECTANGLE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			Rectangle(tgtDC, pt1.x - penWidthOffset, pt1.y - penWidthOffset, pt2.x + penWidthOffset, pt2.y + penWidthOffset);
			return true;
		}
	}
	return false;
}
bool npdisp_func_Output_GetYRange_RECTANGLE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt1.y <= pt2.y) {
				*lineBegin = pt1.y;
				*numLines = pt2.y - pt1.y;
			}
			else {
				*lineBegin = pt2.y;
				*numLines = pt1.y - pt2.y;
			}
			*lineBegin -= penWidthOffset;
			*numLines += penWidthOffset * 2;
			return true;
		}
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_WINDPOLYGON(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	POINT* gdiPoints = (POINT*)malloc(wCount * sizeof(POINT));
	if (gdiPoints) {
		for (int i = 0; i < wCount; i++) {
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
		SetPolyFillMode(tgtDC, WINDING);
		Polygon(tgtDC, gdiPoints, wCount);
		free(gdiPoints);
		return true;
	}
	return false;
}
bool npdisp_func_Output_ALTPOLYGON(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	POINT* gdiPoints = (POINT*)malloc(wCount * sizeof(POINT));
	if (gdiPoints) {
		for (int i = 0; i < wCount; i++) {
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
		SetPolyFillMode(tgtDC, ALTERNATE);
		Polygon(tgtDC, gdiPoints, wCount);
		free(gdiPoints);
		return true;
	}
	return false;
}
bool npdisp_func_Output_GetYRange_POLYGON(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = (lp.lopnWidth.x + 1) / 2;
	}
	int minY = SHRT_MAX;
	int maxY = 0;
	for (int i = 0; i < wCount; i++) {
		NPDISP_POINT pt;
		if (npdisp_readMemory(&pt, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt.y < minY) minY = pt.y;
			if (pt.y > maxY) maxY = pt.y;
		}
		else {
			break;
		}
		lpPointsAddr += sizeof(NPDISP_POINT);
	}
	if (minY <= maxY) {
		*lineBegin = minY - penWidthOffset;
		*numLines = maxY - minY + 1 + penWidthOffset * 2;
		return true;
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_ELLIPSE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			Ellipse(tgtDC, pt1.x - penWidthOffset, pt1.y - penWidthOffset, pt2.x + penWidthOffset, pt2.y + penWidthOffset);
			return true;
		}
	}
	return false;
}
bool npdisp_func_Output_GetYRange_ELLIPSE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt1.y <= pt2.y) {
				*lineBegin = pt1.y;
				*numLines = pt2.y - pt1.y;
			}
			else {
				*lineBegin = pt2.y;
				*numLines = pt1.y - pt2.y;
			}
			*lineBegin -= penWidthOffset;
			*numLines += penWidthOffset * 2;
			return true;
		}
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_ARC(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pts[4];
	for (int i = 0; i < NELEMENTS(pts); i++) {
		if (!npdisp_readMemory(pts + i, lpPointsAddr, sizeof(NPDISP_POINT))) {
			return false;
		}
		lpPointsAddr += sizeof(NPDISP_POINT);
	}
	Arc(tgtDC, pts[0].x - penWidthOffset, pts[0].y - penWidthOffset, pts[1].x + penWidthOffset + 1, pts[1].y + penWidthOffset + 1, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
	return true;
}
bool npdisp_func_Output_GetYRange_ARC(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt1.y <= pt2.y) {
				*lineBegin = pt1.y;
				*numLines = pt2.y - pt1.y + 1;
			}
			else {
				*lineBegin = pt2.y;
				*numLines = pt1.y - pt2.y + 1;
			}
			*lineBegin -= penWidthOffset;
			*numLines += penWidthOffset * 2;
			return true;
		}
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_PIE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pts[4];
	for (int i = 0; i < NELEMENTS(pts); i++) {
		if (!npdisp_readMemory(pts + i, lpPointsAddr, sizeof(NPDISP_POINT))) {
			return false;
		}
		lpPointsAddr += sizeof(NPDISP_POINT);
	}
	Pie(tgtDC, pts[0].x - penWidthOffset, pts[0].y - penWidthOffset, pts[1].x + penWidthOffset + 1, pts[1].y + penWidthOffset + 1, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
	return true;
}
bool npdisp_func_Output_GetYRange_PIE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt1.y <= pt2.y) {
				*lineBegin = pt1.y;
				*numLines = pt2.y - pt1.y + 1;
			}
			else {
				*lineBegin = pt2.y;
				*numLines = pt1.y - pt2.y + 1;
			}
			*lineBegin -= penWidthOffset;
			*numLines += penWidthOffset * 2;
			return true;
		}
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_CHORD(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pts[4];
	for (int i = 0; i < NELEMENTS(pts); i++) {
		if (!npdisp_readMemory(pts + i, lpPointsAddr, sizeof(NPDISP_POINT))) {
			return false;
		}
		lpPointsAddr += sizeof(NPDISP_POINT);
	}
	Chord(tgtDC, pts[0].x - penWidthOffset, pts[0].y - penWidthOffset, pts[1].x + penWidthOffset + 1, pts[1].y + penWidthOffset + 1, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
	return true;
}
bool npdisp_func_Output_GetYRange_CHORD(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt1.y <= pt2.y) {
				*lineBegin = pt1.y;
				*numLines = pt2.y - pt1.y + 1;
			}
			else {
				*lineBegin = pt2.y;
				*numLines = pt1.y - pt2.y + 1;
			}
			*lineBegin -= penWidthOffset;
			*numLines += penWidthOffset * 2;
			return true;
		}
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}
bool npdisp_func_Output_ROUNDRECT(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2, pt3;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			lpPointsAddr += sizeof(NPDISP_POINT);
			if (npdisp_readMemory(&pt3, lpPointsAddr, sizeof(NPDISP_POINT))) {
				RoundRect(tgtDC, pt1.x - penWidthOffset, pt1.y - penWidthOffset, pt2.x + penWidthOffset, pt2.y + penWidthOffset, pt3.x - penWidthOffset*2, pt3.y - penWidthOffset * 2);
				return true;
			}
		}
	}
	return false;
}
bool npdisp_func_Output_GetYRange_ROUNDRECT(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr)
{
	int penWidthOffset = 0;
	if (curPen) {
		LOGPEN lp;
		GetObject(curPen, sizeof(LOGPEN), &lp);
		penWidthOffset = lp.lopnWidth.x / 2;
	}
	NPDISP_POINT pt1, pt2;
	if (npdisp_readMemory(&pt1, lpPointsAddr, sizeof(NPDISP_POINT))) {
		lpPointsAddr += sizeof(NPDISP_POINT);
		if (npdisp_readMemory(&pt2, lpPointsAddr, sizeof(NPDISP_POINT))) {
			if (pt1.y <= pt2.y) {
				*lineBegin = pt1.y;
				*numLines = pt2.y - pt1.y;
			}
			else {
				*lineBegin = pt2.y;
				*numLines = pt1.y - pt2.y;
			}
			*lineBegin -= penWidthOffset;
			*numLines += penWidthOffset * 2;
			return true;
		}
	}
	*lineBegin = 0;
	*numLines = 0;
	return false;
}

#endif