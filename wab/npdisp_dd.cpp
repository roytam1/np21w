/**
 * @file	npdisp_dd.cpp
 * @brief	Neko Project II Display Adapter DirectDraw HAL実装
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>

#include	"pccore.h"
#include	"cpucore.h"
#include	"iocore.h"
#include	"nevent.h"

#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_mem.h"
#include	"npdisp_dd.h"

extern NPDISP_WINDOWS npdispwin;

#if 0
static void npdisp_dd_trace(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define TRACEOUT11(s) npdisp_dd_trace s
#else
#define TRACEOUT11(s) (void)s
#endif

bool npdisp_dd_ensureOffscreenBacking(void)
{
	if (npdisp.mm_ddOffscreenPtr && npdisp.mm_ddOffscreenSize == NPDISP_DD_OFFSCREEN_SIZE) {
		return true;
	}
	if (npdisp.mm_ddOffscreenPtr) {
		VirtualFree(npdisp.mm_ddOffscreenPtr, 0, MEM_RELEASE);
		npdisp.mm_ddOffscreenPtr = NULL;
		npdisp.mm_ddOffscreenSize = 0;
	}
	npdisp.mm_ddOffscreenPtr = (UINT8*)VirtualAlloc(NULL, NPDISP_DD_OFFSCREEN_SIZE,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!npdisp.mm_ddOffscreenPtr) {
		TRACEOUT11(("NPDISP11 DD_VRAM_ALLOC failed bytes=%08x", NPDISP_DD_OFFSCREEN_SIZE));
		return false;
	}
	npdisp.mm_ddOffscreenSize = NPDISP_DD_OFFSCREEN_SIZE;
	TRACEOUT11(("NPDISP11 DD_VRAM_ALLOC host=%p bytes=%08x", npdisp.mm_ddOffscreenPtr, npdisp.mm_ddOffscreenSize));
	return true;
}

void npdisp_dd_releaseOffscreenBacking(void)
{
	if (npdisp.mm_ddOffscreenPtr) {
		VirtualFree(npdisp.mm_ddOffscreenPtr, 0, MEM_RELEASE);
	}
	npdisp.mm_ddOffscreenPtr = NULL;
	npdisp.mm_ddOffscreenSize = 0;
	npdisp.mm_ddScanoutOffset = 0;
	npdisp.mm_ddLastScanoutOffset = 0;
	npdisp.mm_ddPendingFlipOffset = 0;
	npdisp.mm_ddFlipPending = 0;
}

static UINT32 npdisp_ddraw_bytesPerPixel(void);

typedef struct {
	UINT16 width;
	UINT16 height;
} NPDISP_DDRESOLUTION;

// NPDISP.INFで有効なDirectDraw対応解像度。
// 1bpp/4bppはバイト単位のsurface処理に対応しないためDirectDrawへ公開しない。
static const NPDISP_DDRESOLUTION npdisp_ddStandardResolutions[] = {
	{ 320, 200 },
	{ 320, 240 },
	{ 640, 480 },
	{ 768, 512 },
	{ 800, 450 },
	{ 800, 480 },
	{ 800, 600 },
	{ 1024, 576 },
	{ 1024, 600 },
	{ 1024, 768 },
	{ 1280, 720 },
	{ 1280, 800 },
	{ 1280, 960 },
	{ 1280, 1024 },
	{ 1600, 900 },
	{ 1600, 1000 },
	{ 1600, 1200 },
	{ 1920, 1080 },
	{ 1920, 1200 }
};

static void npdisp_dd_fillModeInfo(NPDISP_DDHALMODEINFO* mode, UINT32 width, UINT32 height, UINT32 bpp, bool rgb555)
{
	if (!mode) return;
	memset(mode, 0, sizeof(*mode));
	mode->dwWidth = width;
	mode->dwHeight = height;
	mode->dwBPP = (bpp == 15) ? 16 : bpp;
	mode->lPitch = (SINT32)(((width * bpp + 31) / 32) * 4);
	mode->wRefreshRate = 0;

	if (bpp == 8) {
		mode->wFlags = NPDISP_DDMODEINFO_PALETTIZED;
	}
	else if (rgb555 || bpp == 15) {
		mode->dwRBitMask = 0x00007c00;
		mode->dwGBitMask = 0x000003e0;
		mode->dwBBitMask = 0x0000001f;
	}
	else if (bpp == 16) {
		mode->dwRBitMask = 0x0000f800;
		mode->dwGBitMask = 0x000007e0;
		mode->dwBBitMask = 0x0000001f;
	}
	else if (bpp == 24 || bpp == 32) {
		mode->dwRBitMask = 0x00ff0000;
		mode->dwGBitMask = 0x0000ff00;
		mode->dwBBitMask = 0x000000ff;
	}
}

static bool npdisp_dd_modeEquals(const NPDISP_DDHALMODEINFO* a, const NPDISP_DDHALMODEINFO* b)
{
	return a && b &&
		a->dwWidth == b->dwWidth &&
		a->dwHeight == b->dwHeight &&
		a->lPitch == b->lPitch &&
		a->dwBPP == b->dwBPP &&
		a->wFlags == b->wFlags &&
		a->dwRBitMask == b->dwRBitMask &&
		a->dwGBitMask == b->dwGBitMask &&
		a->dwBBitMask == b->dwBBitMask &&
		a->dwAlphaBitMask == b->dwAlphaBitMask;
}

// DirectDrawの共有状態はDISPLAYの固定共有Dataセグメントに置く。
// NP2Initializeで受け取ったfar pointerをドライバの有効期間中そのまま使用する。
static UINT32 npdisp_dd_currentDataPtr(UINT32 savedPtr)
{
	return savedPtr;
}

// 共有Dataセグメント上のcallback tableを有効な内容へ揃える。
static bool npdisp_dd_ensureCurrentCallbackTable(UINT32 currentAddr, UINT32 savedAddr, UINT32 expectedSize, UINT32 tableBytes, const char *name)
{
	UINT32 currentSize = 0;
	UINT32 savedSize = 0;
	UINT32 verifySize = 0;
	UINT8 table[64];

	if (!currentAddr || !savedAddr || !tableBytes || tableBytes > sizeof(table)) {
		TRACEOUT11(("NPDISP11 DD_CB_SYNC %s reject current=%08x saved=%08x bytes=%u",
			name, currentAddr, savedAddr, tableBytes));
		return false;
	}

	if (npdisp_readMemory(&currentSize, currentAddr, sizeof(currentSize)) && currentSize == expectedSize) {
		TRACEOUT11(("NPDISP11 DD_CB_SYNC %s keep addr=%08x size=%u", name, currentAddr, currentSize));
		return true;
	}

	if (!npdisp_readMemory(&savedSize, savedAddr, sizeof(savedSize)) || savedSize != expectedSize) {
		TRACEOUT11(("NPDISP11 DD_CB_SYNC %s source-invalid current=%08x cursize=%08x saved=%08x savedsize=%08x expect=%u",
			name, currentAddr, currentSize, savedAddr, savedSize, expectedSize));
		return false;
	}

	memset(table, 0, sizeof(table));
	if (!npdisp_readMemory(table, savedAddr, tableBytes) ||
		!npdisp_writeMemory(table, currentAddr, tableBytes) ||
		!npdisp_readMemory(&verifySize, currentAddr, sizeof(verifySize)) ||
		verifySize != expectedSize) {
		TRACEOUT11(("NPDISP11 DD_CB_SYNC %s copy-failed current=%08x cursize=%08x saved=%08x savedsize=%08x verify=%08x",
			name, currentAddr, currentSize, savedAddr, savedSize, verifySize));
		return false;
	}

	TRACEOUT11(("NPDISP11 DD_CB_SYNC %s copied current=%08x oldsize=%08x saved=%08x size=%u",
		name, currentAddr, currentSize, savedAddr, verifySize));
	return true;
}

static bool npdisp_dd_prepareCurrentCallbackTables(UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr)
{
	if (!npdisp_dd_ensureCurrentCallbackTable(ddCallbacksAddr, npdisp.mm_ddCallbacksAddr,
		sizeof(NPDISP_DDHAL_DDCALLBACKS), sizeof(NPDISP_DDHAL_DDCALLBACKS), "dd")) return false;
	if (!npdisp_dd_ensureCurrentCallbackTable(ddSurfaceCallbacksAddr, npdisp.mm_ddSurfaceCallbacksAddr,
		sizeof(NPDISP_DDHAL_DDSURFACECALLBACKS), sizeof(NPDISP_DDHAL_DDSURFACECALLBACKS), "surface")) return false;
	if (ddPaletteCallbacksAddr && !npdisp_dd_ensureCurrentCallbackTable(ddPaletteCallbacksAddr, npdisp.mm_ddPaletteCallbacksAddr,
		sizeof(NPDISP_DDHAL_DDPALETTECALLBACKS), sizeof(NPDISP_DDHAL_DDPALETTECALLBACKS), "palette")) return false;
	return true;
}

// 表示モードに依存するDDHALINFOだけを更新する。
// ReEnableではDriverInitを再実行しないため、HINSTANCEとcallback情報は維持する。
static bool npdisp_dd_configureVideoMemory(NPDISP_DDHALINFO* halInfo)
{
	NPDISP_VIDMEM heap = { 0 };
	UINT32 heapStart;
	UINT32 heapEnd;

	if (!halInfo) return false;

	halInfo->vmiData.dwOffscreenAlign = 0;
	halInfo->vmiData.dwNumHeaps = 0;
	halInfo->vmiData.pvmList = 0;
	halInfo->ddCaps.dwVidMemTotal = 0;
	halInfo->ddCaps.dwVidMemFree = 0;
	halInfo->ddCaps.ddsCaps.dwCaps &= ~(NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_FLIP |
		NPDISP_DDSCAPS_COMPLEX | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_BACKBUFFER |
		NPDISP_DDSCAPS_VIDEOMEMORY);

	if (npdisp.version < 12 || !npdisp.mm_ddVidMemAddr || !npdisp.mm_vramLinearAddr ||
		!npdisp.mm_ddOffscreenPtr || npdisp.mm_ddOffscreenSize != NPDISP_DD_OFFSCREEN_SIZE) {
		return true;
	}

	heapStart = npdisp.mm_vramLinearAddr + NPDISP_DD_OFFSCREEN_OFFSET;
	heapEnd = npdisp.mm_vramLinearAddr + NPDISP_DD_APERTURE_SIZE - 1;
	if (heapStart < npdisp.mm_vramLinearAddr || heapEnd < heapStart) {
		TRACEOUT11(("NPDISP11 DD_VIDMEM_REJECT linear wrap base=%08x start=%08x end=%08x",
			npdisp.mm_vramLinearAddr, heapStart, heapEnd));
		return false;
	}

	heap.dwFlags = NPDISP_VIDMEM_ISLINEAR;
	heap.fpStart = heapStart;
	heap.fpEnd = heapEnd;
	heap.ddsCaps.dwCaps = 0;
	heap.ddsCapsAlt.dwCaps = 0;
	heap.lpHeap = 0;
	if (!npdisp_writeMemory(&heap, npdisp.mm_ddVidMemAddr, sizeof(heap))) {
		TRACEOUT11(("NPDISP11 DD_VIDMEM_REJECT descriptor write addr=%08x", npdisp.mm_ddVidMemAddr));
		return false;
	}

	// オフスクリーンsurfaceはDWORD境界のscan lineを使用する。
	// モード変更後もsurfaceアドレスが変わらないようheap位置は固定する。
	halInfo->vmiData.dwOffscreenAlign = 4;
	halInfo->vmiData.dwNumHeaps = 1;
	halInfo->vmiData.pvmList = npdisp.mm_ddVidMemAddr;
	halInfo->ddCaps.dwVidMemTotal = NPDISP_DD_OFFSCREEN_SIZE;
	halInfo->ddCaps.dwVidMemFree = NPDISP_DD_OFFSCREEN_SIZE;
	halInfo->ddCaps.ddsCaps.dwCaps |= NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_VIDEOMEMORY;
	if (npdisp.version >= 13) {
		halInfo->ddCaps.ddsCaps.dwCaps |= NPDISP_DDSCAPS_FLIP | NPDISP_DDSCAPS_COMPLEX |
			NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_BACKBUFFER;
	}

	TRACEOUT11(("NPDISP11 DD_VIDMEM heap=%08x start=%08x end=%08x bytes=%08x",
		npdisp.mm_ddVidMemAddr, heap.fpStart, heap.fpEnd, NPDISP_DD_OFFSCREEN_SIZE));
	return true;
}

bool npdisp_dd_rebuildModeDependentHalInfo(UINT32 lpPDeviceAddr)
{
	const UINT32 ddHalInfoAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddHalInfoAddr);
	const UINT32 ddModeInfoAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddModeInfoAddr);
	NPDISP_DDHALINFO halInfo = { 0 };
	NPDISP_DDHALMODEINFO modeInfo[NPDISP_DD_MAX_MODES] = { 0 };
	NPDISP_DDHALMODEINFO currentMode = { 0 };
	UINT32 modeCount = 0;
	UINT32 currentModeIndex = 0xffffffffUL;

	if (!ddHalInfoAddr || !ddModeInfoAddr) return true;
	if (!npdisp_readMemory(&halInfo, ddHalInfoAddr, sizeof(halInfo))) return false;

	// DirectDrawオブジェクトが無い場合は再登録する情報も無い。
	if (halInfo.dwSize != sizeof(halInfo) || !halInfo.hInstance) {
		TRACEOUT11(("NPDISP11 DD_REENABLE_HAL inactive size=%u hinst=%08x", halInfo.dwSize, halInfo.hInstance));
		return true;
	}
	if (!npdisp_ddraw_bytesPerPixel()) {
		TRACEOUT11(("NPDISP11 DD_REENABLE_HAL unsupported bpp=%u", npdisp.bpp));
		return false;
	}

	halInfo.vmiData.fpPrimary = npdisp.mm_vramLinearAddr;
	halInfo.vmiData.dwDisplayWidth = npdisp.width;
	halInfo.vmiData.dwDisplayHeight = npdisp.height;
	halInfo.vmiData.lDisplayPitch = (SINT32)npdispwin.stride;
	memset(&halInfo.vmiData.ddpfDisplay, 0, sizeof(halInfo.vmiData.ddpfDisplay));
	halInfo.vmiData.ddpfDisplay.dwSize = sizeof(NPDISP_DDPIXELFORMAT);
	halInfo.vmiData.ddpfDisplay.dwFlags = NPDISP_DDPF_RGB;
	halInfo.vmiData.ddpfDisplay.dwRGBBitCount = (npdisp.bpp == 15) ? 16 : npdisp.bpp;
	if (npdisp.bpp == 8) {
		halInfo.vmiData.ddpfDisplay.dwFlags |= NPDISP_DDPF_PALETTEINDEXED8;
	}
	else if (npdisp.bpp == 15) {
		halInfo.vmiData.ddpfDisplay.dwRBitMask = 0x00007c00;
		halInfo.vmiData.ddpfDisplay.dwGBitMask = 0x000003e0;
		halInfo.vmiData.ddpfDisplay.dwBBitMask = 0x0000001f;
	}
	else if (npdisp.bpp == 16) {
		halInfo.vmiData.ddpfDisplay.dwRBitMask = 0x0000f800;
		halInfo.vmiData.ddpfDisplay.dwGBitMask = 0x000007e0;
		halInfo.vmiData.ddpfDisplay.dwBBitMask = 0x0000001f;
	}
	else if (npdisp.bpp == 24 || npdisp.bpp == 32) {
		halInfo.vmiData.ddpfDisplay.dwRBitMask = 0x00ff0000;
		halInfo.vmiData.ddpfDisplay.dwGBitMask = 0x0000ff00;
		halInfo.vmiData.ddpfDisplay.dwBBitMask = 0x000000ff;
	}

	static const UINT32 ddBpps[] = { 8, 16, 24, 32 };
	for (UINT32 bi = 0; bi < sizeof(ddBpps) / sizeof(ddBpps[0]); ++bi) {
		for (UINT32 ri = 0; ri < sizeof(npdisp_ddStandardResolutions) / sizeof(npdisp_ddStandardResolutions[0]); ++ri) {
			if (modeCount >= NPDISP_DD_MAX_MODES) break;
			npdisp_dd_fillModeInfo(&modeInfo[modeCount],
				npdisp_ddStandardResolutions[ri].width,
				npdisp_ddStandardResolutions[ri].height,
				ddBpps[bi], false);
			modeCount++;
		}
	}

	npdisp_dd_fillModeInfo(&currentMode, npdisp.width, npdisp.height, npdisp.bpp, npdisp.bpp == 15);
	currentMode.lPitch = (SINT32)npdispwin.stride;
	currentMode.dwBPP = halInfo.vmiData.ddpfDisplay.dwRGBBitCount;
	currentMode.wFlags = (npdisp.bpp == 8) ? NPDISP_DDMODEINFO_PALETTIZED : 0;
	currentMode.dwRBitMask = halInfo.vmiData.ddpfDisplay.dwRBitMask;
	currentMode.dwGBitMask = halInfo.vmiData.ddpfDisplay.dwGBitMask;
	currentMode.dwBBitMask = halInfo.vmiData.ddpfDisplay.dwBBitMask;
	currentMode.dwAlphaBitMask = halInfo.vmiData.ddpfDisplay.dwRGBAlphaBitMask;

	for (UINT32 i = 0; i < modeCount; ++i) {
		if (npdisp_dd_modeEquals(&modeInfo[i], &currentMode)) {
			currentModeIndex = i;
			break;
		}
	}
	if (currentModeIndex == 0xffffffffUL && modeCount < NPDISP_DD_MAX_MODES) {
		currentModeIndex = modeCount;
		modeInfo[modeCount++] = currentMode;
	}
	if (currentModeIndex == 0xffffffffUL || !modeCount) return false;

	halInfo.dwModeIndex = currentModeIndex;
	halInfo.dwNumModes = modeCount;
	halInfo.lpModeInfo = ddModeInfoAddr;
	halInfo.lpPDevice = lpPDeviceAddr;
	if (!npdisp_dd_configureVideoMemory(&halInfo)) return false;

	if (!npdisp_writeMemory(modeInfo, ddModeInfoAddr, modeCount * sizeof(NPDISP_DDHALMODEINFO)) ||
		!npdisp_writeMemory(&halInfo, ddHalInfoAddr, sizeof(halInfo))) {
		return false;
	}

	TRACEOUT11(("NPDISP11 DD_REENABLE_HAL size=%ux%u pitch=%d bpp=%u primary=%08x pdevice=%08x modeidx=%u modes=%u hinst=%08x",
		npdisp.width, npdisp.height, npdispwin.stride, npdisp.bpp, halInfo.vmiData.fpPrimary,
		halInfo.lpPDevice, halInfo.dwModeIndex, halInfo.dwNumModes, halInfo.hInstance));
	return true;
}

// DirectDraw HALサーフェス処理

static UINT32 npdisp_ddraw_bytesPerPixel(void)
{
	if (npdisp.bpp == 8) return 1;
	if (npdisp.bpp == 15 || npdisp.bpp == 16) return 2;
	if (npdisp.bpp == 24) return 3;
	if (npdisp.bpp == 32) return 4;
	return 0;
}

static int npdisp_ddraw_readGuest(void* dst, UINT32 addr, int size, bool flatAddress)
{
	return flatAddress ? npdisp_readLinearMemory(dst, addr, size) : npdisp_readMemory(dst, addr, size);
}

static int npdisp_ddraw_writeGuest(void* src, UINT32 addr, int size, bool flatAddress)
{
	return flatAddress ? npdisp_writeLinearMemory(src, addr, size) : npdisp_writeMemory(src, addr, size);
}

static UINT32 npdisp_ddraw_readGuest32(UINT32 addr, bool flatAddress)
{
	UINT32 value = 0;
	if (!npdisp_ddraw_readGuest(&value, addr, sizeof(value), flatAddress)) return 0;
	return value;
}

typedef struct {
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl;
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl;
	bool primaryObject;
	bool visible;
	bool systemMemory;
	UINT8* hostBase;
	UINT32 linearBase;
	UINT32 apertureOffset;
	UINT32 width;
	UINT32 height;
	SINT32 pitch;
	SINT32 hostOriginX;
	SINT32 hostOriginY;
} NPDISP_DDSURFACE_VIEW;

static bool npdisp_ddraw_resolveVideoAddress(UINT32 fpVidMem, SINT32 pitch, UINT32 height,
	UINT8** hostBase, UINT32* linearBase, UINT32* apertureOffset)
{
	UINT32 offset;
	UINT64 bytes;

	if (!hostBase || !linearBase || !apertureOffset || !npdisp.mm_vramLinearAddr || pitch <= 0 || !height) {
		return false;
	}

	// fpVidMem==0は固定primary領域を表す。Flip後はfpVidMemが入れ替わるため、
	// memory種別はsurfaceの役割ではなくアドレスから判定する。
	if (fpVidMem == 0 || fpVidMem == npdisp.mm_vramLinearAddr) {
		bytes = (UINT64)(UINT32)pitch * (UINT64)height;
		if (!npdisp.mm_screenPtr || bytes > NPDISP_DD_PRIMARY_REGION_SIZE) return false;
		*hostBase = npdisp.mm_screenPtr;
		*linearBase = npdisp.mm_vramLinearAddr;
		*apertureOffset = 0;
		return true;
	}

	if (fpVidMem < npdisp.mm_vramLinearAddr) return false;
	offset = fpVidMem - npdisp.mm_vramLinearAddr;
	if (offset < NPDISP_DD_OFFSCREEN_OFFSET || offset >= NPDISP_DD_APERTURE_SIZE ||
		!npdisp.mm_ddOffscreenPtr || npdisp.mm_ddOffscreenSize != NPDISP_DD_OFFSCREEN_SIZE) {
		return false;
	}
	bytes = (UINT64)(UINT32)pitch * (UINT64)height;
	if (bytes > NPDISP_DD_OFFSCREEN_SIZE ||
		(UINT64)(offset - NPDISP_DD_OFFSCREEN_OFFSET) + bytes > NPDISP_DD_OFFSCREEN_SIZE) {
		return false;
	}

	*hostBase = npdisp.mm_ddOffscreenPtr + (offset - NPDISP_DD_OFFSCREEN_OFFSET);
	*linearBase = fpVidMem;
	*apertureOffset = offset;
	return true;
}

static bool npdisp_ddraw_pixelFormatMatchesDisplay(const NPDISP_DDPIXELFORMAT* pf)
{
	const UINT32 displayBits = (npdisp.bpp == 15) ? 16 : npdisp.bpp;
	if (!pf || !pf->dwSize || !pf->dwFlags) return true;
	if (pf->dwSize < sizeof(*pf) || (pf->dwFlags & NPDISP_DDPF_FOURCC) || pf->dwRGBBitCount != displayBits) return false;
	if (npdisp.bpp == 8) return (pf->dwFlags & NPDISP_DDPF_PALETTEINDEXED8) != 0;
	if (!(pf->dwFlags & NPDISP_DDPF_RGB)) return false;
	if (npdisp.bpp == 15) return pf->dwRBitMask == 0x00007c00 && pf->dwGBitMask == 0x000003e0 &&
		pf->dwBBitMask == 0x0000001f && !pf->dwRGBAlphaBitMask;
	if (npdisp.bpp == 16) return pf->dwRBitMask == 0x0000f800 && pf->dwGBitMask == 0x000007e0 &&
		pf->dwBBitMask == 0x0000001f && !pf->dwRGBAlphaBitMask;
	if (npdisp.bpp == 24 || npdisp.bpp == 32) return pf->dwRBitMask == 0x00ff0000 && pf->dwGBitMask == 0x0000ff00 &&
		pf->dwBBitMask == 0x000000ff && !pf->dwRGBAlphaBitMask;
	return false;
}

static bool npdisp_ddraw_getSurfaceEx(UINT32 lpSurfaceAddr, NPDISP_DDSURFACE_VIEW* view, bool flatAddress, bool allowSystemMemory)
{
	NPDISP_DDSURFACE_VIEW v = { 0 };
	UINT32 caps;

	if (!lpSurfaceAddr || !view ||
		!npdisp_ddraw_readGuest(&v.lcl, lpSurfaceAddr, sizeof(v.lcl), flatAddress) ||
		!v.lcl.lpGbl ||
		!npdisp_ddraw_readGuest(&v.gbl, v.lcl.lpGbl, sizeof(v.gbl), flatAddress)) {
		return false;
	}

	caps = v.lcl.ddsCaps.dwCaps;
	if (caps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT fullGbl = { 0 };
		v.width = v.gbl.wWidth;
		v.height = v.gbl.wHeight;
		v.pitch = v.gbl.lPitch;
		const UINT64 rowBytes = (UINT64)v.width * npdisp_ddraw_bytesPerPixel();
		if (!allowSystemMemory || (caps & (NPDISP_DDSCAPS_PRIMARYSURFACE | NPDISP_DDSCAPS_VIDEOMEMORY |
			NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ||
			!v.gbl.fpVidMem || !v.width || !v.height || v.pitch <= 0 || !rowBytes || rowBytes > (UINT32)v.pitch ||
			!npdisp_ddraw_readGuest(&fullGbl, v.lcl.lpGbl, sizeof(fullGbl), flatAddress) ||
			!npdisp_ddraw_pixelFormatMatchesDisplay(&fullGbl.ddpfSurface)) {
			return false;
		}
		v.primaryObject = false;
		v.visible = false;
		v.systemMemory = true;
		v.hostBase = NULL;
		v.linearBase = v.gbl.fpVidMem;
		v.apertureOffset = 0xffffffffUL;
		*view = v;
		return true;
	}
	if (!(caps & (NPDISP_DDSCAPS_PRIMARYSURFACE | NPDISP_DDSCAPS_OFFSCREENPLAIN |
		NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP))) {
		return false;
	}

	v.width = v.gbl.wWidth ? v.gbl.wWidth : npdisp.width;
	v.height = v.gbl.wHeight ? v.gbl.wHeight : npdisp.height;
	v.pitch = v.gbl.lPitch ? v.gbl.lPitch : (SINT32)npdispwin.stride;
	v.primaryObject = (caps & NPDISP_DDSCAPS_PRIMARYSURFACE) != 0;
	if (!npdisp_ddraw_resolveVideoAddress(v.gbl.fpVidMem, v.pitch, v.height,
		&v.hostBase, &v.linearBase, &v.apertureOffset)) {
		return false;
	}
	v.visible = (v.apertureOffset == npdisp.mm_ddScanoutOffset);

	*view = v;
	return true;
}

static bool npdisp_ddraw_getSurface(UINT32 lpSurfaceAddr, NPDISP_DDSURFACE_VIEW* view, bool flatAddress)
{
	return npdisp_ddraw_getSurfaceEx(lpSurfaceAddr, view, flatAddress, false);
}

// hostBase上のlogical surface座標を実アドレスへ変換する。
static UINT8* npdisp_ddraw_surfacePtr(const NPDISP_DDSURFACE_VIEW* view, SINT32 x, SINT32 y, UINT32 bytesPerPixel)
{
	return view->hostBase + (size_t)(y - view->hostOriginY) * (size_t)view->pitch +
		(size_t)(x - view->hostOriginX) * bytesPerPixel;
}

// destinationの実描画矩形から、nearest-neighbor変換で参照されるsource範囲を求める。
static bool npdisp_ddraw_mapDestRectToSource(NPDISP_DDRECTL* srcRect, const NPDISP_DDRECTL* dstRect, const NPDISP_DDRECTL* mapDst, const NPDISP_DDRECTL* mapSrc)
{
	const SINT32 dstWidth = mapDst ? mapDst->right - mapDst->left : 0;
	const SINT32 dstHeight = mapDst ? mapDst->bottom - mapDst->top : 0;
	const SINT32 srcWidth = mapSrc ? mapSrc->right - mapSrc->left : 0;
	const SINT32 srcHeight = mapSrc ? mapSrc->bottom - mapSrc->top : 0;
	UINT64 x0, x1, y0, y1;

	if (!srcRect || !dstRect || !mapDst || !mapSrc || dstWidth <= 0 || dstHeight <= 0 ||
		srcWidth <= 0 || srcHeight <= 0 || dstRect->left < mapDst->left || dstRect->top < mapDst->top ||
		dstRect->right > mapDst->right || dstRect->bottom > mapDst->bottom ||
		dstRect->left >= dstRect->right || dstRect->top >= dstRect->bottom) return false;

	x0 = (UINT64)(dstRect->left - mapDst->left);
	x1 = (UINT64)(dstRect->right - 1 - mapDst->left);
	y0 = (UINT64)(dstRect->top - mapDst->top);
	y1 = (UINT64)(dstRect->bottom - 1 - mapDst->top);
	srcRect->left = mapSrc->left + (SINT32)((x0 * (UINT32)srcWidth) / (UINT32)dstWidth);
	srcRect->right = mapSrc->left + (SINT32)((x1 * (UINT32)srcWidth) / (UINT32)dstWidth) + 1;
	srcRect->top = mapSrc->top + (SINT32)((y0 * (UINT32)srcHeight) / (UINT32)dstHeight);
	srcRect->bottom = mapSrc->top + (SINT32)((y1 * (UINT32)srcHeight) / (UINT32)dstHeight) + 1;
	return srcRect->left < srcRect->right && srcRect->top < srcRect->bottom;
}

// System-memory surfaceの指定矩形だけをtight-packed host bufferへmirrorする。
static bool npdisp_ddraw_mirrorSystemRect(NPDISP_DDSURFACE_VIEW* view, const NPDISP_DDRECTL* rect, UINT32 bytesPerPixel, bool readExisting, UINT8** storage)
{
	UINT64 rowBytes64;
	UINT64 totalBytes;
	UINT8* buffer;

	if (!view || !rect || !storage) return false;
	*storage = NULL;
	if (!view->systemMemory) return true;
	if (rect->left < 0 || rect->top < 0 || rect->right > (SINT32)view->width || rect->bottom > (SINT32)view->height ||
		rect->left >= rect->right || rect->top >= rect->bottom) return false;
	rowBytes64 = (UINT64)(rect->right - rect->left) * bytesPerPixel;
	totalBytes = rowBytes64 * (UINT32)(rect->bottom - rect->top);
	if (!rowBytes64 || rowBytes64 > 0x7fffffffUL || !totalBytes || totalBytes > 0x7fffffffUL ||
		view->gbl.lPitch <= 0 || (UINT64)view->width * bytesPerPixel > (UINT32)view->gbl.lPitch) return false;
	buffer = (UINT8*)malloc((size_t)totalBytes);
	if (!buffer) return false;

	if (readExisting) {
		for (SINT32 y = rect->top; y < rect->bottom; ++y) {
			const UINT64 guestAddr = (UINT64)view->gbl.fpVidMem + (UINT64)y * (UINT32)view->gbl.lPitch +
				(UINT64)rect->left * bytesPerPixel;
			UINT8* d = buffer + (size_t)(y - rect->top) * (size_t)rowBytes64;
			if (guestAddr > 0xffffffffUL || guestAddr + rowBytes64 > ((UINT64)1 << 32) ||
				!npdisp_readLinearMemory(d, (UINT32)guestAddr, (int)rowBytes64)) {
				free(buffer);
				return false;
			}
		}
	}

	view->hostBase = buffer;
	view->pitch = (SINT32)rowBytes64;
	view->hostOriginX = rect->left;
	view->hostOriginY = rect->top;
	*storage = buffer;
	return true;
}

// System-memory destinationの指定矩形だけをguest flat memoryへ書き戻す。
static bool npdisp_ddraw_commitSystemRect(const NPDISP_DDSURFACE_VIEW* view, const NPDISP_DDRECTL* rect, UINT32 bytesPerPixel)
{
	UINT64 rowBytes64;
	if (!view || !rect || !view->systemMemory || !view->hostBase || rect->left < view->hostOriginX ||
		rect->top < view->hostOriginY || rect->left >= rect->right || rect->top >= rect->bottom) return false;
	rowBytes64 = (UINT64)(rect->right - rect->left) * bytesPerPixel;
	if (!rowBytes64 || rowBytes64 > 0x7fffffffUL) return false;
	for (SINT32 y = rect->top; y < rect->bottom; ++y) {
		const UINT64 guestAddr = (UINT64)view->gbl.fpVidMem + (UINT64)y * (UINT32)view->gbl.lPitch +
			(UINT64)rect->left * bytesPerPixel;
		UINT8* q = npdisp_ddraw_surfacePtr(view, rect->left, y, bytesPerPixel);
		if (guestAddr > 0xffffffffUL || guestAddr + rowBytes64 > ((UINT64)1 << 32) ||
			!npdisp_writeLinearMemory(q, (UINT32)guestAddr, (int)rowBytes64)) return false;
	}
	return true;
}

bool npdisp_ddraw_isScanoutOffsetValid(UINT32 offset)
{
	UINT64 screenBytes;
	if (!offset) return npdisp.mm_screenPtr != NULL;
	if (offset < NPDISP_DD_OFFSCREEN_OFFSET || offset >= NPDISP_DD_APERTURE_SIZE ||
		!npdisp.mm_ddOffscreenPtr || npdisp.mm_ddOffscreenSize != NPDISP_DD_OFFSCREEN_SIZE) {
		return false;
	}
	screenBytes = (UINT64)npdispwin.stride * (UINT64)npdisp.height;
	return (UINT64)(offset - NPDISP_DD_OFFSCREEN_OFFSET) + screenBytes <= NPDISP_DD_OFFSCREEN_SIZE;
}

UINT8* npdisp_ddraw_getScanoutHostBase(void)
{
	if (!npdisp_ddraw_isScanoutOffsetValid(npdisp.mm_ddScanoutOffset)) return NULL;
	if (!npdisp.mm_ddScanoutOffset) return npdisp.mm_screenPtr;
	return npdisp.mm_ddOffscreenPtr + (npdisp.mm_ddScanoutOffset - NPDISP_DD_OFFSCREEN_OFFSET);
}

static void npdisp_ddraw_commitPendingFlip(void)
{
	UINT32 targetOffset;

	if (!npdisp.mm_ddFlipPending) return;
	targetOffset = npdisp.mm_ddPendingFlipOffset;
	npdisp.mm_ddFlipPending = 0;
	npdisp.mm_ddPendingFlipOffset = 0;
	if (!npdisp_ddraw_isScanoutOffsetValid(targetOffset)) {
		TRACEOUT11(("NPDISP11 DD_FLIP_COMMIT_CANCEL target=%08x", targetOffset));
		return;
	}

	npdisp.mm_ddScanoutOffset = targetOffset;
	npdisp.mm_ddLastScanoutOffset = targetOffset;
	npdisp_setDirtyAll();
	npdisp.updated = 1;
	TRACEOUT11(("NPDISP11 DD_FLIP_COMMIT target=%08x", targetOffset));
}

void npdisp_dd_vsync(void)
{
	if (!npdisp.active) return;
	npdispcs_enter_criticalsection();
	npdisp_ddraw_commitPendingFlip();
	npdispcs_leave_criticalsection();
}

static bool npdisp_ddraw_normalizeSurfaceRect(NPDISP_DDRECTL* r, UINT32 width, UINT32 height)
{
	if (!r || !width || !height) return false;
	if (r->left < 0) r->left = 0;
	if (r->top < 0) r->top = 0;
	if (r->right > (SINT32)width) r->right = (SINT32)width;
	if (r->bottom > (SINT32)height) r->bottom = (SINT32)height;
	return r->left < r->right && r->top < r->bottom;
}

static void npdisp_ddraw_setLockRect(const NPDISP_DDRECTL* r)
{
	if (r) {
		npdispwin.ddrawDirtyRect.left = r->left;
		npdispwin.ddrawDirtyRect.top = r->top;
		npdispwin.ddrawDirtyRect.right = r->right;
		npdispwin.ddrawDirtyRect.bottom = r->bottom;
	}
	else {
		npdispwin.ddrawDirtyRect.left = 0;
		npdispwin.ddrawDirtyRect.top = 0;
		npdispwin.ddrawDirtyRect.right = npdisp.width;
		npdispwin.ddrawDirtyRect.bottom = npdisp.height;
	}
}

static bool npdisp_ddraw_calcSurfaceAllocation(UINT32 width, UINT32 height, UINT32* pitch, UINT32* bytes)
{
	UINT64 rowBytes;
	UINT64 surfaceBytes;
	const UINT32 bytesPerPixel = npdisp_ddraw_bytesPerPixel();

	if (!pitch || !bytes || !bytesPerPixel || !width || !height) return false;
	rowBytes = (UINT64)width * bytesPerPixel;
	if (!rowBytes || rowBytes > 0xfffffffcUL) return false;
	rowBytes = (rowBytes + 3) & ~((UINT64)3);
	surfaceBytes = rowBytes * height;
	if (!surfaceBytes || surfaceBytes > NPDISP_DD_OFFSCREEN_SIZE || surfaceBytes > (UINT64)0xffffffffUL) return false;
	*pitch = (UINT32)rowBytes;
	*bytes = (UINT32)surfaceBytes;
	return true;
}

static bool npdisp_ddraw_getRasterState(UINT32* scanLine, bool* inVBlank)
{
	const bool vblank = (gdc.vsync & 0x20) != 0;
	SINT32 remain;
	UINT32 elapsed;
	UINT32 line;

	if (!scanLine || !inVBlank || !npdisp.height || !gdc.dispclock) return false;
	*inVBlank = vblank;
	if (vblank) {
		*scanLine = npdisp.height;
		return true;
	}

	remain = nevent_getremain(NEVENT_FLAMES);
	if (remain < 0) return false;
	if ((UINT32)remain >= gdc.dispclock) elapsed = 0;
	else elapsed = gdc.dispclock - (UINT32)remain;
	if (elapsed >= gdc.dispclock) elapsed = gdc.dispclock - 1;

	line = (UINT32)(((UINT64)elapsed * npdisp.height) / gdc.dispclock);
	if (line >= npdisp.height) line = npdisp.height - 1;
	*scanLine = line;
	return true;
}

static UINT32 npdisp_func_DD_WaitForVerticalBlank(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_WAITFORVERTICALBLANKDATA data = { 0 };
	UINT32 scanLine = 0;
	bool inVBlank = false;

	if (!lpDataAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!npdisp_ddraw_getRasterState(&scanLine, &inVBlank)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	if (data.dwFlags == NPDISP_DDWAITVB_I_TESTVB) {
		data.bIsInVB = inVBlank ? 1 : 0;
		data.ddRVal = 0;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	// TESTVBのみ処理し、BLOCKBEGIN/BLOCKENDは未処理として返す。
	return NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_func_DD_GetScanLine(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_GETSCANLINEDATA data = { 0 };
	UINT32 scanLine = 0;
	bool inVBlank = false;

	if (!lpDataAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!npdisp_ddraw_getRasterState(&scanLine, &inVBlank)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	if (inVBlank) {
		data.ddRVal = NPDISP_DDERR_VERTICALBLANKINPROGRESS;
	}
	else {
		data.dwScanLine = scanLine;
		data.ddRVal = 0;
	}
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_CanCreateSurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_CANCREATESURFACEDATA data = { 0 };
	NPDISP_DDSURFACEDESC desc = { 0 };
	UINT32 caps;

	if (!lpDataAddr || !npdisp.isWin9x || npdisp.version < 6 || !npdisp.mm_vramLinearAddr) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) || !data.lpDDSurfaceDesc) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_readGuest(&desc, data.lpDDSurfaceDesc, sizeof(desc), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	caps = desc.ddsCaps.dwCaps;
	if (data.bIsDifferentPixelFormat || !npdisp_ddraw_bytesPerPixel() ||
		(caps & NPDISP_DDSCAPS_SYSTEMMEMORY)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	if (caps & NPDISP_DDSCAPS_PRIMARYSURFACE) {
		if (caps & NPDISP_DDSCAPS_FLIP) {
			UINT32 pitch = 0;
			UINT32 surfaceBytes = 0;
			const UINT32 backCount = desc.dwBackBufferCount;
			const UINT32 width = desc.dwWidth ? desc.dwWidth : npdisp.width;
			const UINT32 height = desc.dwHeight ? desc.dwHeight : npdisp.height;
			if (npdisp.version < 13 || !npdisp.mm_ddOffscreenPtr || !npdisp.mm_ddVidMemAddr ||
				!backCount || width != npdisp.width || height != npdisp.height ||
				!npdisp_ddraw_calcSurfaceAllocation(width, height, &pitch, &surfaceBytes) ||
				(UINT64)surfaceBytes * backCount > NPDISP_DD_OFFSCREEN_SIZE) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			TRACEOUT11(("NPDISP11 DD_FLIP_CANCREATE size=%ux%u pitch=%u back=%u bytes=%u",
				width, height, pitch, backCount, surfaceBytes));
		}
		data.ddRVal = 0;
		npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	if (caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER)) {
		UINT32 pitch = 0;
		UINT32 surfaceBytes = 0;
		const UINT32 width = desc.dwWidth ? desc.dwWidth : npdisp.width;
		const UINT32 height = desc.dwHeight ? desc.dwHeight : npdisp.height;
		if (npdisp.version < 13 || !npdisp.mm_ddOffscreenPtr || !npdisp.mm_ddVidMemAddr ||
			width != npdisp.width || height != npdisp.height ||
			!npdisp_ddraw_calcSurfaceAllocation(width, height, &pitch, &surfaceBytes)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		data.ddRVal = 0;
		npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	if ((caps & NPDISP_DDSCAPS_OFFSCREENPLAIN) && npdisp.mm_ddOffscreenPtr && npdisp.mm_ddVidMemAddr) {
		data.ddRVal = 0;
		npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	return NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_func_DD_CreateSurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_CREATESURFACEDATA data = { 0 };
	NPDISP_DDSURFACEDESC desc = { 0 };
	UINT32 descPitch = 0;
	bool haveDesc = false;

	if (!lpDataAddr || !npdisp.isWin9x || npdisp.version < 6 || !npdisp.mm_screenPtr || !npdisp.mm_vramLinearAddr) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) || !data.dwSCnt || data.dwSCnt > 16 || !data.lplpSList) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.lpDDSurfaceDesc && npdisp_ddraw_readGuest(&desc, data.lpDDSurfaceDesc, sizeof(desc), flatAddress)) {
		haveDesc = true;
	}

	for (UINT32 i = 0; i < data.dwSCnt; ++i) {
		NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
		UINT32 lpSurfaceAddr = npdisp_ddraw_readGuest32(data.lplpSList + i * sizeof(UINT32), flatAddress);
		UINT32 caps;

		if (!lpSurfaceAddr ||
			!npdisp_ddraw_readGuest(&lcl, lpSurfaceAddr, sizeof(lcl), flatAddress) ||
			!lcl.lpGbl ||
			!npdisp_ddraw_readGuest(&gbl, lcl.lpGbl, sizeof(gbl), flatAddress)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		caps = lcl.ddsCaps.dwCaps;
		if (caps & NPDISP_DDSCAPS_SYSTEMMEMORY) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

		if (caps & NPDISP_DDSCAPS_PRIMARYSURFACE) {
			// fpVidMem==0はfpPrimaryに対応する固定GDI/primary領域を表す。
			gbl.fpVidMem = 0;
			gbl.lPitch = (SINT32)npdispwin.stride;
			gbl.dwBlockSizeX = npdisp.mm_screenSize;
			gbl.dwBlockSizeY = 1;
			gbl.wHeight = (UINT16)npdisp.height;
			gbl.wWidth = (UINT16)npdisp.width;
			descPitch = npdispwin.stride;
		}
		else if (caps & (NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) {
			UINT32 rowBytes = 0;
			UINT32 surfaceBytes = 0;
			UINT32 width = gbl.wWidth;
			UINT32 height = gbl.wHeight;

			if (!npdisp.mm_ddOffscreenPtr || !npdisp.mm_ddVidMemAddr || npdisp.version < 12) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			if (!width) width = (haveDesc && desc.dwWidth) ? desc.dwWidth : ((caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ? npdisp.width : 0);
			if (!height) height = (haveDesc && desc.dwHeight) ? desc.dwHeight : ((caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ? npdisp.height : 0);
			if ((caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) &&
				(npdisp.version < 13 || width != npdisp.width || height != npdisp.height)) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			if (!npdisp_ddraw_calcSurfaceAllocation(width, height, &rowBytes, &surfaceBytes)) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}

			gbl.fpVidMem = NPDISP_DDHAL_PLEASEALLOC_BLOCKSIZE;
			gbl.lPitch = (SINT32)rowBytes;
			gbl.dwBlockSizeX = surfaceBytes;
			gbl.dwBlockSizeY = 1;
			gbl.wWidth = (UINT16)width;
			gbl.wHeight = (UINT16)height;
			descPitch = rowBytes;
			TRACEOUT11(("NPDISP11 DD_OFFSCREEN_ALLOC_REQ idx=%u/%u caps=%08x size=%ux%u pitch=%u bytes=%u",
				i + 1, data.dwSCnt, caps, width, height, rowBytes, surfaceBytes));
		}
		else {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (!npdisp_ddraw_writeGuest(&gbl, lcl.lpGbl, sizeof(gbl), flatAddress)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}

	if (haveDesc && descPitch) {
		desc.lPitch = (SINT32)descPitch;
		desc.dwFlags |= NPDISP_DDSD_PITCH;
		npdisp_ddraw_writeGuest(&desc, data.lpDDSurfaceDesc, sizeof(desc), flatAddress);
	}

	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_DestroySurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_DESTROYSURFACEDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	if (view.apertureOffset != 0) {
		TRACEOUT11(("NPDISP11 DD_OFFSCREEN_DESTROY fp=%08x offset=%08x size=%ux%u pitch=%d visible=%u",
			view.gbl.fpVidMem, view.apertureOffset, view.width, view.height, view.pitch, view.visible ? 1 : 0));
	}

	// DestroySurface後にheapが解放されるため、scanoutが解放対象を指す場合は
	// 固定GDI primaryへ戻す。
	if (!view.systemMemory && npdisp.mm_ddFlipPending &&
		(view.visible || view.apertureOffset == npdisp.mm_ddPendingFlipOffset)) {
		npdisp.mm_ddFlipPending = 0;
		npdisp.mm_ddPendingFlipOffset = 0;
		TRACEOUT11(("NPDISP11 DD_FLIP_PENDING_DESTROY_CANCEL offset=%08x visible=%u",
			view.apertureOffset, view.visible ? 1U : 0U));
	}

	if (view.apertureOffset != 0 && view.apertureOffset == npdisp.mm_ddScanoutOffset) {
		npdisp.mm_ddScanoutOffset = 0;
		npdisp_setDirtyAll();
		npdisp.updated = 1;
		TRACEOUT11(("NPDISP11 DD_FLIP_DESTROY_RESET offset=%08x", view.apertureOffset));
	}
	if (view.apertureOffset != 0 && view.apertureOffset == npdisp.mm_ddLastScanoutOffset) {
		npdisp.mm_ddLastScanoutOffset = 0;
	}
	else if (view.visible) {
		npdisp_setDirtyAll();
		npdisp.updated = 1;
	}
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_SetClipList(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_SETCLIPLISTDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };

	if (!lpDataAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// DirectDraw runtimeがBlt時にクリップ矩形列を渡すため、driver側ではsurface固有のclip stateを保持しない。
	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUT11(("NPDISP11 DD_SET_CLIPLIST surf=%08x caps=%08x", data.lpDDSurface, view.lcl.ddsCaps.dwCaps));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_AddAttachedSurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_ADDATTACHEDSURFACEDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW baseView = { 0 };
	NPDISP_DDSURFACE_VIEW attachedView = { 0 };

	if (!lpDataAddr || npdisp.version < 13 ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!data.lpDDSurface || !data.lpSurfAttached || data.lpDDSurface == data.lpSurfAttached ||
		!npdisp_ddraw_getSurface(data.lpDDSurface, &baseView, flatAddress) ||
		!npdisp_ddraw_getSurface(data.lpSurfAttached, &attachedView, flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// NPDISPのattachmentはflip chain用途だけを扱う。両surfaceは同一display modeのscanout可能な大きさである必要がある。
	if (baseView.width != attachedView.width || baseView.height != attachedView.height ||
		baseView.pitch != attachedView.pitch || baseView.width != npdisp.width ||
		baseView.height != npdisp.height || baseView.pitch != (SINT32)npdispwin.stride) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// attachment listそのものはDirectDraw runtimeが管理し、Flip callbackには現在面と対象面が直接渡される。
	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUT11(("NPDISP11 DD_ADD_ATTACHED base=%08x/%08x attached=%08x/%08x",
		data.lpDDSurface, baseView.gbl.fpVidMem, data.lpSurfAttached, attachedView.gbl.fpVidMem));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_Lock(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_LOCKDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };
	NPDISP_DDRECTL r;
	const UINT32 bytesPerPixel = npdisp_ddraw_bytesPerPixel();

	if (!lpDataAddr || !bytesPerPixel || !npdisp.mm_vramLinearAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	if (data.bHasRect) {
		r = data.rArea;
		if (!npdisp_ddraw_normalizeSurfaceRect(&r, view.width, view.height)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		data.lpSurfData = view.linearBase + r.top * view.pitch + r.left * bytesPerPixel;
		if (view.visible) npdisp_ddraw_setLockRect(&r);
	}
	else {
		data.lpSurfData = view.linearBase;
		if (view.visible) npdisp_ddraw_setLockRect(NULL);
	}
	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	if (view.apertureOffset != 0) {
		TRACEOUT11(("NPDISP11 DD_OFFSCREEN_LOCK fp=%08x data=%08x size=%ux%u pitch=%d",
			view.gbl.fpVidMem, data.lpSurfData, view.width, view.height, view.pitch));
	}
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_Unlock(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_UNLOCKDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	if (view.visible) {
		npdisp_setDirty(npdispwin.ddrawDirtyRect.left, npdispwin.ddrawDirtyRect.top, npdispwin.ddrawDirtyRect.right, npdispwin.ddrawDirtyRect.bottom);
		npdispwin.ddrawDirtyRect.left = 0;
		npdispwin.ddrawDirtyRect.top = 0;
		npdispwin.ddrawDirtyRect.right = 0;
		npdispwin.ddrawDirtyRect.bottom = 0;
		npdisp.updated = 1;
	}
	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static bool npdisp_ddraw_getColorKey(UINT32 lpSurfaceAddr, bool flatAddress, bool source,
	NPDISP_DDCOLORKEY* colorKey)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS lcl = { 0 };

	if (!lpSurfaceAddr || !colorKey ||
		!npdisp_ddraw_readGuest(&lcl, lpSurfaceAddr, sizeof(lcl), flatAddress)) {
		return false;
	}
	*colorKey = source ? lcl.ddckCKSrcBlt : lcl.ddckCKDestBlt;
	return true;
}

static UINT32 npdisp_func_DD_SetColorKey(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_SETCOLORKEYDATA data = { 0 };
	NPDISP_DDCOLORKEY colorKey;
	UINT32 keyOffset;
	const UINT32 keyFlags = NPDISP_DDCKEY_SRCBLT | NPDISP_DDCKEY_DESTBLT;
	const UINT32 allowedFlags = keyFlags | NPDISP_DDCKEY_COLORSPACE;

	if (!lpDataAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!data.lpDDSurface ||
		(data.dwFlags & ~allowedFlags) ||
		((data.dwFlags & keyFlags) != NPDISP_DDCKEY_SRCBLT &&
		 (data.dwFlags & keyFlags) != NPDISP_DDCKEY_DESTBLT)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	colorKey = data.ckNew;
	if (!(data.dwFlags & NPDISP_DDCKEY_COLORSPACE)) {
		colorKey.dwColorSpaceHighValue = colorKey.dwColorSpaceLowValue;
	}
	if (colorKey.dwColorSpaceLowValue > colorKey.dwColorSpaceHighValue) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// Blt用color keyはsurfaceのLCLへ保存し、後続のBltで参照する。
	keyOffset = (data.dwFlags & NPDISP_DDCKEY_SRCBLT) ?
		(UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS, ddckCKSrcBlt) :
		(UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS, ddckCKDestBlt);
	if (!npdisp_ddraw_writeGuest(&colorKey, data.lpDDSurface + keyOffset,
		sizeof(colorKey), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUT11(("NPDISP11 DD_SET_COLORKEY surf=%08x flags=%08x key=%08x-%08x",
		data.lpDDSurface, data.dwFlags, colorKey.dwColorSpaceLowValue, colorKey.dwColorSpaceHighValue));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static bool npdisp_ddraw_rectsOverlap(const NPDISP_DDRECTL* a, const NPDISP_DDRECTL* b)
{
	return a->left < b->right && b->left < a->right &&
		a->top < b->bottom && b->top < a->bottom;
}

static bool npdisp_ddraw_intersectRect(NPDISP_DDRECTL* out, const NPDISP_DDRECTL* a, const NPDISP_DDRECTL* b)
{
	if (!out || !a || !b) return false;
	out->left = (a->left > b->left) ? a->left : b->left;
	out->top = (a->top > b->top) ? a->top : b->top;
	out->right = (a->right < b->right) ? a->right : b->right;
	out->bottom = (a->bottom < b->bottom) ? a->bottom : b->bottom;
	return out->left < out->right && out->top < out->bottom;
}

static void npdisp_ddraw_copyRect(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst,
	const NPDISP_DDSURFACE_VIEW* srcView, const NPDISP_DDRECTL* src, UINT32 bytesPerPixel)
{
	const SINT32 width = src->right - src->left;
	const SINT32 height = src->bottom - src->top;
	const size_t rowBytes = (size_t)width * bytesPerPixel;
	const bool sameSurface = dstView->hostBase == srcView->hostBase;
	const bool overlap = sameSurface && npdisp_ddraw_rectsOverlap(dst, src);

	if (rowBytes == (size_t)dstView->pitch && rowBytes == (size_t)srcView->pitch &&
		dst->left == dstView->hostOriginX && src->left == srcView->hostOriginX) {
		UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top, bytesPerPixel);
		UINT8* q = npdisp_ddraw_surfacePtr(srcView, src->left, src->top, bytesPerPixel);
		if (overlap) memmove(d, q, rowBytes * height);
		else memcpy(d, q, rowBytes * height);
		return;
	}

	if (!overlap) {
		for (SINT32 y = 0; y < height; ++y) {
			UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
			UINT8* q = npdisp_ddraw_surfacePtr(srcView, src->left, src->top + y, bytesPerPixel);
			memcpy(d, q, rowBytes);
		}
		return;
	}

	if (dst->top > src->top) {
		for (SINT32 y = height - 1; y >= 0; --y) {
			UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
			UINT8* q = npdisp_ddraw_surfacePtr(srcView, src->left, src->top + y, bytesPerPixel);
			memmove(d, q, rowBytes);
		}
	}
	else {
		for (SINT32 y = 0; y < height; ++y) {
			UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
			UINT8* q = npdisp_ddraw_surfacePtr(srcView, src->left, src->top + y, bytesPerPixel);
			memmove(d, q, rowBytes);
		}
	}
}

static UINT32 npdisp_ddraw_readPixelValue(const UINT8* src, UINT32 bytesPerPixel)
{
	switch (bytesPerPixel) {
	case 1:
		return src[0];
	case 2:
		return (UINT32)src[0] | ((UINT32)src[1] << 8);
	case 3:
		return (UINT32)src[0] | ((UINT32)src[1] << 8) | ((UINT32)src[2] << 16);
	case 4:
		return (UINT32)src[0] | ((UINT32)src[1] << 8) | ((UINT32)src[2] << 16) | ((UINT32)src[3] << 24);
	default:
		return 0;
	}
}

static void npdisp_ddraw_writePixelValue(UINT8* dst, UINT32 bytesPerPixel, UINT32 value)
{
	for (UINT32 i = 0; i < bytesPerPixel; ++i) {
		dst[i] = (UINT8)(value >> (i * 8));
	}
}

static UINT32 npdisp_ddraw_pixelMask(UINT32 bytesPerPixel)
{
	if (!bytesPerPixel || bytesPerPixel > 4) return 0;
	if (bytesPerPixel == 4) return 0xffffffffUL;
	return (1UL << (bytesPerPixel * 8)) - 1UL;
}

static bool npdisp_ddraw_colorKeyMatches(UINT32 pixel, const NPDISP_DDCOLORKEY* colorKey)
{
	return colorKey &&
		pixel >= colorKey->dwColorSpaceLowValue &&
		pixel <= colorKey->dwColorSpaceHighValue;
}

static UINT8 npdisp_ddraw_getRop3(UINT32 rop)
{
	return (UINT8)((rop >> 16) & 0xff);
}

static bool npdisp_ddraw_isPatternIndependentRop(UINT8 rop3)
{
	return (rop3 & 0x0f) == (rop3 >> 4);
}

static bool npdisp_ddraw_ropUsesSource(UINT8 rop3)
{
	const UINT8 truth = rop3 & 0x0f;
	return (truth & 0x03) != ((truth >> 2) & 0x03);
}

static bool npdisp_ddraw_ropUsesDest(UINT8 rop3)
{
	const UINT8 truth = rop3 & 0x0f;
	return (truth & 0x05) != ((truth >> 1) & 0x05);
}

static UINT32 npdisp_ddraw_applyRop(UINT8 rop3, UINT32 source, UINT32 dest, UINT32 pixelMask)
{
	const UINT8 truth = rop3 & 0x0f;
	const UINT32 ns = (~source) & pixelMask;
	const UINT32 nd = (~dest) & pixelMask;
	UINT32 result = 0;

	if (truth & 0x01) result |= ns & nd;
	if (truth & 0x02) result |= ns & dest;
	if (truth & 0x04) result |= source & nd;
	if (truth & 0x08) result |= source & dest;
	return result & pixelMask;
}

static bool npdisp_ddraw_bltPixels(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst, const NPDISP_DDRECTL* mapDst, const NPDISP_DDSURFACE_VIEW* srcView, const NPDISP_DDRECTL* src, UINT32 bytesPerPixel, UINT8 rop3, const NPDISP_DDCOLORKEY* srcColorKey, const NPDISP_DDCOLORKEY* dstColorKey)
{
	const SINT32 dstWidth = dst->right - dst->left;
	const SINT32 dstHeight = dst->bottom - dst->top;
	const SINT32 mapDstWidth = mapDst ? mapDst->right - mapDst->left : dstWidth;
	const SINT32 mapDstHeight = mapDst ? mapDst->bottom - mapDst->top : dstHeight;
	const bool sourceRequired = npdisp_ddraw_ropUsesSource(rop3) || srcColorKey != NULL;
	const bool destRequired = npdisp_ddraw_ropUsesDest(rop3) || dstColorKey != NULL;
	SINT32 srcWidth = dstWidth;
	SINT32 srcHeight = dstHeight;
	bool overlap = false;
	const UINT32 pixelMask = npdisp_ddraw_pixelMask(bytesPerPixel);
	UINT8* snapshot = NULL;
	size_t snapshotPitch = 0;

	if (dstWidth <= 0 || dstHeight <= 0 || mapDstWidth <= 0 || mapDstHeight <= 0 || !pixelMask || !npdisp_ddraw_isPatternIndependentRop(rop3)) return false;
	if (sourceRequired) {
		if (!srcView || !src) return false;
		srcWidth = src->right - src->left;
		srcHeight = src->bottom - src->top;
		if (srcWidth <= 0 || srcHeight <= 0) return false;
		overlap = dstView->hostBase == srcView->hostBase && npdisp_ddraw_rectsOverlap(dst, src);
	}
	if ((srcColorKey && srcColorKey->dwColorSpaceLowValue > srcColorKey->dwColorSpaceHighValue) ||
		(dstColorKey && dstColorKey->dwColorSpaceLowValue > dstColorKey->dwColorSpaceHighValue)) return false;

	// 同一surface内でsourceとdestinationが重なる場合は、source矩形を退避して読み出し元を固定する。
	if (overlap) {
		const UINT64 snapshotBytes = (UINT64)srcWidth * bytesPerPixel * srcHeight;
		if (!snapshotBytes || snapshotBytes > 0x7fffffffUL) return false;
		snapshotPitch = (size_t)srcWidth * bytesPerPixel;
		snapshot = (UINT8*)malloc((size_t)snapshotBytes);
		if (!snapshot) return false;
		for (SINT32 y = 0; y < srcHeight; ++y) {
			const UINT8* q = npdisp_ddraw_surfacePtr(srcView, src->left, src->top + y, bytesPerPixel);
			memcpy(snapshot + (size_t)y * snapshotPitch, q, snapshotPitch);
		}
	}

	for (SINT32 y = 0; y < dstHeight; ++y) {
		UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
		const SINT32 mapY = (dst->top + y) - (mapDst ? mapDst->top : dst->top);
		const SINT32 sy = sourceRequired ? (SINT32)(((UINT64)mapY * srcHeight) / mapDstHeight) : 0;
		for (SINT32 x = 0; x < dstWidth; ++x) {
			UINT32 sourcePixel = 0;
			UINT32 destPixel = 0;

			if (sourceRequired) {
				const SINT32 mapX = (dst->left + x) - (mapDst ? mapDst->left : dst->left);
				const SINT32 sx = (SINT32)(((UINT64)mapX * srcWidth) / mapDstWidth);
				const UINT8* q;
				if (snapshot) q = snapshot + (size_t)sy * snapshotPitch + (size_t)sx * bytesPerPixel;
				else q = npdisp_ddraw_surfacePtr(srcView, src->left + sx, src->top + sy, bytesPerPixel);
				sourcePixel = npdisp_ddraw_readPixelValue(q, bytesPerPixel);
				if (npdisp_ddraw_colorKeyMatches(sourcePixel, srcColorKey)) {
					d += bytesPerPixel;
					continue;
				}
			}

			if (destRequired) destPixel = npdisp_ddraw_readPixelValue(d, bytesPerPixel);
			if (dstColorKey && !npdisp_ddraw_colorKeyMatches(destPixel, dstColorKey)) {
				d += bytesPerPixel;
				continue;
			}

			npdisp_ddraw_writePixelValue(d, bytesPerPixel,
				npdisp_ddraw_applyRop(rop3, sourcePixel, destPixel, pixelMask));
			d += bytesPerPixel;
		}
	}

	if (snapshot) free(snapshot);
	return true;
}

static void npdisp_ddraw_fillRect(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst,
	UINT32 bytesPerPixel, UINT32 color)
{
	const SINT32 width = dst->right - dst->left;
	const SINT32 height = dst->bottom - dst->top;

	for (SINT32 y = 0; y < height; ++y) {
		UINT8* row = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
		switch (bytesPerPixel) {
		case 1:
			memset(row, (UINT8)color, width);
			break;
		case 2:
			{
				UINT16* q = (UINT16*)row;
				const UINT16 pixel = (UINT16)color;
				for (SINT32 x = 0; x < width; ++x) q[x] = pixel;
			}
			break;
		case 3:
			{
				const UINT8 b0 = (UINT8)color;
				const UINT8 b1 = (UINT8)(color >> 8);
				const UINT8 b2 = (UINT8)(color >> 16);
				for (SINT32 x = 0; x < width; ++x) {
					row[x * 3 + 0] = b0;
					row[x * 3 + 1] = b1;
					row[x * 3 + 2] = b2;
				}
			}
			break;
		case 4:
			{
				UINT32* q = (UINT32*)row;
				for (SINT32 x = 0; x < width; ++x) q[x] = color;
			}
			break;
		}
	}
}

static UINT32 npdisp_func_DD_Blt(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_BLTDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW dstView = { 0 };
	NPDISP_DDSURFACE_VIEW srcView = { 0 };
	NPDISP_DDSURFACE_VIEW srcExecView = { 0 };
	NPDISP_DDRECTL src = { 0 };
	NPDISP_DDRECTL dst = { 0 };
	NPDISP_DDRECTL mapSrc = { 0 };
	NPDISP_DDRECTL mapDst = { 0 };
	NPDISP_DDRECTL surfaceBounds = { 0 };
	NPDISP_DDRECTL* clipRects = NULL;
	NPDISP_DDRECTL* execRects = NULL;
	NPDISP_DDRECTL* srcNeedRects = NULL;
	NPDISP_DDSURFACE_VIEW* srcSystemViews = NULL;
	UINT8** srcSystemBuffers = NULL;
	NPDISP_DDCOLORKEY srcColorKey = { 0 };
	NPDISP_DDCOLORKEY dstColorKey = { 0 };
	const NPDISP_DDCOLORKEY* srcColorKeyPtr = NULL;
	const NPDISP_DDCOLORKEY* dstColorKeyPtr = NULL;
	const UINT32 bytesPerPixel = npdisp_ddraw_bytesPerPixel();
	UINT8* sourceSnapshot = NULL;
	UINT8 rop3 = 0xcc;
	UINT32 rectCount = 1;
	UINT32 execRectCount = 0;
	bool sourceRequired;
	bool destRequired = false;
	bool haveSource = false;
	bool clipped = false;
	bool stretch = false;
	bool drewAny = false;

	if (!lpDataAddr || !bytesPerPixel ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurfaceEx(data.lpDDDestSurface, &dstView, flatAddress, true)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	const UINT32 commonFlags = NPDISP_DDBLT_ASYNC | NPDISP_DDBLT_WAIT;
	if (data.dwFlags & NPDISP_DDBLT_COLORFILL) {
		if (data.dwFlags & ~(commonFlags | NPDISP_DDBLT_COLORFILL)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		sourceRequired = false;
	}
	else {
		const UINT32 supportedFlags = commonFlags | NPDISP_DDBLT_ROP |
			NPDISP_DDBLT_KEYDEST | NPDISP_DDBLT_KEYDESTOVERRIDE |
			NPDISP_DDBLT_KEYSRC | NPDISP_DDBLT_KEYSRCOVERRIDE;
		if (data.dwFlags & ~supportedFlags) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if ((data.dwFlags & NPDISP_DDBLT_KEYSRC) && (data.dwFlags & NPDISP_DDBLT_KEYSRCOVERRIDE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if ((data.dwFlags & NPDISP_DDBLT_KEYDEST) && (data.dwFlags & NPDISP_DDBLT_KEYDESTOVERRIDE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

		if (data.dwFlags & NPDISP_DDBLT_ROP) {
			rop3 = npdisp_ddraw_getRop3(data.bltFX.dwROP);
			if (!npdisp_ddraw_isPatternIndependentRop(rop3)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (data.dwFlags & NPDISP_DDBLT_KEYDESTOVERRIDE) {
			dstColorKey = data.bltFX.ddckDestColorkey;
			dstColorKeyPtr = &dstColorKey;
		}
		else if (data.dwFlags & NPDISP_DDBLT_KEYDEST) {
			if (!npdisp_ddraw_getColorKey(data.lpDDDestSurface, flatAddress, false, &dstColorKey)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			dstColorKeyPtr = &dstColorKey;
		}

		sourceRequired = npdisp_ddraw_ropUsesSource(rop3) ||
			(data.dwFlags & (NPDISP_DDBLT_KEYSRC | NPDISP_DDBLT_KEYSRCOVERRIDE)) != 0;
		if (data.lpDDSrcSurface) {
			if (!npdisp_ddraw_getSurfaceEx(data.lpDDSrcSurface, &srcView, flatAddress, true)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			haveSource = true;
		}
		else if (sourceRequired) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (data.dwFlags & NPDISP_DDBLT_KEYSRCOVERRIDE) {
			srcColorKey = data.bltFX.ddckSrcColorkey;
			srcColorKeyPtr = &srcColorKey;
		}
		else if (data.dwFlags & NPDISP_DDBLT_KEYSRC) {
			if (!npdisp_ddraw_getColorKey(data.lpDDSrcSurface, flatAddress, true, &srcColorKey)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			srcColorKeyPtr = &srcColorKey;
		}
	}

	clipped = data.IsClipped != 0;
	if (clipped) {
		if (data.dwRectCnt > NPDISP_DD_MAX_CLIP_RECTS || (data.dwRectCnt && !data.prDestRects)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		mapDst = data.rOrigDest;
		if (mapDst.left >= mapDst.right || mapDst.top >= mapDst.bottom) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if (sourceRequired) {
			mapSrc = data.rOrigSrc;
			if (mapSrc.left < 0 || mapSrc.top < 0 || mapSrc.right > (SINT32)srcView.width || mapSrc.bottom > (SINT32)srcView.height ||
				mapSrc.left >= mapSrc.right || mapSrc.top >= mapSrc.bottom) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		rectCount = data.dwRectCnt;
		if (rectCount) {
			clipRects = (NPDISP_DDRECTL*)malloc((size_t)rectCount * sizeof(*clipRects));
			if (!clipRects || !npdisp_ddraw_readGuest(clipRects, data.prDestRects, (int)(rectCount * sizeof(*clipRects)), flatAddress)) {
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
		}
	}
	else {
		mapDst = data.rDest;
		if (!npdisp_ddraw_normalizeSurfaceRect(&mapDst, dstView.width, dstView.height)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if (sourceRequired) {
			mapSrc = data.rSrc;
			if (!npdisp_ddraw_normalizeSurfaceRect(&mapSrc, srcView.width, srcView.height)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}

	const SINT32 mapDstWidth = mapDst.right - mapDst.left;
	const SINT32 mapDstHeight = mapDst.bottom - mapDst.top;
	const SINT32 mapSrcWidth = sourceRequired ? mapSrc.right - mapSrc.left : mapDstWidth;
	const SINT32 mapSrcHeight = sourceRequired ? mapSrc.bottom - mapSrc.top : mapDstHeight;
	stretch = sourceRequired && (mapSrcWidth != mapDstWidth || mapSrcHeight != mapDstHeight);
	destRequired = npdisp_ddraw_ropUsesDest(rop3) || srcColorKeyPtr != NULL || dstColorKeyPtr != NULL;

	// 実際に描画されるdestination矩形を先に確定する。
	surfaceBounds.left = 0;
	surfaceBounds.top = 0;
	surfaceBounds.right = (SINT32)dstView.width;
	surfaceBounds.bottom = (SINT32)dstView.height;
	if (rectCount) {
		execRects = (NPDISP_DDRECTL*)malloc((size_t)rectCount * sizeof(*execRects));
		if (!execRects) {
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		for (UINT32 i = 0; i < rectCount; ++i) {
			NPDISP_DDRECTL requested = clipped ? clipRects[i] : mapDst;
			NPDISP_DDRECTL clippedToMap;
			NPDISP_DDRECTL actual;
			if (!npdisp_ddraw_intersectRect(&clippedToMap, &requested, &mapDst) ||
				!npdisp_ddraw_intersectRect(&actual, &clippedToMap, &surfaceBounds)) continue;
			execRects[execRectCount++] = actual;
		}
	}

	// System-memory sourceは、各実描画矩形から参照されるsource領域だけを描画開始前にmirrorする。
	// sourceとdestinationが同一surfaceでも、全source mirrorを先に済ませるため後続矩形のsourceは破壊されない。
	if (sourceRequired && srcView.systemMemory && execRectCount) {
		srcNeedRects = (NPDISP_DDRECTL*)malloc((size_t)execRectCount * sizeof(*srcNeedRects));
		srcSystemViews = (NPDISP_DDSURFACE_VIEW*)malloc((size_t)execRectCount * sizeof(*srcSystemViews));
		srcSystemBuffers = (UINT8**)calloc((size_t)execRectCount, sizeof(*srcSystemBuffers));
		if (!srcNeedRects || !srcSystemViews || !srcSystemBuffers) {
			if (srcSystemBuffers) free(srcSystemBuffers);
			if (srcSystemViews) free(srcSystemViews);
			if (srcNeedRects) free(srcNeedRects);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		for (UINT32 i = 0; i < execRectCount; ++i) {
			srcSystemViews[i] = srcView;
			if (!npdisp_ddraw_mapDestRectToSource(&srcNeedRects[i], &execRects[i], &mapDst, &mapSrc) ||
				!npdisp_ddraw_mirrorSystemRect(&srcSystemViews[i], &srcNeedRects[i], bytesPerPixel, true, &srcSystemBuffers[i])) {
				for (UINT32 j = 0; j <= i; ++j) if (srcSystemBuffers[j]) free(srcSystemBuffers[j]);
				free(srcSystemBuffers);
				free(srcSystemViews);
				free(srcNeedRects);
				free(execRects);
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
		}
	}

	// Video-memory内のclipped self-Bltは、複数clip矩形を順次処理してもsourceが変化しないよう元sourceを固定する。
	srcExecView = srcView;
	if (clipped && sourceRequired && !srcView.systemMemory && !dstView.systemMemory && dstView.hostBase == srcView.hostBase) {
		bool needsSnapshot = false;
		for (UINT32 i = 0; i < execRectCount; ++i) {
			if (npdisp_ddraw_rectsOverlap(&execRects[i], &mapSrc)) {
				needsSnapshot = true;
				break;
			}
		}
		if (needsSnapshot) {
			const size_t snapshotPitch = (size_t)mapSrcWidth * bytesPerPixel;
			const UINT64 snapshotBytes = (UINT64)snapshotPitch * mapSrcHeight;
			if (!snapshotBytes || snapshotBytes > 0x7fffffffUL) {
				if (srcSystemBuffers) {
					for (UINT32 i = 0; i < execRectCount; ++i) if (srcSystemBuffers[i]) free(srcSystemBuffers[i]);
					free(srcSystemBuffers);
				}
				if (srcSystemViews) free(srcSystemViews);
				if (srcNeedRects) free(srcNeedRects);
				if (execRects) free(execRects);
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			sourceSnapshot = (UINT8*)malloc((size_t)snapshotBytes);
			if (!sourceSnapshot) {
				if (srcSystemBuffers) {
					for (UINT32 i = 0; i < execRectCount; ++i) if (srcSystemBuffers[i]) free(srcSystemBuffers[i]);
					free(srcSystemBuffers);
				}
				if (srcSystemViews) free(srcSystemViews);
				if (srcNeedRects) free(srcNeedRects);
				if (execRects) free(execRects);
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			for (SINT32 y = 0; y < mapSrcHeight; ++y) {
				const UINT8* q = npdisp_ddraw_surfacePtr(&srcView, mapSrc.left, mapSrc.top + y, bytesPerPixel);
				memcpy(sourceSnapshot + (size_t)y * snapshotPitch, q, snapshotPitch);
			}
			srcExecView.hostBase = sourceSnapshot;
			srcExecView.pitch = (SINT32)snapshotPitch;
			srcExecView.width = (UINT32)mapSrcWidth;
			srcExecView.height = (UINT32)mapSrcHeight;
			srcExecView.hostOriginX = 0;
			srcExecView.hostOriginY = 0;
			srcExecView.apertureOffset = 0xffffffffUL;
			mapSrc.left = 0;
			mapSrc.top = 0;
			mapSrc.right = mapSrcWidth;
			mapSrc.bottom = mapSrcHeight;
		}
	}

	for (UINT32 i = 0; i < execRectCount; ++i) {
		NPDISP_DDSURFACE_VIEW dstExecView = dstView;
		const NPDISP_DDSURFACE_VIEW* srcRectView = (sourceRequired && srcView.systemMemory) ? &srcSystemViews[i] : &srcExecView;
		UINT8* dstSystemBuffer = NULL;

		dst = execRects[i];
		// System-memory destinationもこの描画矩形だけをmirrorする。完全上書きなら既存画素は読まない。
		if (dstView.systemMemory && !npdisp_ddraw_mirrorSystemRect(&dstExecView, &dst, bytesPerPixel, destRequired, &dstSystemBuffer)) {
			if (sourceSnapshot) free(sourceSnapshot);
			if (srcSystemBuffers) {
				for (UINT32 j = 0; j < execRectCount; ++j) if (srcSystemBuffers[j]) free(srcSystemBuffers[j]);
				free(srcSystemBuffers);
			}
			if (srcSystemViews) free(srcSystemViews);
			if (srcNeedRects) free(srcNeedRects);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (data.dwFlags & NPDISP_DDBLT_COLORFILL) {
			npdisp_ddraw_fillRect(&dstExecView, &dst, bytesPerPixel, data.bltFX.dwFillColor);
		}
		else if (!stretch && rop3 == 0xcc && !srcColorKeyPtr && !dstColorKeyPtr && haveSource) {
			src.left = mapSrc.left + (dst.left - mapDst.left);
			src.top = mapSrc.top + (dst.top - mapDst.top);
			src.right = src.left + (dst.right - dst.left);
			src.bottom = src.top + (dst.bottom - dst.top);
			if (src.left < 0 || src.top < 0 || src.right > (SINT32)srcRectView->width || src.bottom > (SINT32)srcRectView->height) {
				if (dstSystemBuffer) free(dstSystemBuffer);
				if (sourceSnapshot) free(sourceSnapshot);
				if (srcSystemBuffers) {
					for (UINT32 j = 0; j < execRectCount; ++j) if (srcSystemBuffers[j]) free(srcSystemBuffers[j]);
					free(srcSystemBuffers);
				}
				if (srcSystemViews) free(srcSystemViews);
				if (srcNeedRects) free(srcNeedRects);
				if (execRects) free(execRects);
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			npdisp_ddraw_copyRect(&dstExecView, &dst, srcRectView, &src, bytesPerPixel);
		}
		else if (!npdisp_ddraw_bltPixels(&dstExecView, &dst, &mapDst, sourceRequired ? srcRectView : NULL,
			sourceRequired ? &mapSrc : NULL, bytesPerPixel, rop3, srcColorKeyPtr, dstColorKeyPtr)) {
			if (dstSystemBuffer) free(dstSystemBuffer);
			if (sourceSnapshot) free(sourceSnapshot);
			if (srcSystemBuffers) {
				for (UINT32 j = 0; j < execRectCount; ++j) if (srcSystemBuffers[j]) free(srcSystemBuffers[j]);
				free(srcSystemBuffers);
			}
			if (srcSystemViews) free(srcSystemViews);
			if (srcNeedRects) free(srcNeedRects);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (dstView.systemMemory && !npdisp_ddraw_commitSystemRect(&dstExecView, &dst, bytesPerPixel)) {
			if (dstSystemBuffer) free(dstSystemBuffer);
			if (sourceSnapshot) free(sourceSnapshot);
			if (srcSystemBuffers) {
				for (UINT32 j = 0; j < execRectCount; ++j) if (srcSystemBuffers[j]) free(srcSystemBuffers[j]);
				free(srcSystemBuffers);
			}
			if (srcSystemViews) free(srcSystemViews);
			if (srcNeedRects) free(srcNeedRects);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (dstSystemBuffer) free(dstSystemBuffer);
		drewAny = true;
		if (dstView.visible) npdisp_setDirty(dst.left, dst.top, dst.right, dst.bottom);
	}

	if (sourceSnapshot) free(sourceSnapshot);
	if (srcSystemBuffers) {
		for (UINT32 i = 0; i < execRectCount; ++i) if (srcSystemBuffers[i]) free(srcSystemBuffers[i]);
		free(srcSystemBuffers);
	}
	if (srcSystemViews) free(srcSystemViews);
	if (srcNeedRects) free(srcNeedRects);
	if (execRects) free(execRects);
	if (clipRects) free(clipRects);
	if (dstView.visible && drewAny) npdisp.updated = 1;
	if (dstView.systemMemory || (sourceRequired && srcView.systemMemory)) {
		TRACEOUT11(("NPDISP11 DD_BLT_SYSMEM src=%u dst=%u srcptr=%08x dstptr=%08x flags=%08x clipped=%u rects=%u",
			(sourceRequired && srcView.systemMemory) ? 1U : 0U, dstView.systemMemory ? 1U : 0U,
			sourceRequired ? srcView.gbl.fpVidMem : 0U, dstView.gbl.fpVidMem, data.dwFlags, clipped ? 1U : 0U, rectCount));
	}
	if (stretch) {
		TRACEOUT11(("NPDISP11 DD_BLT_STRETCH src=%dx%d dst=%dx%d flags=%08x rop=%02x clipped=%u rects=%u",
			mapSrcWidth, mapSrcHeight, mapDstWidth, mapDstHeight, data.dwFlags, rop3, clipped ? 1U : 0U, rectCount));
	}
	else if (clipped) {
		TRACEOUT11(("NPDISP11 DD_BLT_CLIPPED dst=%dx%d flags=%08x rop=%02x rects=%u",
			mapDstWidth, mapDstHeight, data.dwFlags, rop3, rectCount));
	}
	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_Flip(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_FLIPDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW currentView = { 0 };
	NPDISP_DDSURFACE_VIEW targetView = { 0 };

	if (!lpDataAddr || npdisp.version < 13 ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!npdisp_ddraw_getSurface(data.lpSurfCurr, &currentView, flatAddress) ||
		!npdisp_ddraw_getSurface(data.lpSurfTarg, &targetView, flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (targetView.width != npdisp.width || targetView.height != npdisp.height ||
		targetView.pitch != (SINT32)npdispwin.stride) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// fpVidMem交換はDirectDraw runtimeが行い、host側は物理scanoutの切替時刻だけを管理する。
	if (npdisp.mm_ddFlipPending) {
		data.ddRVal = NPDISP_DDERR_WASSTILLDRAWING;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		TRACEOUT11(("NPDISP11 DD_FLIP_BUSY pending=%08x flags=%08x",
			npdisp.mm_ddPendingFlipOffset, data.dwFlags));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	// DirectDraw runtime performs the fpVidMem exchange. The host display
	// switches to the target allocation only during vertical blank.
	npdisp.mm_ddPendingFlipOffset = targetView.apertureOffset;
	npdisp.mm_ddFlipPending = 1;
	if (gdc.vsync & 0x20) {
		// A request issued after VBlank has already begun can safely take effect
		// in the current blanking interval instead of waiting one extra frame.
		npdisp_ddraw_commitPendingFlip();
	}

	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUT11(("NPDISP11 DD_FLIP_QUEUE from=%08x/%08x to=%08x/%08x flags=%08x pending=%u",
		currentView.gbl.fpVidMem, currentView.apertureOffset,
		targetView.gbl.fpVidMem, targetView.apertureOffset, data.dwFlags, npdisp.mm_ddFlipPending));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_GetBltStatus(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_GETBLTSTATUSDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };

	if (!lpDataAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.dwFlags != NPDISP_DDGBS_CANBLT && data.dwFlags != NPDISP_DDGBS_ISBLTDONE) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// Bltはすべて同期実行なので、キュー待ちの処理は存在しない。
	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_GetFlipStatus(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_GETFLIPSTATUSDATA data = { 0 };

	if (!lpDataAddr || npdisp.version < 13 ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.dwFlags != NPDISP_DDGFS_CANFLIP && data.dwFlags != NPDISP_DDGFS_ISFLIPDONE) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	// VBlank待ちのFlipが残っている間はDirectDrawへbusy状態を返す。
	data.ddRVal = npdisp.mm_ddFlipPending ? NPDISP_DDERR_WASSTILLDRAWING : 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUT11(("NPDISP11 DD_FLIP_STATUS flags=%08x scanout=%08x pending=%u target=%08x rval=%08x",
		data.dwFlags, npdisp.mm_ddScanoutOffset, npdisp.mm_ddFlipPending,
		npdisp.mm_ddPendingFlipOffset, (UINT32)data.ddRVal));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_FlipToGDISurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_FLIPTOGDISURFACEDATA data = { 0 };

	if (!lpDataAddr || npdisp.version < 13 ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (npdisp.mm_ddFlipPending) {
		TRACEOUT11(("NPDISP11 DD_FLIP_GDI_CANCEL pending=%08x", npdisp.mm_ddPendingFlipOffset));
		npdisp.mm_ddFlipPending = 0;
		npdisp.mm_ddPendingFlipOffset = 0;
	}

	if (data.dwToGDI) {
		// GDI表示からDirectDraw表示へ戻せるよう、現在のfront allocationを保存する。
		if (npdisp.mm_ddScanoutOffset != 0 && npdisp_ddraw_isScanoutOffsetValid(npdisp.mm_ddScanoutOffset)) {
			npdisp.mm_ddLastScanoutOffset = npdisp.mm_ddScanoutOffset;
		}
		npdisp.mm_ddScanoutOffset = 0;
	}
	else if (npdisp_ddraw_isScanoutOffsetValid(npdisp.mm_ddLastScanoutOffset)) {
		npdisp.mm_ddScanoutOffset = npdisp.mm_ddLastScanoutOffset;
	}
	else {
		npdisp.mm_ddScanoutOffset = 0;
	}
	npdisp_setDirtyAll();
	npdisp.updated = 1;
	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	TRACEOUT11(("NPDISP11 DD_FLIP_GDI togdi=%u scanout=%08x last=%08x",
		data.dwToGDI, npdisp.mm_ddScanoutOffset, npdisp.mm_ddLastScanoutOffset));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}


// DirectDraw HAL情報を構築し、16bit側の共有領域へ設定する。
static UINT16 npdisp_dd_controlCreateDriverObject(UINT32 lpDestDevAddr, const NPDISP_DCICMD* cmd)
{
	UINT16 retValue = 0;
	if (!cmd) return 0;
	const NPDISP_DCICMD& dciCmd = *cmd;
	do {

		const UINT32 ddHalInfoAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddHalInfoAddr);
		const UINT32 ddModeInfoAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddModeInfoAddr);
		const UINT32 ddCallbacksAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddCallbacksAddr);
		const UINT32 ddSurfaceCallbacksAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddSurfaceCallbacksAddr);
		const UINT32 ddPaletteCallbacksAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddPaletteCallbacksAddr);
		TRACEOUT(("DDCREATEDRIVEROBJECT v=%08x p1=%08x setinfo_reset=%08x hal=%08x cb=%08x surfcb=%08x", dciCmd.dwVersion, dciCmd.dwParam1, 0U, ddHalInfoAddr, ddCallbacksAddr, ddSurfaceCallbacksAddr));
		if (dciCmd.dwVersion == NPDISP_DD_VERSION && npdisp.version >= 10 && npdisp.isWin9x && ddHalInfoAddr && ddModeInfoAddr && ddCallbacksAddr && ddSurfaceCallbacksAddr && ddPaletteCallbacksAddr && npdisp_ddraw_bytesPerPixel()) {
			npdisp.mm_ddPendingFlipOffset = 0;
			npdisp.mm_ddFlipPending = 0;
			if (!npdisp_dd_prepareCurrentCallbackTables(ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr)) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT callback sync failed cb=%08x surfcb=%08x palcb=%08x",
					ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr));
				retValue = 0;
				break;
			}
			NPDISP_DDHALINFO bootstrapInfo = { 0 };
			NPDISP_DDHALINFO halInfo = { 0 };
			NPDISP_DDHALMODEINFO modeInfo[NPDISP_DD_MAX_MODES] = { 0 };
			NPDISP_DDHALMODEINFO currentMode = { 0 };
			UINT32 modeCount = 0;
			UINT32 currentModeIndex = 0xffffffffUL;
			NPDISP_DDHAL_DDCALLBACKS ddCallbacks = { 0 };
			NPDISP_DDHAL_DDSURFACECALLBACKS surfaceCallbacks = { 0 };
			NPDISP_DDHAL_DDPALETTECALLBACKS paletteCallbacks = { 0 };
			// DriverInitが共有DDHALINFOへ設定した32bit HAL DLLのHINSTANCEを保持する。
			npdisp_readMemory(&bootstrapInfo, ddHalInfoAddr, sizeof(bootstrapInfo));
			npdisp_readMemory(&ddCallbacks, ddCallbacksAddr, sizeof(ddCallbacks));
			npdisp_readMemory(&surfaceCallbacks, ddSurfaceCallbacksAddr, sizeof(surfaceCallbacks));
			npdisp_readMemory(&paletteCallbacks, ddPaletteCallbacksAddr, sizeof(paletteCallbacks));
			if (bootstrapInfo.lpD3DGlobalDriverData != NPDISP_DDBRIDGE_ACK_MAGIC ||
				bootstrapInfo.lpD3DHALCallbacks != NPDISP_DDBRIDGE_ABI_VERSION ||
				(bootstrapInfo.lpDDExeBufCallbacksAddr & ~NPDISP_DDBRIDGE_FEATURE_SUPPORTED) != 0) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT bridge ack=%08x abi=%08x feat=%08x",
					bootstrapInfo.lpD3DGlobalDriverData, bootstrapInfo.lpD3DHALCallbacks, bootstrapInfo.lpDDExeBufCallbacksAddr));
				retValue = 0;
				break;
			}
			if (ddCallbacks.dwFlags != NPDISP_DDBRIDGE_DD_REQUEST_MASK ||
				surfaceCallbacks.dwFlags != NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK ||
				paletteCallbacks.dwFlags != NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT bridge masks=%08x/%08x/%08x",
					ddCallbacks.dwFlags, surfaceCallbacks.dwFlags, paletteCallbacks.dwFlags));
				retValue = 0;
				break;
			}
			TRACEOUT11(("NPDISP11 DD_CB32 ddsize=%u ddflags=%08x create=%08x can=%08x fliptogdi=%08x surfsize=%u surfflags=%08x destroy=%08x flip=%08x lock=%08x unlock=%08x blt=%08x setck=%08x flipstatus=%08x",
				ddCallbacks.dwSize, ddCallbacks.dwFlags, ddCallbacks.CreateSurfaceAddr, ddCallbacks.CanCreateSurfaceAddr, ddCallbacks.FlipToGDISurfaceAddr,
				surfaceCallbacks.dwSize, surfaceCallbacks.dwFlags, surfaceCallbacks.DestroySurfaceAddr, surfaceCallbacks.FlipAddr,
				surfaceCallbacks.LockAddr, surfaceCallbacks.UnlockAddr, surfaceCallbacks.BltAddr, surfaceCallbacks.SetColorKeyAddr, surfaceCallbacks.GetFlipStatusAddr));
			TRACEOUT11(("NPDISP11 DD_CB32_EXTRA setclip=%08x addattached=%08x",
				surfaceCallbacks.SetClipListAddr, surfaceCallbacks.AddAttachedSurfaceAddr));
			TRACEOUT11(("NPDISP11 DD_PALCB addr=%08x size=%u flags=%08x destroy=%08x setentries=%08x",
				ddPaletteCallbacksAddr, paletteCallbacks.dwSize, paletteCallbacks.dwFlags,
				paletteCallbacks.DestroyPaletteAddr, paletteCallbacks.SetEntriesAddr));
			halInfo.dwSize = sizeof(halInfo);
			// hInstanceには16bit display driverではなく32bit HAL DLLのHINSTANCEを設定する。
			halInfo.hInstance = bootstrapInfo.hInstance;
			halInfo.lpDDCallbacksAddr = ddCallbacksAddr;
			halInfo.lpDDSurfaceCallbacksAddr = ddSurfaceCallbacksAddr;
			halInfo.lpDDPaletteCallbacksAddr = ddPaletteCallbacksAddr;
			if (bootstrapInfo.lpDDExeBufCallbacksAddr & NPDISP_DDBRIDGE_FEATURE_GETDRIVERINFO) {
				halInfo.GetDriverInfoAddr = bootstrapInfo.GetDriverInfoAddr;
			}
			halInfo.vmiData.fpPrimary = npdisp.mm_vramLinearAddr;
			// VIDMEMINFO.dwFlagsは予約領域。VIDMEM_ISLINEARは各VIDMEM heapへ設定する。
			halInfo.vmiData.dwFlags = 0;
			halInfo.vmiData.dwDisplayWidth = npdisp.width;
			halInfo.vmiData.dwDisplayHeight = npdisp.height;
			halInfo.vmiData.lDisplayPitch = (SINT32)npdispwin.stride;
			halInfo.vmiData.ddpfDisplay.dwSize = sizeof(NPDISP_DDPIXELFORMAT);
			halInfo.vmiData.ddpfDisplay.dwFlags = NPDISP_DDPF_RGB;
			halInfo.vmiData.ddpfDisplay.dwRGBBitCount = (npdisp.bpp == 15) ? 16 : npdisp.bpp;
			if (npdisp.bpp == 8) {
				halInfo.vmiData.ddpfDisplay.dwFlags |= NPDISP_DDPF_PALETTEINDEXED8;
			}
			else if (npdisp.bpp == 15) {
				halInfo.vmiData.ddpfDisplay.dwRBitMask = 0x00007c00;
				halInfo.vmiData.ddpfDisplay.dwGBitMask = 0x000003e0;
				halInfo.vmiData.ddpfDisplay.dwBBitMask = 0x0000001f;
			}
			else if (npdisp.bpp == 16) {
				halInfo.vmiData.ddpfDisplay.dwRBitMask = 0x0000f800;
				halInfo.vmiData.ddpfDisplay.dwGBitMask = 0x000007e0;
				halInfo.vmiData.ddpfDisplay.dwBBitMask = 0x0000001f;
			}
			else if (npdisp.bpp == 24 || npdisp.bpp == 32) {
				halInfo.vmiData.ddpfDisplay.dwRBitMask = 0x00ff0000;
				halInfo.vmiData.ddpfDisplay.dwGBitMask = 0x0000ff00;
				halInfo.vmiData.ddpfDisplay.dwBBitMask = 0x000000ff;
			}

			// HAL mode tableはEnumDisplayModesとモード変更のindexに使われるため、
			// DirectDraw対応モードをすべて登録する。
			static const UINT32 ddBpps[] = { 8, 16, 24, 32 };
			for (UINT32 bi = 0; bi < sizeof(ddBpps) / sizeof(ddBpps[0]); ++bi) {
				for (UINT32 ri = 0; ri < sizeof(npdisp_ddStandardResolutions) / sizeof(npdisp_ddStandardResolutions[0]); ++ri) {
					if (modeCount >= NPDISP_DD_MAX_MODES) break;
					npdisp_dd_fillModeInfo(&modeInfo[modeCount],
						npdisp_ddStandardResolutions[ri].width,
						npdisp_ddStandardResolutions[ri].height,
						ddBpps[bi], false);
					modeCount++;
				}
			}

			npdisp_dd_fillModeInfo(&currentMode, npdisp.width, npdisp.height, npdisp.bpp, npdisp.bpp == 15);
			currentMode.lPitch = (SINT32)npdispwin.stride;
			currentMode.dwBPP = halInfo.vmiData.ddpfDisplay.dwRGBBitCount;
			currentMode.wFlags = (npdisp.bpp == 8) ? NPDISP_DDMODEINFO_PALETTIZED : 0;
			currentMode.dwRBitMask = halInfo.vmiData.ddpfDisplay.dwRBitMask;
			currentMode.dwGBitMask = halInfo.vmiData.ddpfDisplay.dwGBitMask;
			currentMode.dwBBitMask = halInfo.vmiData.ddpfDisplay.dwBBitMask;
			currentMode.dwAlphaBitMask = halInfo.vmiData.ddpfDisplay.dwRGBAlphaBitMask;

			for (UINT32 i = 0; i < modeCount; ++i) {
				if (npdisp_dd_modeEquals(&modeInfo[i], &currentMode)) {
					currentModeIndex = i;
					break;
				}
			}
			if (currentModeIndex == 0xffffffffUL && modeCount < NPDISP_DD_MAX_MODES) {
				currentModeIndex = modeCount;
				modeInfo[modeCount++] = currentMode;
			}
			if (currentModeIndex == 0xffffffffUL || !modeCount) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT mode table full current=%ux%u bpp=%u",
					npdisp.width, npdisp.height, npdisp.bpp));
				retValue = 0;
				break;
			}

			halInfo.dwModeIndex = currentModeIndex;
			halInfo.dwNumModes = modeCount;
			halInfo.lpModeInfo = ddModeInfoAddr;

			halInfo.ddCaps.dwSize = sizeof(NPDISP_DDCORECAPS);
			halInfo.ddCaps.dwCaps = NPDISP_DDCAPS_BLT | NPDISP_DDCAPS_BLTSTRETCH | NPDISP_DDCAPS_GDI |
				NPDISP_DDCAPS_READSCANLINE | NPDISP_DDCAPS_COLORKEY | NPDISP_DDCAPS_BLTCOLORFILL |
				NPDISP_DDCAPS_CANCLIP | NPDISP_DDCAPS_CANCLIPSTRETCHED | NPDISP_DDCAPS_CANBLTSYSMEM;
			halInfo.ddCaps.dwCaps2 = NPDISP_DDCAPS2_NOPAGELOCKREQUIRED;
			halInfo.ddCaps.dwCKeyCaps = NPDISP_DDCKEYCAPS_DESTBLT | NPDISP_DDCKEYCAPS_DESTBLTCLRSPACE |
				NPDISP_DDCKEYCAPS_SRCBLT | NPDISP_DDCKEYCAPS_SRCBLTCLRSPACE;
			halInfo.ddCaps.dwFXCaps = NPDISP_DDFXCAPS_BLTSHRINKX | NPDISP_DDFXCAPS_BLTSHRINKXN |
				NPDISP_DDFXCAPS_BLTSHRINKY | NPDISP_DDFXCAPS_BLTSHRINKYN |
				NPDISP_DDFXCAPS_BLTSTRETCHX | NPDISP_DDFXCAPS_BLTSTRETCHXN |
				NPDISP_DDFXCAPS_BLTSTRETCHY | NPDISP_DDFXCAPS_BLTSTRETCHYN;
			const UINT32 sysBltCaps = NPDISP_DDCAPS_BLT | NPDISP_DDCAPS_BLTSTRETCH | NPDISP_DDCAPS_COLORKEY |
				NPDISP_DDCAPS_CANCLIP | NPDISP_DDCAPS_CANCLIPSTRETCHED;
			halInfo.ddCaps.dwSVBCaps = sysBltCaps;
			halInfo.ddCaps.dwVSBCaps = sysBltCaps;
			halInfo.ddCaps.dwSSBCaps = sysBltCaps;
			halInfo.ddCaps.dwSVBCKeyCaps = halInfo.ddCaps.dwCKeyCaps;
			halInfo.ddCaps.dwVSBCKeyCaps = halInfo.ddCaps.dwCKeyCaps;
			halInfo.ddCaps.dwSSBCKeyCaps = halInfo.ddCaps.dwCKeyCaps;
			halInfo.ddCaps.dwSVBFXCaps = halInfo.ddCaps.dwFXCaps;
			halInfo.ddCaps.dwVSBFXCaps = halInfo.ddCaps.dwFXCaps;
			halInfo.ddCaps.dwSSBFXCaps = halInfo.ddCaps.dwFXCaps;
			halInfo.ddCaps.dwSVBCaps2 = NPDISP_DDCAPS2_NOPAGELOCKREQUIRED;
			halInfo.ddCaps.ddsCaps.dwCaps = NPDISP_DDSCAPS_PRIMARYSURFACE;
			if (!npdisp_dd_configureVideoMemory(&halInfo)) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT video memory setup failed"));
				retValue = 0;
				break;
			}
			// Patternに依存しないROP3は、source/destinationだけで処理できるため全て公開する。
			for (UINT32 rop = 0; rop < 256; ++rop) {
				if ((rop & 0x0f) == (rop >> 4)) {
					const UINT32 bit = 1UL << (rop & 31);
					halInfo.ddCaps.dwRops[rop >> 5] |= bit;
					halInfo.ddCaps.dwSVBRops[rop >> 5] |= bit;
					halInfo.ddCaps.dwVSBRops[rop >> 5] |= bit;
					halInfo.ddCaps.dwSSBRops[rop >> 5] |= bit;
				}
			}
			halInfo.dwFlags = NPDISP_DDHALINFO_ISPRIMARYDISPLAY | NPDISP_DDHALINFO_MODEXILLEGAL;
			if (halInfo.GetDriverInfoAddr) {
				halInfo.dwFlags |= NPDISP_DDHALINFO_GETDRIVERINFOSET;
			}
			// lpPDeviceにはControlへ渡されたGDI display deviceのPDEVICEをそのまま設定する。
			halInfo.lpPDevice = lpDestDevAddr;

			TRACEOUT11(("NPDISP11 DD_HALINFO cmdp1=%08x setinfo_reset=%08x size=%u hinst=%08x primary=%08x vmiflags=%08x heaps=%u align=%u/%u/%u/%u/%u caps=%08x caps2=%08x ckey=%08x vid=%u/%u ddscaps=%08x cb=%08x surfcb=%08x palcb=%08x modeidx=%u modes=%u modeptr=%08x",
				dciCmd.dwParam1, 0U, halInfo.dwSize, halInfo.hInstance, halInfo.vmiData.fpPrimary, halInfo.vmiData.dwFlags, halInfo.vmiData.dwNumHeaps,
				halInfo.vmiData.dwOffscreenAlign, halInfo.vmiData.dwOverlayAlign, halInfo.vmiData.dwTextureAlign,
				halInfo.vmiData.dwZBufferAlign, halInfo.vmiData.dwAlphaAlign,
				halInfo.ddCaps.dwCaps, halInfo.ddCaps.dwCaps2, halInfo.ddCaps.dwCKeyCaps, halInfo.ddCaps.dwVidMemTotal, halInfo.ddCaps.dwVidMemFree,
				halInfo.ddCaps.ddsCaps.dwCaps, halInfo.lpDDCallbacksAddr, halInfo.lpDDSurfaceCallbacksAddr, halInfo.lpDDPaletteCallbacksAddr,
				halInfo.dwModeIndex, halInfo.dwNumModes, halInfo.lpModeInfo));
			TRACEOUT11(("NPDISP11 DD_HALTAIL flags=%08x pdevice=%08x hinst=%08x bootstrap_hinst=%08x",
				halInfo.dwFlags, halInfo.lpPDevice, halInfo.hInstance, bootstrapInfo.hInstance));
			TRACEOUT11(("NPDISP11 DD_MODE_TABLE count=%u current=%u addr=%08x bytes=%u",
				modeCount, currentModeIndex, ddModeInfoAddr, modeCount * (UINT32)sizeof(NPDISP_DDHALMODEINFO)));
			TRACEOUT11(("NPDISP11 DD_MODE_CURRENT size=%ux%u pitch=%d bpp=%u flags=%04x refresh=%u masks=%08x/%08x/%08x/%08x",
				currentMode.dwWidth, currentMode.dwHeight, currentMode.lPitch, currentMode.dwBPP,
				currentMode.wFlags, currentMode.wRefreshRate, currentMode.dwRBitMask, currentMode.dwGBitMask,
				currentMode.dwBBitMask, currentMode.dwAlphaBitMask));
			TRACEOUT11(("NPDISP11 DD_CREATE_HAL hinst=%08x ctx=%08x saved=%08x ds=%04x", halInfo.hInstance, ddHalInfoAddr, npdisp.mm_ddHalInfoAddr, CPU_DS));
			if (!halInfo.hInstance) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT DriverInit did not publish HAL HINSTANCE ctx=%08x", ddHalInfoAddr));
				retValue = 0;
				break;
			}
			// modeInfoとhalInfoは16bit display driverの共有メモリへ書き込む。
			if (!npdisp_writeMemory(modeInfo, ddModeInfoAddr, modeCount * sizeof(NPDISP_DDHALMODEINFO)) ||
				!npdisp_writeMemory(&halInfo, ddHalInfoAddr, sizeof(halInfo))) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT shared write failed hal=%08x mode=%08x",
					ddHalInfoAddr, ddModeInfoAddr));
				retValue = 0;
				break;
			}
			retValue = 1;
		}
		else {
			TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT ver=%08x expect=%08x npver=%u win9x=%u hal=%08x mode=%08x cb=%08x surfcb=%08x palcb=%08x bppbytes=%u",
				dciCmd.dwVersion, NPDISP_DD_VERSION, npdisp.version, npdisp.isWin9x,
				npdisp.mm_ddHalInfoAddr, npdisp.mm_ddModeInfoAddr, npdisp.mm_ddCallbacksAddr, npdisp.mm_ddSurfaceCallbacksAddr, npdisp.mm_ddPaletteCallbacksAddr,
				npdisp_ddraw_bytesPerPixel()));
			retValue = 0;
		}
	} while (0);
	return retValue;
}

// 32bit HAL DLLへ渡す共有領域とコールバック表を準備する。
static UINT16 npdisp_dd_controlGet32BitDriverName(const NPDISP_DCICMD* cmd, UINT32 lpOutDataAddr)
{
	UINT16 retValue = 0;
	if (!cmd) return 0;
	const NPDISP_DCICMD& dciCmd = *cmd;
	do {

		const UINT32 ddHalInfoAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddHalInfoAddr);
		const UINT32 ddCallbacksAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddCallbacksAddr);
		const UINT32 ddSurfaceCallbacksAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddSurfaceCallbacksAddr);
		const UINT32 ddPaletteCallbacksAddr = npdisp_dd_currentDataPtr(npdisp.mm_ddPaletteCallbacksAddr);
		TRACEOUT(("DDGET32BITDRIVERNAME v=%08x out=%08x", dciCmd.dwVersion, lpOutDataAddr));
		if (dciCmd.dwVersion == NPDISP_DD_VERSION && npdisp.version >= 8 && npdisp.isWin9x && lpOutDataAddr) {
			if (!npdisp_dd_prepareCurrentCallbackTables(ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr)) {
				TRACEOUT11(("NPDISP11 DD32_NAME callback sync failed cb=%08x surfcb=%08x palcb=%08x",
					ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr));
				retValue = 0;
				break;
			}
			NPDISP_DD32BITDRIVERDATA driverData = { 0 };
			UINT32 linearContext = 0;
			UINT32 linearCallbacks = 0;
			UINT32 linearSurfaceCallbacks = 0;
			UINT32 linearPaletteCallbacks = 0;
			lstrcpyA(driverData.szName, "NPDISPDD.DLL");
			lstrcpyA(driverData.szEntryPoint, "DriverInit");
			// DDHelpはdwContextを32bit HAL DLLへ渡す。DDHALINFOは初期化時の共有領域としても使用する。
			if (!npdisp_memory_getLinearAddress(ddHalInfoAddr, &linearContext) ||
				!npdisp_memory_getLinearAddress(ddCallbacksAddr, &linearCallbacks) ||
				!npdisp_memory_getLinearAddress(ddSurfaceCallbacksAddr, &linearSurfaceCallbacks) ||
				!npdisp_memory_getLinearAddress(ddPaletteCallbacksAddr, &linearPaletteCallbacks)) {
				TRACEOUT11(("NPDISP11 DD32_NAME context conversion failed hal=%08x cb=%08x surfcb=%08x palcb=%08x",
					npdisp.mm_ddHalInfoAddr, npdisp.mm_ddCallbacksAddr, npdisp.mm_ddSurfaceCallbacksAddr, npdisp.mm_ddPaletteCallbacksAddr));
				retValue = 0;
				break;
			}
			// DriverInit前はdwFlagsを要求maskとして使う。
			// 要求したentryだけを32bit callbackで設定し、それ以外は未登録のままにする。
			npdisp_writeMemory32(NPDISP_DDBRIDGE_DD_REQUEST_MASK, ddCallbacksAddr + offsetof(NPDISP_DDHAL_DDCALLBACKS, dwFlags));
			npdisp_writeMemory32(NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK, ddSurfaceCallbacksAddr + offsetof(NPDISP_DDHAL_DDSURFACECALLBACKS, dwFlags));
			npdisp_writeMemory32(NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK, ddPaletteCallbacksAddr + offsetof(NPDISP_DDHAL_DDPALETTECALLBACKS, dwFlags));

			// 初期化中だけDDHALINFOへflat callback tableアドレスとABI情報を格納する。
			// DDCREATEDRIVEROBJECTで最終DDHALINFOへ作り直す際にこれらは消去する。
			npdisp_writeMemory32(linearCallbacks, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDCallbacksAddr));
			npdisp_writeMemory32(linearSurfaceCallbacks, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDSurfaceCallbacksAddr));
			npdisp_writeMemory32(linearPaletteCallbacks, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDPaletteCallbacksAddr));
			npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, GetDriverInfoAddr));
			npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, hInstance));
			npdisp_writeMemory32(NPDISP_DDBRIDGE_REQUEST_MAGIC, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpD3DGlobalDriverData));
			npdisp_writeMemory32(NPDISP_DDBRIDGE_ABI_VERSION, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpD3DHALCallbacks));
			npdisp_writeMemory32(NPDISP_DDBRIDGE_REQUEST_FEATURES, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDExeBufCallbacksAddr));
			driverData.dwContext = linearContext;
			TRACEOUT11(("NPDISP11 DD32_NAME far=%08x context=%08x cb=%08x surf=%08x pal=%08x mask=%08x/%08x/%08x abi=%08x feat=%08x",
				ddHalInfoAddr, driverData.dwContext, linearCallbacks, linearSurfaceCallbacks, linearPaletteCallbacks,
				NPDISP_DDBRIDGE_DD_REQUEST_MASK, NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK, NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK,
				NPDISP_DDBRIDGE_ABI_VERSION, NPDISP_DDBRIDGE_REQUEST_FEATURES));
			npdisp_writeMemory(&driverData, lpOutDataAddr, sizeof(driverData));
			retValue = 1;
		}
		else {
			retValue = 0;
		}
	} while (0);
	return retValue;
}

// DDNEWCALLBACKFNSの引数を検証する。
static UINT16 npdisp_dd_controlNewCallbackFns(const NPDISP_DCICMD* cmd)
{
	if (!cmd) return 0;
	return (cmd->dwVersion == NPDISP_DD_VERSION && cmd->dwParam1) ? 1 : 0;
}

// DDVERSIONINFOへDirectDrawランタイムのバージョン情報を返す。
static UINT16 npdisp_dd_controlVersionInfo(const NPDISP_DCICMD* cmd, UINT32 lpOutDataAddr)
{
	if (!cmd) return 0;
	TRACEOUT(("DDVERSIONINFO v=%08x p1=%08x p2=%08x out=%08x", cmd->dwVersion, cmd->dwParam1, cmd->dwParam2, lpOutDataAddr));
	if (cmd->dwVersion == NPDISP_DD_VERSION && lpOutDataAddr) {
		NPDISP_DDVERSIONDATA ver = { NPDISP_DD_RUNTIME_VERSION, 0, 0 };
		TRACEOUT11(("NPDISP11 DD_VERSION compat=%08x runtime=%08x", NPDISP_DD_VERSION, NPDISP_DD_RUNTIME_VERSION));
		npdisp_writeMemory(&ver, lpOutDataAddr, sizeof(ver));
		return 1;
	}
	return 0;
}

// DCICOMMANDのDirectDraw固有処理を実行する。
UINT16 npdisp_dd_controlCommand(UINT32 lpDestDevAddr, const NPDISP_DCICMD* cmd, UINT32 lpOutDataAddr)
{
	if (!cmd) return 0;
	switch (cmd->dwCommand) {
	case NPDISP_CONTROL_DCI_DDCREATEDRIVEROBJECT:
		return npdisp_dd_controlCreateDriverObject(lpDestDevAddr, cmd);
	case NPDISP_CONTROL_DCI_DDGET32BITDRIVERNAME:
		return npdisp_dd_controlGet32BitDriverName(cmd, lpOutDataAddr);
	case NPDISP_CONTROL_DCI_DDNEWCALLBACKFNS:
		return npdisp_dd_controlNewCallbackFns(cmd);
	case NPDISP_CONTROL_DCI_DDVERSIONINFO:
		return npdisp_dd_controlVersionInfo(cmd, lpOutDataAddr);
	default:
		return 0;
	}
}

// 固定2Dブリッジのcallback IDをDirectDraw処理へ振り分ける。
UINT32 npdisp_dd_dispatchBridge(UINT32 callbackId, UINT32 lpDataAddr)
{
	UINT32 retValue = NPDISP_DDHAL_DRIVER_NOTHANDLED;
	switch (callbackId) {
	case NPDISP_DDBRIDGE_CB_DD_CREATESURFACE: retValue = npdisp_func_DD_CreateSurface(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_DD_WAITVB: retValue = npdisp_func_DD_WaitForVerticalBlank(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_DD_CANCREATESURFACE: retValue = npdisp_func_DD_CanCreateSurface(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_DD_GETSCANLINE: retValue = npdisp_func_DD_GetScanLine(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_DD_FLIPTOGDI: retValue = npdisp_func_DD_FlipToGDISurface(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_DESTROY: retValue = npdisp_func_DD_DestroySurface(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_FLIP: retValue = npdisp_func_DD_Flip(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_SETCLIPLIST: retValue = npdisp_func_DD_SetClipList(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_LOCK: retValue = npdisp_func_DD_Lock(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_UNLOCK: retValue = npdisp_func_DD_Unlock(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_BLT: retValue = npdisp_func_DD_Blt(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_SETCOLORKEY: retValue = npdisp_func_DD_SetColorKey(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_ADDATTACHED: retValue = npdisp_func_DD_AddAttachedSurface(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_GETBLTSTATUS: retValue = npdisp_func_DD_GetBltStatus(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_GETFLIPSTATUS: retValue = npdisp_func_DD_GetFlipStatus(lpDataAddr, true); break;
	default: break;
	}
	TRACEOUT11(("NPDISP11 DD32_BRIDGE id=%04x data=%08x ret=%08x", callbackId, lpDataAddr, retValue));
	return retValue;
}


#endif
