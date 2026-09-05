/**
 * @file	npdisp.h
 * @brief	Interface of the Neko Project II Display Adapter
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP)

#define NPDISP_DD_PRIMARY_REGION_SIZE  0x04000000UL
#define NPDISP_DD_OFFSCREEN_OFFSET     0x04000000UL
#define NPDISP_DD_OFFSCREEN_SIZE       0x0c000000UL
#define NPDISP_DD_APERTURE_SIZE        0x10000000UL

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
		UINT32	dataAddr;
		UINT32	cmdBuf;

		UINT16	version;

		UINT8	ioenabled;
		UINT8	enabled;
		UINT32	width;
		UINT32	height;
		UINT32	bpp;
		UINT32	dpiX;
		UINT32	dpiY;
		UINT32	usePalette;

		UINT32	updated;
		UINT32	paletteUpdated;
		UINT16	devType;

		SINT32	cursorX;
		SINT32	cursorY;
		SINT32	cursorWidth;
		SINT32	cursorHeight;
		SINT32  cursorHotSpotX;
		SINT32  cursorHotSpotY;
		UINT32  cursorStride;

		int longjmpnum;

		UINT8	active;

		UINT32  cursorBpp; // カーソルbpp (0の場合はモノクロ1bpp扱い)

		UINT8	isWin9x;

		int longjmpnum_nonfast;

		// プロトコルバージョン5以降
		UINT32 mm_vramPhysicalAddr;
		UINT8* mm_screenPtr;
		UINT32 mm_screenSize;
		UINT32 mm_bmpinfoAddr;
		UINT32 mm_beginAccessAddr;
		UINT32 mm_endAccessAddr;
		UINT32 mm_dcibufAddr;
		UINT32 mm_dciBeginAccessAddr;
		UINT32 mm_dciEndAccessAddr;
		UINT32 mm_dciDestroySurfaceAddr;
		UINT32 mm_vramLinearAddr;
		UINT32 mm_dciEnable;
		UINT32 mm_ddCallbacksAddr;
		UINT32 mm_ddSurfaceCallbacksAddr;
		UINT32 mm_ddPaletteCallbacksAddr;
		UINT32 mm_ddHalInfoAddr;
		UINT32 mm_ddModeInfoAddr;

		// protocol v8以降: DCIからVRAMを参照するためのWin9x selector。
		UINT16 mm_vramSelector;

		// protocol v12以降: 256MiBのDirectDraw apertureとオフスクリーン領域。
		UINT32 mm_ddVidMemAddr;
		UINT8* mm_ddOffscreenPtr;
		UINT32 mm_ddOffscreenSize;
		UINT32 mm_ddScanoutOffset;
		UINT32 mm_ddLastScanoutOffset;
		UINT32 mm_ddPendingFlipOffset;
		UINT32 mm_ddFlipPending;
	} NPDISP;

	extern NPDISP		npdisp;

	void npdispcs_enter_criticalsection(void);
	void npdispcs_leave_criticalsection(void);

	void npdisp_setDirty(int x1, int y1, int x2, int y2);
	void npdisp_setDirtyAll(void);
	void npdisp_resetDirty(void);

	int npdisp_drawGraphic(void);

	void npdisp_exec(void);

	void npdisp_reset(const NP2CFG* pConfig);
	void npdisp_bind(void);
	void npdisp_unbind(void);
	void npdisp_shutdown(void);

#ifdef __cplusplus
}
#endif



#endif