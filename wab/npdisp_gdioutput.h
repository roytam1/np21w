/**
 * @file	npdisp_gdioutput.h
 * @brief	Interface of the Neko Project II Display Adapter GDI Output Functions
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP)

#ifdef __cplusplus
extern "C" {
#endif

bool npdisp_func_Output_POLYLINE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_POLYLINE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_SCANLINES(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_SCANLINES(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_RECTANGLE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_RECTANGLE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_WINDPOLYGON(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_ALTPOLYGON(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_POLYGON(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_ELLIPSE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_ELLIPSE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_ARC(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_ARC(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_PIE(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_PIE(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_CHORD(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_CHORD(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_ROUNDRECT(HDC tgtDC, NPDISP_WINDOWS_BMPHDC* bmphdc, NPDISP_PBITMAP* dstPBmp, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);
bool npdisp_func_Output_GetYRange_ROUNDRECT(int* lineBegin, int* numLines, HPEN curPen, HBRUSH curBrush, UINT16 wCount, UINT32 lpPointsAddr);

#ifdef __cplusplus
}
#endif

#endif