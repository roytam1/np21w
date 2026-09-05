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
	npdisp.mm_ddOverlayVisible = 0;
	npdisp.mm_ddOverlayOffset = 0;
	npdisp.mm_ddOverlayFourCC = 0;
}

static UINT32 npdisp_ddraw_bytesPerPixel(void);
static void npdisp_dd_overlayDirty(void);

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
	halInfo->vmiData.dwOverlayAlign = 0;
	halInfo->vmiData.dwAlphaAlign = 0;
	halInfo->vmiData.dwNumHeaps = 0;
	halInfo->vmiData.pvmList = 0;
	halInfo->ddCaps.dwVidMemTotal = 0;
	halInfo->ddCaps.dwVidMemFree = 0;
	halInfo->ddCaps.ddsCaps.dwCaps &= ~(NPDISP_DDSCAPS_ALPHA | NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_FLIP |
		NPDISP_DDSCAPS_COMPLEX | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_BACKBUFFER |
		NPDISP_DDSCAPS_OVERLAY | NPDISP_DDSCAPS_VIDEOMEMORY);

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
	halInfo->vmiData.dwOverlayAlign = 4;
	halInfo->vmiData.dwAlphaAlign = 4;
	halInfo->vmiData.dwNumHeaps = 1;
	halInfo->vmiData.pvmList = npdisp.mm_ddVidMemAddr;
	halInfo->ddCaps.dwVidMemTotal = NPDISP_DD_OFFSCREEN_SIZE;
	halInfo->ddCaps.dwVidMemFree = NPDISP_DD_OFFSCREEN_SIZE;
	halInfo->ddCaps.ddsCaps.dwCaps |= NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_OVERLAY | NPDISP_DDSCAPS_VIDEOMEMORY;
	if (npdisp.bpp >= 15) halInfo->ddCaps.ddsCaps.dwCaps |= NPDISP_DDSCAPS_ALPHA;
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
	if (npdisp.version >= 14 && npdisp.bpp >= 15) {
		const UINT32 fourCCAddr = ddModeInfoAddr + NPDISP_DD_FOURCC_SLOT_OFFSET;
		UINT32 fourCC = NPDISP_DD_FOURCC_YUY2;
		halInfo.ddCaps.dwNumFourCCCodes = 1;
		halInfo.lpdwFourCC = fourCCAddr;
		npdisp_writeMemory(&fourCC, fourCCAddr, sizeof(fourCC));
	}
	else {
		halInfo.ddCaps.dwNumFourCCCodes = 0;
		halInfo.lpdwFourCC = 0;
	}
	halInfo.lpPDevice = lpPDeviceAddr;
	// Display-mode changes invalidate the current overlay presentation state.
	npdisp.mm_ddOverlayVisible = 0;
	npdisp.mm_ddOverlayOffset = 0;
	npdisp.mm_ddOverlayFourCC = 0;
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
	UINT32 fourCC;
} NPDISP_DDSURFACE_VIEW;

enum { NPDISP_DD_PATTERN_SIZE = 8 };
// Win9x DirectDraw runtimeはpattern ROPをHALへ渡す際、dwFlags bit31とdwROPFlagsのpattern bitを設定する。
static const UINT32 NPDISP_DDBLT_RUNTIME_PATTERN_ROP = 0x80000000UL;
static const UINT32 NPDISP_DD_ROPFLAG_HAS_PATTERN = 0x00000002UL;

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

static bool npdisp_ddraw_pixelFormatIsYUY2(const NPDISP_DDPIXELFORMAT* pf)
{
	return pf && pf->dwSize >= sizeof(*pf) && (pf->dwFlags & NPDISP_DDPF_FOURCC) &&
		pf->dwFourCC == NPDISP_DD_FOURCC_YUY2;
}

static bool npdisp_ddraw_pixelFormatIsAlpha8(const NPDISP_DDPIXELFORMAT* pf)
{
	return pf && pf->dwSize >= sizeof(*pf) && (pf->dwFlags & NPDISP_DDPF_ALPHA) &&
		!(pf->dwFlags & (NPDISP_DDPF_FOURCC | NPDISP_DDPF_RGB | NPDISP_DDPF_PALETTEINDEXED8)) &&
		pf->dwAlphaBitDepth == 8;
}

static bool npdisp_ddraw_descIsAlpha8(const NPDISP_DDSURFACEDESC* desc)
{
	if (!desc || !(desc->ddsCaps.dwCaps & NPDISP_DDSCAPS_ALPHA)) return false;
	if ((desc->dwFlags & NPDISP_DDSD_PIXELFORMAT) && !npdisp_ddraw_pixelFormatIsAlpha8(&desc->ddpfPixelFormat)) return false;
	if ((desc->dwFlags & NPDISP_DDSD_ALPHABITDEPTH) && desc->dwAlphaBitDepth != 8) return false;
	return (desc->dwFlags & (NPDISP_DDSD_PIXELFORMAT | NPDISP_DDSD_ALPHABITDEPTH)) != 0;
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
	// DDSCAPS_ALPHAは表示色形式ではなく8bpp Alpha専用surfaceとして扱う。
	if (caps & NPDISP_DDSCAPS_ALPHA) return false;
	if (caps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		v.width = v.gbl.wWidth;
		v.height = v.gbl.wHeight;
		v.pitch = v.gbl.lPitch;
		const UINT64 rowBytes = (UINT64)v.width * npdisp_ddraw_bytesPerPixel();
		if (!allowSystemMemory || (caps & (NPDISP_DDSCAPS_PRIMARYSURFACE | NPDISP_DDSCAPS_VIDEOMEMORY |
			NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ||
			!v.gbl.fpVidMem || !v.width || !v.height || v.pitch <= 0 || !rowBytes || rowBytes > (UINT32)v.pitch) {
			return false;
		}
		if (v.lcl.dwFlags & NPDISP_DDRAWISURF_HASPIXELFORMAT) {
			NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT fullGbl = { 0 };
			if (!npdisp_ddraw_readGuest(&fullGbl, v.lcl.lpGbl, sizeof(fullGbl), flatAddress) ||
				!npdisp_ddraw_pixelFormatMatchesDisplay(&fullGbl.ddpfSurface)) return false;
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
	if (!(caps & (NPDISP_DDSCAPS_PRIMARYSURFACE | NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_OVERLAY |
		NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP))) {
		return false;
	}

	v.width = v.gbl.wWidth ? v.gbl.wWidth : npdisp.width;
	v.height = v.gbl.wHeight ? v.gbl.wHeight : npdisp.height;
	v.pitch = v.gbl.lPitch ? v.gbl.lPitch : (SINT32)npdispwin.stride;
	v.primaryObject = (caps & NPDISP_DDSCAPS_PRIMARYSURFACE) != 0;
	if (npdisp.version >= 14) {
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT fullGbl = { 0 };
		if (npdisp_ddraw_readGuest(&fullGbl, v.lcl.lpGbl, sizeof(fullGbl), flatAddress) &&
			npdisp_ddraw_pixelFormatIsYUY2(&fullGbl.ddpfSurface)) {
			v.fourCC = NPDISP_DD_FOURCC_YUY2;
		}
	}
	if (v.fourCC == NPDISP_DD_FOURCC_YUY2) {
		const UINT32 minPitch = (v.width * 2U + 3U) & ~3U;
		if ((v.width & 1U) || v.pitch < (SINT32)minPitch) return false;
	}
	if (!npdisp_ddraw_resolveVideoAddress(v.gbl.fpVidMem, v.pitch, v.height,
		&v.hostBase, &v.linearBase, &v.apertureOffset)) {
		return false;
	}
	v.visible = (v.apertureOffset == npdisp.mm_ddScanoutOffset);

	*view = v;
	return true;
}

static bool npdisp_ddraw_getPatternSurface(UINT32 lpPatternSurfaceAddr, NPDISP_DDSURFACE_VIEW* view, bool flatAddress)
{
	NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };

	if (!lpPatternSurfaceAddr || !view) return false;

	// Win32 HAL Blt callbackではDirectDraw runtimeがlpDDSPatternをDDRAWI_DDRAWSURFACE_LCLへ変換して渡す。
	if (flatAddress) return npdisp_ddraw_getSurfaceEx(lpPatternSurfaceAddr, view, true, true);

	// 16bit互換経路では公開surface interfaceが渡される場合も受理する。
	if (npdisp_ddraw_readGuest(&surfaceInt, lpPatternSurfaceAddr, sizeof(surfaceInt), false) &&
		surfaceInt.lpLcl && npdisp_ddraw_getSurfaceEx(surfaceInt.lpLcl, view, false, true)) {
		return true;
	}
	return npdisp_ddraw_getSurfaceEx(lpPatternSurfaceAddr, view, false, true);
}

typedef struct {
	bool systemMemory;
	UINT8* hostBase;
	UINT32 linearBase;
	UINT32 width;
	UINT32 height;
	SINT32 pitch;
} NPDISP_DDALPHA_VIEW;

typedef struct {
	bool enabled;
	bool negate;
	bool constant;
	UINT8 constantValue;
	NPDISP_DDALPHA_VIEW surface;
} NPDISP_DDALPHA_CHANNEL;

static bool npdisp_ddraw_getAlphaSurfaceEx(UINT32 lpSurfaceAddr, NPDISP_DDALPHA_VIEW* view, bool flatAddress, bool allowSystemMemory)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT fullGbl = { 0 };
	NPDISP_DDALPHA_VIEW v = { 0 };
	UINT32 caps;
	UINT32 apertureOffset;

	if (!lpSurfaceAddr || !view ||
		!npdisp_ddraw_readGuest(&lcl, lpSurfaceAddr, sizeof(lcl), flatAddress) || !lcl.lpGbl ||
		!npdisp_ddraw_readGuest(&gbl, lcl.lpGbl, sizeof(gbl), flatAddress) ||
		!npdisp_ddraw_readGuest(&fullGbl, lcl.lpGbl, sizeof(fullGbl), flatAddress)) return false;

	caps = lcl.ddsCaps.dwCaps;
	if (!(caps & NPDISP_DDSCAPS_ALPHA) || (caps & (NPDISP_DDSCAPS_PRIMARYSURFACE | NPDISP_DDSCAPS_OVERLAY |
		NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ||
		!npdisp_ddraw_pixelFormatIsAlpha8(&fullGbl.ddpfSurface)) return false;

	v.width = gbl.wWidth;
	v.height = gbl.wHeight;
	v.pitch = gbl.lPitch;
	if (!v.width || !v.height || v.pitch <= 0 || v.width > (UINT32)v.pitch || !gbl.fpVidMem) return false;

	if (caps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		if (!allowSystemMemory || (caps & NPDISP_DDSCAPS_VIDEOMEMORY)) return false;
		v.systemMemory = true;
		v.linearBase = gbl.fpVidMem;
	}
	else if (!npdisp_ddraw_resolveVideoAddress(gbl.fpVidMem, v.pitch, v.height, &v.hostBase, &v.linearBase, &apertureOffset)) return false;

	*view = v;
	return true;
}

static bool npdisp_ddraw_getAlphaSurfacePointer(UINT32 lpAlphaSurfaceAddr, NPDISP_DDALPHA_VIEW* view, bool flatAddress)
{
	NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
	if (!lpAlphaSurfaceAddr || !view) return false;

	// Win32 HAL callbackではDDBLTFX内のsurface pointerはLCLを指す。
	if (flatAddress) return npdisp_ddraw_getAlphaSurfaceEx(lpAlphaSurfaceAddr, view, true, true);

	// 16bit経路ではsurface interface経由も受け付ける。
	if (npdisp_ddraw_readGuest(&surfaceInt, lpAlphaSurfaceAddr, sizeof(surfaceInt), false) && surfaceInt.lpLcl &&
		npdisp_ddraw_getAlphaSurfaceEx(surfaceInt.lpLcl, view, false, true)) return true;
	return npdisp_ddraw_getAlphaSurfaceEx(lpAlphaSurfaceAddr, view, false, true);
}

static bool npdisp_ddraw_findAttachedAlphaSurface(UINT32 lpSurfaceAddr, NPDISP_DDALPHA_VIEW* view, bool flatAddress)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	UINT32 linkAddr;
	UINT32 count = 0;

	if (!lpSurfaceAddr || !view || !npdisp_ddraw_readGuest(&lcl, lpSurfaceAddr, sizeof(lcl), flatAddress)) return false;
	linkAddr = lcl.lpAttachList;
	while (linkAddr && count++ < 64) {
		NPDISP_DDATTACHLIST link = { 0 };
		if (!npdisp_ddraw_readGuest(&link, linkAddr, sizeof(link), flatAddress)) return false;
		if (link.lpAttached && npdisp_ddraw_getAlphaSurfaceEx(link.lpAttached, view, flatAddress, true)) return true;
		if (link.lpLink == linkAddr) return false;
		linkAddr = link.lpLink;
	}
	return false;
}

static bool npdisp_ddraw_readAlphaValue(const NPDISP_DDALPHA_VIEW* view, SINT32 x, SINT32 y, bool flatAddress, UINT8* value)
{
	if (!view || !value || x < 0 || y < 0 || x >= (SINT32)view->width || y >= (SINT32)view->height) return false;
	if (view->systemMemory) {
		const UINT64 addr = (UINT64)view->linearBase + (UINT64)(UINT32)view->pitch * (UINT32)y + (UINT32)x;
		return addr <= 0xffffffffUL && npdisp_ddraw_readGuest(value, (UINT32)addr, 1, flatAddress) != 0;
	}
	*value = view->hostBase[(size_t)y * (size_t)view->pitch + (size_t)x];
	return true;
}

static bool npdisp_ddraw_normalizeAlphaConst(UINT32 bitDepth, UINT32 value, UINT8* alpha)
{
	UINT32 maxValue;
	if (!alpha || (bitDepth != 2 && bitDepth != 4 && bitDepth != 8)) return false;
	maxValue = (1U << bitDepth) - 1U;
	if (value > maxValue) return false;
	*alpha = (UINT8)((value * 255U + maxValue / 2U) / maxValue);
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

static bool npdisp_ddraw_snapshotPattern(const NPDISP_DDSURFACE_VIEW* view, UINT32 bytesPerPixel, bool flatAddress, UINT8* pixels, size_t pitch)
{
	const UINT32 rowBytes = NPDISP_DD_PATTERN_SIZE * bytesPerPixel;

	if (!view || !pixels || !bytesPerPixel || bytesPerPixel > 4 || view->fourCC ||
		view->width != NPDISP_DD_PATTERN_SIZE || view->height != NPDISP_DD_PATTERN_SIZE ||
		view->pitch < (SINT32)rowBytes || pitch < rowBytes) {
		return false;
	}

	for (UINT32 y = 0; y < NPDISP_DD_PATTERN_SIZE; ++y) {
		UINT8* d = pixels + (size_t)y * pitch;
		if (view->systemMemory) {
			const UINT64 guestAddr = (UINT64)view->linearBase + (UINT64)(UINT32)view->pitch * y;
			if (guestAddr > 0xffffffffUL || !npdisp_ddraw_readGuest(d, (UINT32)guestAddr, (int)rowBytes, flatAddress)) return false;
		}
		else {
			const UINT8* s = npdisp_ddraw_surfacePtr(view, 0, (SINT32)y, bytesPerPixel);
			memcpy(d, s, rowBytes);
		}
	}
	return true;
}

// 回転後のsource画像上の座標を元source座標へ戻す。Mirrorは回転後画像の左右・上下に適用する。
static void npdisp_ddraw_transformBltSample(SINT32* sourceX, SINT32* sourceY, SINT32 x, SINT32 y, SINT32 srcWidth, SINT32 srcHeight, UINT32 rotation, bool mirrorX, bool mirrorY)
{
	const SINT32 rotatedWidth = (rotation == 90 || rotation == 270) ? srcHeight : srcWidth;
	const SINT32 rotatedHeight = (rotation == 90 || rotation == 270) ? srcWidth : srcHeight;

	if (mirrorX) x = rotatedWidth - 1 - x;
	if (mirrorY) y = rotatedHeight - 1 - y;
	switch (rotation) {
	case 90:
		*sourceX = y;
		*sourceY = srcHeight - 1 - x;
		break;
	case 180:
		*sourceX = srcWidth - 1 - x;
		*sourceY = srcHeight - 1 - y;
		break;
	case 270:
		*sourceX = srcWidth - 1 - y;
		*sourceY = x;
		break;
	default:
		*sourceX = x;
		*sourceY = y;
		break;
	}
}

// destinationの実描画矩形から、nearest-neighbor変換で参照されるsource範囲を求める。
static bool npdisp_ddraw_mapDestRectToSource(NPDISP_DDRECTL* srcRect, const NPDISP_DDRECTL* dstRect, const NPDISP_DDRECTL* mapDst, const NPDISP_DDRECTL* mapSrc, UINT32 rotation, bool mirrorX, bool mirrorY)
{
	const SINT32 dstWidth = mapDst ? mapDst->right - mapDst->left : 0;
	const SINT32 dstHeight = mapDst ? mapDst->bottom - mapDst->top : 0;
	const SINT32 srcWidth = mapSrc ? mapSrc->right - mapSrc->left : 0;
	const SINT32 srcHeight = mapSrc ? mapSrc->bottom - mapSrc->top : 0;
	const bool swapAxes = rotation == 90 || rotation == 270;
	const SINT32 rotatedWidth = swapAxes ? srcHeight : srcWidth;
	const SINT32 rotatedHeight = swapAxes ? srcWidth : srcHeight;
	UINT64 x0, x1, y0, y1;
	SINT32 rx0, rx1, ry0, ry1;
	SINT32 sx[4], sy[4];
	SINT32 minX, maxX, minY, maxY;

	if (!srcRect || !dstRect || !mapDst || !mapSrc || dstWidth <= 0 || dstHeight <= 0 ||
		srcWidth <= 0 || srcHeight <= 0 || (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) ||
		dstRect->left < mapDst->left || dstRect->top < mapDst->top || dstRect->right > mapDst->right ||
		dstRect->bottom > mapDst->bottom || dstRect->left >= dstRect->right || dstRect->top >= dstRect->bottom) return false;

	x0 = (UINT64)(dstRect->left - mapDst->left);
	x1 = (UINT64)(dstRect->right - 1 - mapDst->left);
	y0 = (UINT64)(dstRect->top - mapDst->top);
	y1 = (UINT64)(dstRect->bottom - 1 - mapDst->top);
	rx0 = (SINT32)((x0 * (UINT32)rotatedWidth) / (UINT32)dstWidth);
	rx1 = (SINT32)((x1 * (UINT32)rotatedWidth) / (UINT32)dstWidth);
	ry0 = (SINT32)((y0 * (UINT32)rotatedHeight) / (UINT32)dstHeight);
	ry1 = (SINT32)((y1 * (UINT32)rotatedHeight) / (UINT32)dstHeight);
	npdisp_ddraw_transformBltSample(&sx[0], &sy[0], rx0, ry0, srcWidth, srcHeight, rotation, mirrorX, mirrorY);
	npdisp_ddraw_transformBltSample(&sx[1], &sy[1], rx1, ry0, srcWidth, srcHeight, rotation, mirrorX, mirrorY);
	npdisp_ddraw_transformBltSample(&sx[2], &sy[2], rx0, ry1, srcWidth, srcHeight, rotation, mirrorX, mirrorY);
	npdisp_ddraw_transformBltSample(&sx[3], &sy[3], rx1, ry1, srcWidth, srcHeight, rotation, mirrorX, mirrorY);
	minX = maxX = sx[0];
	minY = maxY = sy[0];
	for (UINT32 i = 1; i < 4; ++i) {
		if (sx[i] < minX) minX = sx[i];
		if (sx[i] > maxX) maxX = sx[i];
		if (sy[i] < minY) minY = sy[i];
		if (sy[i] > maxY) maxY = sy[i];
	}
	srcRect->left = mapSrc->left + minX;
	srcRect->right = mapSrc->left + maxX + 1;
	srcRect->top = mapSrc->top + minY;
	srcRect->bottom = mapSrc->top + maxY + 1;
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

static void npdisp_ddraw_freeSystemSourceMirrors(NPDISP_DDRECTL* rects, NPDISP_DDSURFACE_VIEW* views, UINT8** buffers, UINT32 count)
{
	if (buffers) {
		for (UINT32 i = 0; i < count; ++i) if (buffers[i]) free(buffers[i]);
		free(buffers);
	}
	if (views) free(views);
	if (rects) free(rects);
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

bool npdisp_dd_overlayVisible(void)
{
	UINT64 bytes;
	UINT64 base;
	if (!npdisp.mm_ddOverlayVisible || !npdisp.mm_ddOverlayOffset || !npdisp.mm_ddOffscreenPtr ||
		!npdisp.mm_ddOverlayWidth || !npdisp.mm_ddOverlayHeight || npdisp.mm_ddOverlayPitch <= 0) return false;
	if (npdisp.mm_ddOverlayOffset < NPDISP_DD_OFFSCREEN_OFFSET || npdisp.mm_ddOverlayOffset >= NPDISP_DD_APERTURE_SIZE) return false;
	base = npdisp.mm_ddOverlayOffset - NPDISP_DD_OFFSCREEN_OFFSET;
	bytes = (UINT64)(UINT32)npdisp.mm_ddOverlayPitch * npdisp.mm_ddOverlayHeight;
	return base + bytes <= NPDISP_DD_OFFSCREEN_SIZE;
}

static UINT32 npdisp_dd_overlayReadPixel(const UINT8* p, UINT32 bytesPerPixel)
{
	UINT32 v = 0;
	for (UINT32 i = 0; i < bytesPerPixel; ++i) v |= (UINT32)p[i] << (i * 8);
	return v;
}

static UINT8 npdisp_dd_clipByte(SINT32 v)
{
	if (v < 0) return 0;
	if (v > 255) return 255;
	return (UINT8)v;
}

static void npdisp_dd_yuy2ToRgb(const UINT8* pair, bool secondPixel, UINT8* r, UINT8* g, UINT8* b)
{
	const SINT32 y = (SINT32)pair[secondPixel ? 2 : 0] - 16;
	const SINT32 u = (SINT32)pair[1] - 128;
	const SINT32 v = (SINT32)pair[3] - 128;
	const SINT32 c = (y > 0) ? y : 0;

	*r = npdisp_dd_clipByte((298 * c + 409 * v + 128) >> 8);
	*g = npdisp_dd_clipByte((298 * c - 100 * u - 208 * v + 128) >> 8);
	*b = npdisp_dd_clipByte((298 * c + 516 * u + 128) >> 8);
}

static void npdisp_dd_writeRgbPixel(UINT8* p, UINT32 bytesPerPixel, UINT8 r, UINT8 g, UINT8 b)
{
	if (npdisp.bpp == 15) {
		const UINT16 v = (UINT16)(((UINT16)(r >> 3) << 10) | ((UINT16)(g >> 3) << 5) | (UINT16)(b >> 3));
		p[0] = (UINT8)v;
		p[1] = (UINT8)(v >> 8);
	}
	else if (npdisp.bpp == 16) {
		const UINT16 v = (UINT16)(((UINT16)(r >> 3) << 11) | ((UINT16)(g >> 2) << 5) | (UINT16)(b >> 3));
		p[0] = (UINT8)v;
		p[1] = (UINT8)(v >> 8);
	}
	else if (bytesPerPixel >= 3) {
		p[0] = b;
		p[1] = g;
		p[2] = r;
		if (bytesPerPixel == 4) p[3] = 0;
	}
}

void npdisp_dd_compositeOverlay(UINT8* dest, UINT32 destStride, int left, int top, int right, int bottom)
{
	const UINT32 bytesPerPixel = npdisp_ddraw_bytesPerPixel();
	const bool yuy2 = npdisp.mm_ddOverlayFourCC == NPDISP_DD_FOURCC_YUY2;
	const bool destKey = (npdisp.mm_ddOverlayFlags & (NPDISP_DDOVER_KEYDEST | NPDISP_DDOVER_KEYDESTOVERRIDE)) != 0;
	const bool srcKey = (npdisp.mm_ddOverlayFlags & (NPDISP_DDOVER_KEYSRC | NPDISP_DDOVER_KEYSRCOVERRIDE)) != 0;
	const bool mirrorX = (npdisp.mm_ddOverlayDDFX & NPDISP_DDOVERFX_MIRRORLEFTRIGHT) != 0;
	const bool mirrorY = (npdisp.mm_ddOverlayDDFX & NPDISP_DDOVERFX_MIRRORUPDOWN) != 0;
	SINT32 dl = npdisp.mm_ddOverlayDstLeft;
	SINT32 dt = npdisp.mm_ddOverlayDstTop;
	SINT32 dr = npdisp.mm_ddOverlayDstRight;
	SINT32 db = npdisp.mm_ddOverlayDstBottom;
	const SINT32 sw = npdisp.mm_ddOverlaySrcRight - npdisp.mm_ddOverlaySrcLeft;
	const SINT32 sh = npdisp.mm_ddOverlaySrcBottom - npdisp.mm_ddOverlaySrcTop;
	const SINT32 dw = dr - dl;
	const SINT32 dh = db - dt;
	UINT8* srcBase;
	UINT32 xStep, xRemStep, yStep, yRemStep;
	UINT32 xBase, xRemBase, yBase, yRemBase;
	UINT32 ddOverlayDestKeyHigh = npdisp.mm_ddOverlayDestKeyHigh;

	if (!dest || !bytesPerPixel || !npdisp_dd_overlayVisible() || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
	if (yuy2 && npdisp.bpp < 15) return;
	if (left > dl) dl = left;
	if (top > dt) dt = top;
	if (right < dr) dr = right;
	if (bottom < db) db = bottom;
	if (dl < 0) dl = 0;
	if (dt < 0) dt = 0;
	if (dr > (SINT32)npdisp.width) dr = (SINT32)npdisp.width;
	if (db > (SINT32)npdisp.height) db = (SINT32)npdisp.height;
	if (dl >= dr || dt >= db) return;
	srcBase = npdisp.mm_ddOffscreenPtr + (npdisp.mm_ddOverlayOffset - NPDISP_DD_OFFSCREEN_OFFSET);

	// WORKAROUND: 色変換規則のずれを吸収する ただし真っ白は作らない
	if (npdisp.bpp == 16) {
		UINT32 b = ddOverlayDestKeyHigh & 0x1f;
		UINT32 g = (ddOverlayDestKeyHigh >> 5) & 0x3f;
		UINT32 r = (ddOverlayDestKeyHigh >> 11) & 0x1f;
		if (b <= 30) b++;
		if (g <= 62) g++;
		if (r <= 30) r++;
		ddOverlayDestKeyHigh = b | (g << 5) | (r << 11);
	}
	else if (npdisp.bpp == 15) {
		UINT32 b = ddOverlayDestKeyHigh & 0x1f;
		UINT32 g = (ddOverlayDestKeyHigh >> 5) & 0x1f;
		UINT32 r = (ddOverlayDestKeyHigh >> 10) & 0x1f;
		if (b <= 30) b++;
		if (g <= 30) g++;
		if (r <= 30) r++;
		ddOverlayDestKeyHigh = b | (g << 5) | (r << 10);
	}

	// Preserve floor((position * sourceSize) / destinationSize) without per-pixel division.
	xStep = (UINT32)(sw / dw);
	xRemStep = (UINT32)(sw % dw);
	yStep = (UINT32)(sh / dh);
	yRemStep = (UINT32)(sh % dh);
	{
		const UINT64 xStart = (UINT64)(UINT32)(dl - npdisp.mm_ddOverlayDstLeft) * (UINT32)sw;
		const UINT64 yStart = (UINT64)(UINT32)(dt - npdisp.mm_ddOverlayDstTop) * (UINT32)sh;
		xBase = (UINT32)(xStart / (UINT32)dw);
		xRemBase = (UINT32)(xStart % (UINT32)dw);
		yBase = (UINT32)(yStart / (UINT32)dh);
		yRemBase = (UINT32)(yStart % (UINT32)dh);
	}

	for (SINT32 y = dt; y < db; ++y) {
		const SINT32 sy = npdisp.mm_ddOverlaySrcTop + (mirrorY ? sh - 1 - (SINT32)yBase : (SINT32)yBase);
		UINT8* drow = dest + (UINT32)y * destStride;
		const UINT8* srow = srcBase + (UINT32)sy * (UINT32)npdisp.mm_ddOverlayPitch;
		UINT32 xBaseLine = xBase;
		UINT32 xRem = xRemBase;
		for (SINT32 x = dl; x < dr; ++x) {
			const SINT32 sx = npdisp.mm_ddOverlaySrcLeft + (mirrorX ? sw - 1 - (SINT32)xBaseLine : (SINT32)xBaseLine);
			UINT8* dp = drow + (UINT32)x * bytesPerPixel;
			bool drawPixel = true;

			if (destKey) {
				const UINT32 dv = npdisp_dd_overlayReadPixel(dp, bytesPerPixel);
				drawPixel = dv >= npdisp.mm_ddOverlayDestKeyLow && dv <= ddOverlayDestKeyHigh;
			}
			if (drawPixel) {
				if (yuy2) {
					UINT8 r, g, b;
					const UINT8* pair = srow + ((UINT32)sx & ~1U) * 2U;
					npdisp_dd_yuy2ToRgb(pair, (sx & 1) != 0, &r, &g, &b);
					npdisp_dd_writeRgbPixel(dp, bytesPerPixel, r, g, b);
				}
				else {
					const UINT8* sp = srow + (UINT32)sx * bytesPerPixel;
					if (!srcKey) {
						memcpy(dp, sp, bytesPerPixel);
					}
					else {
						const UINT32 sv = npdisp_dd_overlayReadPixel(sp, bytesPerPixel);
						if (sv < npdisp.mm_ddOverlaySrcKeyLow || sv > npdisp.mm_ddOverlaySrcKeyHigh) memcpy(dp, sp, bytesPerPixel);
					}
				}
			}

			xBaseLine += xStep;
			xRem += xRemStep;
			if (xRem >= (UINT32)dw) {
				xRem -= (UINT32)dw;
				xBaseLine++;
			}
		}
		yBase += yStep;
		yRemBase += yRemStep;
		if (yRemBase >= (UINT32)dh) {
			yRemBase -= (UINT32)dh;
			yBase++;
		}
	}
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
	if (!npdisp.active || !npdisp.mm_ddFlipPending) return;
	npdispcs_enter_criticalsection();
	if (npdisp.mm_ddFlipPending) npdisp_ddraw_commitPendingFlip();
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

static bool npdisp_ddraw_calcSurfaceAllocation(UINT32 width, UINT32 height, UINT32 bytesPerPixel, UINT32* pitch, UINT32* bytes)
{
	UINT64 rowBytes;
	UINT64 surfaceBytes;

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
	const bool overlay = (caps & NPDISP_DDSCAPS_OVERLAY) != 0;
	const bool alpha8 = npdisp.bpp >= 15 && npdisp_ddraw_descIsAlpha8(&desc);
	const bool yuy2 = overlay && npdisp.version >= 14 && npdisp.bpp >= 15 &&
		npdisp_ddraw_pixelFormatIsYUY2(&desc.ddpfPixelFormat);
	if ((!yuy2 && !alpha8 && data.bIsDifferentPixelFormat) || !npdisp_ddraw_bytesPerPixel() ||
		(caps & NPDISP_DDSCAPS_SYSTEMMEMORY)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	if (caps & NPDISP_DDSCAPS_ALPHA) {
		UINT32 pitch = 0;
		UINT32 surfaceBytes = 0;
		if (!alpha8 || !npdisp.mm_ddOffscreenPtr || !npdisp.mm_ddVidMemAddr || npdisp.version < 12 ||
			(caps & (NPDISP_DDSCAPS_PRIMARYSURFACE | NPDISP_DDSCAPS_OVERLAY | NPDISP_DDSCAPS_BACKBUFFER |
			NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) || !desc.dwWidth || !desc.dwHeight ||
			!npdisp_ddraw_calcSurfaceAllocation(desc.dwWidth, desc.dwHeight, 1, &pitch, &surfaceBytes)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		data.ddRVal = 0;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return NPDISP_DDHAL_DRIVER_HANDLED;
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
				!npdisp_ddraw_calcSurfaceAllocation(width, height, npdisp_ddraw_bytesPerPixel(), &pitch, &surfaceBytes) ||
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

	// Overlay flip chains are independent of the primary scanout size.
	if (overlay) {
		UINT32 pitch = 0;
		UINT32 surfaceBytes = 0;
		const UINT32 width = desc.dwWidth;
		const UINT32 height = desc.dwHeight;
		const UINT32 bytesPerPixel = yuy2 ? 2U : npdisp_ddraw_bytesPerPixel();
		if (!npdisp.mm_ddOffscreenPtr || !npdisp.mm_ddVidMemAddr || npdisp.version < 12 ||
			!width || !height || (yuy2 && (width & 1)) ||
			!npdisp_ddraw_calcSurfaceAllocation(width, height, bytesPerPixel, &pitch, &surfaceBytes)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
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
			!npdisp_ddraw_calcSurfaceAllocation(width, height, npdisp_ddraw_bytesPerPixel(), &pitch, &surfaceBytes)) {
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
	bool overlayRequest = false;
	bool alphaRequest = false;

	if (!lpDataAddr || !npdisp.isWin9x || npdisp.version < 6 || !npdisp.mm_screenPtr || !npdisp.mm_vramLinearAddr) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) || !data.dwSCnt || data.dwSCnt > 16 || !data.lplpSList) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.lpDDSurfaceDesc && npdisp_ddraw_readGuest(&desc, data.lpDDSurfaceDesc, sizeof(desc), flatAddress)) {
		haveDesc = true;
		overlayRequest = (desc.ddsCaps.dwCaps & NPDISP_DDSCAPS_OVERLAY) != 0;
		alphaRequest = (desc.ddsCaps.dwCaps & NPDISP_DDSCAPS_ALPHA) != 0;
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

		const bool overlaySurface = overlayRequest || ((caps & NPDISP_DDSCAPS_OVERLAY) != 0);
		const bool alphaSurface = alphaRequest || ((caps & NPDISP_DDSCAPS_ALPHA) != 0);
		if (overlaySurface) overlayRequest = true;
		if (alphaSurface) alphaRequest = true;
		if (alphaSurface) {
			UINT32 rowBytes = 0;
			UINT32 surfaceBytes = 0;
			UINT32 width = gbl.wWidth ? gbl.wWidth : (haveDesc ? desc.dwWidth : 0);
			UINT32 height = gbl.wHeight ? gbl.wHeight : (haveDesc ? desc.dwHeight : 0);
			if (!haveDesc || !npdisp_ddraw_descIsAlpha8(&desc) || npdisp.bpp < 15 || !npdisp.mm_ddOffscreenPtr ||
				!npdisp.mm_ddVidMemAddr || npdisp.version < 12 || (caps & (NPDISP_DDSCAPS_PRIMARYSURFACE |
				NPDISP_DDSCAPS_OVERLAY | NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ||
				!npdisp_ddraw_calcSurfaceAllocation(width, height, 1, &rowBytes, &surfaceBytes)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			gbl.fpVidMem = NPDISP_DDHAL_PLEASEALLOC_BLOCKSIZE;
			gbl.lPitch = (SINT32)rowBytes;
			gbl.dwBlockSizeX = surfaceBytes;
			gbl.dwBlockSizeY = 1;
			gbl.wWidth = (UINT16)width;
			gbl.wHeight = (UINT16)height;
			descPitch = rowBytes;
		}
		else if (caps & NPDISP_DDSCAPS_PRIMARYSURFACE) {
			gbl.fpVidMem = 0;
			gbl.lPitch = (SINT32)npdispwin.stride;
			gbl.dwBlockSizeX = npdisp.mm_screenSize;
			gbl.dwBlockSizeY = 1;
			gbl.wHeight = (UINT16)npdisp.height;
			gbl.wWidth = (UINT16)npdisp.width;
			descPitch = npdispwin.stride;
		}
		else if (caps & (NPDISP_DDSCAPS_OFFSCREENPLAIN | NPDISP_DDSCAPS_OVERLAY | NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) {
			UINT32 rowBytes = 0;
			UINT32 surfaceBytes = 0;
			UINT32 width = gbl.wWidth;
			UINT32 height = gbl.wHeight;

			if (!npdisp.mm_ddOffscreenPtr || !npdisp.mm_ddVidMemAddr || npdisp.version < 12) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			if (!width) width = (haveDesc && desc.dwWidth) ? desc.dwWidth :
				((caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ? npdisp.width : 0);
			if (!height) height = (haveDesc && desc.dwHeight) ? desc.dwHeight :
				((caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) ? npdisp.height : 0);

			// Only primary/display flip chains must match the current scanout dimensions.
			if (!overlaySurface &&
				(caps & (NPDISP_DDSCAPS_BACKBUFFER | NPDISP_DDSCAPS_FRONTBUFFER | NPDISP_DDSCAPS_FLIP)) &&
				(npdisp.version < 13 || width != npdisp.width || height != npdisp.height)) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}

			const bool yuy2 = overlaySurface && npdisp.version >= 14 && npdisp.bpp >= 15 &&
				haveDesc && npdisp_ddraw_pixelFormatIsYUY2(&desc.ddpfPixelFormat);
			if (yuy2 && (width & 1)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			const UINT32 bytesPerPixel = yuy2 ? 2U : npdisp_ddraw_bytesPerPixel();
			if (!npdisp_ddraw_calcSurfaceAllocation(width, height, bytesPerPixel, &rowBytes, &surfaceBytes)) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}

			gbl.fpVidMem = NPDISP_DDHAL_PLEASEALLOC_BLOCKSIZE;
			gbl.lPitch = (SINT32)rowBytes;
			gbl.dwBlockSizeX = surfaceBytes;
			gbl.dwBlockSizeY = 1;
			gbl.wWidth = (UINT16)width;
			gbl.wHeight = (UINT16)height;
			descPitch = rowBytes;
		}
		else {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (!npdisp_ddraw_writeGuest(&gbl, lcl.lpGbl, sizeof(gbl), flatAddress)) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (alphaSurface) {
			NPDISP_DDPIXELFORMAT alphaPf = { 0 };
			const UINT32 pfAddr = lcl.lpGbl + (UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT, ddpfSurface);
			alphaPf.dwSize = sizeof(alphaPf);
			alphaPf.dwFlags = NPDISP_DDPF_ALPHA;
			alphaPf.dwAlphaBitDepth = 8;
			if (!npdisp_ddraw_writeGuest(&alphaPf, pfAddr, sizeof(alphaPf), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		else if (overlaySurface && haveDesc && npdisp_ddraw_pixelFormatIsYUY2(&desc.ddpfPixelFormat)) {
			const UINT32 pfAddr = lcl.lpGbl + (UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT, ddpfSurface);
			if (!npdisp_ddraw_writeGuest(&desc.ddpfPixelFormat, pfAddr, sizeof(desc.ddpfPixelFormat), flatAddress)) {
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
		}
	}

	if (haveDesc && descPitch) {
		desc.lPitch = (SINT32)descPitch;
		desc.dwFlags |= NPDISP_DDSD_PITCH;
		if (alphaRequest) {
			memset(&desc.ddpfPixelFormat, 0, sizeof(desc.ddpfPixelFormat));
			desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
			desc.ddpfPixelFormat.dwFlags = NPDISP_DDPF_ALPHA;
			desc.ddpfPixelFormat.dwAlphaBitDepth = 8;
			desc.dwAlphaBitDepth = 8;
			desc.dwFlags |= NPDISP_DDSD_PIXELFORMAT | NPDISP_DDSD_ALPHABITDEPTH;
		}
		npdisp_ddraw_writeGuest(&desc, data.lpDDSurfaceDesc, sizeof(desc), flatAddress);
	}

	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);

	// For overlay surfaces DirectDraw owns the heap allocation. Returning NOTHANDLED after
	// DDHAL_PLEASEALLOC_BLOCKSIZE lets the runtime replace fpVidMem with the allocated address.
	if (overlayRequest) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}
static UINT32 npdisp_func_DD_DestroySurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_DESTROYSURFACEDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) {
		NPDISP_DDALPHA_VIEW alphaView = { 0 };
		if (!npdisp_ddraw_getAlphaSurfaceEx(data.lpDDSurface, &alphaView, flatAddress, false)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		data.ddRVal = 0;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	if (view.apertureOffset != 0 && view.apertureOffset == npdisp.mm_ddOverlayOffset) {
		npdisp_dd_overlayDirty();
		npdisp.mm_ddOverlayVisible = 0;
		npdisp.mm_ddOverlayOffset = 0;
		npdisp.mm_ddOverlayFourCC = 0;
		npdisp.mm_ddOverlayDDFX = 0;
	}
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

	// DirectDraw passes the clipping rectangles with each clipped Blt, so no persistent clip state is required here.
	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUT11(("NPDISP11 DD_SET_CLIPLIST surf=%08x caps=%08x", data.lpDDSurface, view.lcl.ddsCaps.dwCaps));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_AddAttachedSurface(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_ADDATTACHEDSURFACEDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW baseView = { 0 };
	NPDISP_DDSURFACE_VIEW attachedView = { 0 };
	NPDISP_DDALPHA_VIEW baseAlpha = { 0 };
	NPDISP_DDALPHA_VIEW attachedAlpha = { 0 };
	bool baseIsColor;
	bool attachedIsColor;

	if (!lpDataAddr || npdisp.version < 13 ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!data.lpDDSurface || !data.lpSurfAttached || data.lpDDSurface == data.lpSurfAttached) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	baseIsColor = npdisp_ddraw_getSurface(data.lpDDSurface, &baseView, flatAddress);
	attachedIsColor = npdisp_ddraw_getSurface(data.lpSurfAttached, &attachedView, flatAddress);
	if (baseIsColor != attachedIsColor) {
		if (!baseIsColor && !npdisp_ddraw_getAlphaSurfaceEx(data.lpDDSurface, &baseAlpha, flatAddress, false)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if (!attachedIsColor && !npdisp_ddraw_getAlphaSurfaceEx(data.lpSurfAttached, &attachedAlpha, flatAddress, false)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

		const UINT32 colorWidth = baseIsColor ? baseView.width : attachedView.width;
		const UINT32 colorHeight = baseIsColor ? baseView.height : attachedView.height;
		const UINT32 alphaWidth = baseIsColor ? attachedAlpha.width : baseAlpha.width;
		const UINT32 alphaHeight = baseIsColor ? attachedAlpha.height : baseAlpha.height;
		if (colorWidth != alphaWidth || colorHeight != alphaHeight) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

		data.ddRVal = 0;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	if (!baseIsColor) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	const bool baseOverlay = (baseView.lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_OVERLAY) != 0;
	const bool attachedOverlay = (attachedView.lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_OVERLAY) != 0;
	if (baseOverlay || attachedOverlay) {
		if (baseView.primaryObject || attachedView.primaryObject ||
			baseView.width != attachedView.width || baseView.height != attachedView.height ||
			baseView.pitch != attachedView.pitch || baseView.fourCC != attachedView.fourCC) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}
	else if (baseView.width != attachedView.width || baseView.height != attachedView.height ||
		baseView.pitch != attachedView.pitch || baseView.width != npdisp.width ||
		baseView.height != npdisp.height || baseView.pitch != (SINT32)npdispwin.stride) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUT11(("NPDISP11 DD_ADD_ATTACHED base=%08x/%08x attached=%08x/%08x overlay=%u",
		data.lpDDSurface, baseView.gbl.fpVidMem, data.lpSurfAttached, attachedView.gbl.fpVidMem,
		(baseOverlay || attachedOverlay) ? 1U : 0U));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_Lock(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_LOCKDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };
	NPDISP_DDALPHA_VIEW alphaView = { 0 };
	NPDISP_DDRECTL r;
	UINT32 bytesPerPixel;
	bool alphaSurface = false;

	if (!lpDataAddr || !npdisp.mm_vramLinearAddr ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) {
		if (!npdisp_ddraw_getAlphaSurfaceEx(data.lpDDSurface, &alphaView, flatAddress, false)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		alphaSurface = true;
		bytesPerPixel = 1;
	}
	else bytesPerPixel = (view.fourCC == NPDISP_DD_FOURCC_YUY2) ? 2U : npdisp_ddraw_bytesPerPixel();
	if (!bytesPerPixel) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	const UINT32 width = alphaSurface ? alphaView.width : view.width;
	const UINT32 height = alphaSurface ? alphaView.height : view.height;
	const SINT32 pitch = alphaSurface ? alphaView.pitch : view.pitch;
	const UINT32 linearBase = alphaSurface ? alphaView.linearBase : view.linearBase;
	if (data.bHasRect) {
		r = data.rArea;
		if (!npdisp_ddraw_normalizeSurfaceRect(&r, width, height)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		data.lpSurfData = linearBase + r.top * pitch + r.left * bytesPerPixel;
		if (!alphaSurface && view.visible) npdisp_ddraw_setLockRect(&r);
	}
	else {
		data.lpSurfData = linearBase;
		if (!alphaSurface && view.visible) npdisp_ddraw_setLockRect(NULL);
	}
	data.ddRVal = 0;
	npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
	if (!alphaSurface && view.apertureOffset != 0) {
		TRACEOUT11(("NPDISP11 DD_OFFSCREEN_LOCK fp=%08x data=%08x size=%ux%u pitch=%d",
			view.gbl.fpVidMem, data.lpSurfData, view.width, view.height, view.pitch));
	}
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_Unlock(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_UNLOCKDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW view = { 0 };
	NPDISP_DDALPHA_VIEW alphaView = { 0 };

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDSurface, &view, flatAddress)) {
		if (!npdisp_ddraw_getAlphaSurfaceEx(data.lpDDSurface, &alphaView, flatAddress, false)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		data.ddRVal = 0;
		npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress);
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	if (view.apertureOffset != 0 && view.apertureOffset == npdisp.mm_ddOverlayOffset && npdisp.mm_ddOverlayVisible) npdisp_dd_overlayDirty();
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

static bool npdisp_ddraw_getOverlayColorKey(UINT32 lpSurfaceAddr, bool flatAddress, bool source, NPDISP_DDCOLORKEY* colorKey)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS lcl = { 0 };

	if (!lpSurfaceAddr || !colorKey ||
		!npdisp_ddraw_readGuest(&lcl, lpSurfaceAddr, sizeof(lcl), flatAddress)) {
		return false;
	}
	*colorKey = source ? lcl.ddckCKSrcOverlay : lcl.ddckCKDestOverlay;
	return true;
}

static UINT32 npdisp_func_DD_SetColorKey(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_SETCOLORKEYDATA data = { 0 };
	NPDISP_DDCOLORKEY colorKey;
	UINT32 keyOffset;
	const UINT32 keyFlags = NPDISP_DDCKEY_SRCBLT | NPDISP_DDCKEY_DESTBLT |
		NPDISP_DDCKEY_SRCOVERLAY | NPDISP_DDCKEY_DESTOVERLAY;
	const UINT32 allowedFlags = keyFlags | NPDISP_DDCKEY_COLORSPACE;
	UINT32 selected;

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!data.lpDDSurface || (data.dwFlags & ~allowedFlags)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	selected = data.dwFlags & keyFlags;
	if (!selected || (selected & (selected - 1))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	colorKey = data.ckNew;
	if (!(data.dwFlags & NPDISP_DDCKEY_COLORSPACE)) colorKey.dwColorSpaceHighValue = colorKey.dwColorSpaceLowValue;
	if (colorKey.dwColorSpaceLowValue > colorKey.dwColorSpaceHighValue) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	if (selected == NPDISP_DDCKEY_SRCOVERLAY) {
		keyOffset = (UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS, ddckCKSrcOverlay);
	}
	else if (selected == NPDISP_DDCKEY_DESTOVERLAY) {
		keyOffset = (UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS, ddckCKDestOverlay);
	}
	else if (selected == NPDISP_DDCKEY_SRCBLT) {
		keyOffset = (UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS, ddckCKSrcBlt);
	}
	else {
		keyOffset = (UINT32)offsetof(NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS, ddckCKDestBlt);
	}
	if (!npdisp_ddraw_writeGuest(&colorKey, data.lpDDSurface + keyOffset, sizeof(colorKey), flatAddress)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUT11(("NPDISP11 DD_SET_COLORKEY surf=%08x flags=%08x key=%08x-%08x",
		data.lpDDSurface, data.dwFlags, colorKey.dwColorSpaceLowValue, colorKey.dwColorSpaceHighValue));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static void npdisp_dd_overlayDirty(void)
{
	if (npdisp.mm_ddOverlayDstLeft < npdisp.mm_ddOverlayDstRight &&
		npdisp.mm_ddOverlayDstTop < npdisp.mm_ddOverlayDstBottom) {
		npdisp_setDirty(npdisp.mm_ddOverlayDstLeft, npdisp.mm_ddOverlayDstTop,
			npdisp.mm_ddOverlayDstRight, npdisp.mm_ddOverlayDstBottom);
		npdisp.updated = 1;
	}
}

static UINT32 npdisp_func_DD_UpdateOverlay(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_UPDATEOVERLAYDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW srcView = { 0 };
	NPDISP_DDSURFACE_VIEW dstView = { 0 };
	NPDISP_DDCOLORKEY destKey = { 0 };
	NPDISP_DDCOLORKEY srcKey = { 0 };
	const UINT32 supportedDDFX = NPDISP_DDOVERFX_MIRRORLEFTRIGHT | NPDISP_DDOVERFX_MIRRORUPDOWN;
	const UINT32 supportedFlags = NPDISP_DDOVER_SHOW | NPDISP_DDOVER_HIDE | NPDISP_DDOVER_DDFX |
		NPDISP_DDOVER_KEYDEST | NPDISP_DDOVER_KEYDESTOVERRIDE |
		NPDISP_DDOVER_KEYSRC | NPDISP_DDOVER_KEYSRCOVERRIDE;

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!data.lpDDSrcSurface || !npdisp_ddraw_getSurface(data.lpDDSrcSurface, &srcView, flatAddress) ||
		!(srcView.lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_OVERLAY) || srcView.systemMemory || !srcView.apertureOffset) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if ((data.dwFlags & ~supportedFlags) || ((data.dwFlags & NPDISP_DDOVER_SHOW) && (data.dwFlags & NPDISP_DDOVER_HIDE)) ||
		((data.dwFlags & NPDISP_DDOVER_DDFX) && (data.overlayFX.dwDDFX & ~supportedDDFX)) ||
		(srcView.fourCC == NPDISP_DD_FOURCC_YUY2 && (data.dwFlags & (NPDISP_DDOVER_KEYSRC | NPDISP_DDOVER_KEYSRCOVERRIDE)))) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	if (data.dwFlags & NPDISP_DDOVER_HIDE) {
		npdisp_dd_overlayDirty();
		npdisp.mm_ddOverlayVisible = 0;
		npdisp.mm_ddOverlayOffset = srcView.apertureOffset;
		data.ddRVal = 0;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	if (!data.lpDDDestSurface || !npdisp_ddraw_getSurface(data.lpDDDestSurface, &dstView, flatAddress) ||
		dstView.width != npdisp.width || dstView.height != npdisp.height ||
		data.rSrc.left < 0 || data.rSrc.top < 0 || data.rSrc.right <= data.rSrc.left || data.rSrc.bottom <= data.rSrc.top ||
		data.rSrc.right > (SINT32)srcView.width || data.rSrc.bottom > (SINT32)srcView.height ||
		data.rDest.right <= data.rDest.left || data.rDest.bottom <= data.rDest.top) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}

	if ((data.dwFlags & NPDISP_DDOVER_KEYDEST) &&
		!npdisp_ddraw_getOverlayColorKey(data.lpDDDestSurface, flatAddress, false, &destKey)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.dwFlags & NPDISP_DDOVER_KEYDESTOVERRIDE) destKey = data.overlayFX.dckDestColorkey;
	if ((data.dwFlags & NPDISP_DDOVER_KEYSRC) &&
		!npdisp_ddraw_getOverlayColorKey(data.lpDDSrcSurface, flatAddress, true, &srcKey)) {
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.dwFlags & NPDISP_DDOVER_KEYSRCOVERRIDE) srcKey = data.overlayFX.dckSrcColorkey;

	npdisp_dd_overlayDirty();
	npdisp.mm_ddOverlayOffset = srcView.apertureOffset;
	npdisp.mm_ddOverlayWidth = srcView.width;
	npdisp.mm_ddOverlayHeight = srcView.height;
	npdisp.mm_ddOverlayPitch = srcView.pitch;
	npdisp.mm_ddOverlaySrcLeft = data.rSrc.left;
	npdisp.mm_ddOverlaySrcTop = data.rSrc.top;
	npdisp.mm_ddOverlaySrcRight = data.rSrc.right;
	npdisp.mm_ddOverlaySrcBottom = data.rSrc.bottom;
	npdisp.mm_ddOverlayDstLeft = data.rDest.left;
	npdisp.mm_ddOverlayDstTop = data.rDest.top;
	npdisp.mm_ddOverlayDstRight = data.rDest.right;
	npdisp.mm_ddOverlayDstBottom = data.rDest.bottom;
	npdisp.mm_ddOverlayFourCC = srcView.fourCC;
	npdisp.mm_ddOverlayFlags = data.dwFlags & (NPDISP_DDOVER_KEYDEST | NPDISP_DDOVER_KEYDESTOVERRIDE |
		NPDISP_DDOVER_KEYSRC | NPDISP_DDOVER_KEYSRCOVERRIDE);
	npdisp.mm_ddOverlayDDFX = (data.dwFlags & NPDISP_DDOVER_DDFX) ? data.overlayFX.dwDDFX : 0;
	npdisp.mm_ddOverlayDestKeyLow = destKey.dwColorSpaceLowValue;
	npdisp.mm_ddOverlayDestKeyHigh = destKey.dwColorSpaceHighValue;
	npdisp.mm_ddOverlaySrcKeyLow = srcKey.dwColorSpaceLowValue;
	npdisp.mm_ddOverlaySrcKeyHigh = srcKey.dwColorSpaceHighValue;
	if (data.dwFlags & NPDISP_DDOVER_SHOW) npdisp.mm_ddOverlayVisible = 1;
	npdisp_dd_overlayDirty();

	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUT11(("NPDISP11 DD_OVERLAY_UPDATE src=%08x off=%08x src=%d,%d-%d,%d dst=%d,%d-%d,%d flags=%08x ddfx=%08x visible=%u",
		data.lpDDSrcSurface, srcView.apertureOffset, data.rSrc.left, data.rSrc.top, data.rSrc.right, data.rSrc.bottom,
		data.rDest.left, data.rDest.top, data.rDest.right, data.rDest.bottom, data.dwFlags, npdisp.mm_ddOverlayDDFX, npdisp.mm_ddOverlayVisible));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_func_DD_SetOverlayPosition(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_SETOVERLAYPOSITIONDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW srcView = { 0 };
	SINT32 width;
	SINT32 height;

	if (!lpDataAddr || !npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress) ||
		!data.lpDDSrcSurface || !npdisp_ddraw_getSurface(data.lpDDSrcSurface, &srcView, flatAddress) ||
		srcView.apertureOffset != npdisp.mm_ddOverlayOffset) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	width = npdisp.mm_ddOverlayDstRight - npdisp.mm_ddOverlayDstLeft;
	height = npdisp.mm_ddOverlayDstBottom - npdisp.mm_ddOverlayDstTop;
	if (width <= 0 || height <= 0) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	npdisp_dd_overlayDirty();
	npdisp.mm_ddOverlayDstLeft = data.lXPos;
	npdisp.mm_ddOverlayDstTop = data.lYPos;
	npdisp.mm_ddOverlayDstRight = data.lXPos + width;
	npdisp.mm_ddOverlayDstBottom = data.lYPos + height;
	npdisp_dd_overlayDirty();

	data.ddRVal = 0;
	if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
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

static bool npdisp_ddraw_snapshotRect(const NPDISP_DDSURFACE_VIEW* view, const NPDISP_DDRECTL* rect, UINT32 bytesPerPixel, UINT8** storage, size_t* pitch)
{
	const SINT32 width = rect ? rect->right - rect->left : 0;
	const SINT32 height = rect ? rect->bottom - rect->top : 0;
	UINT64 rowBytes;
	UINT64 totalBytes;
	UINT8* buffer;

	if (!view || !rect || !storage || !pitch || !view->hostBase || !bytesPerPixel || bytesPerPixel > 4 ||
		width <= 0 || height <= 0 || rect->left < view->hostOriginX || rect->top < view->hostOriginY ||
		rect->right > (SINT32)view->width || rect->bottom > (SINT32)view->height) return false;
	rowBytes = (UINT64)(UINT32)width * bytesPerPixel;
	totalBytes = rowBytes * (UINT32)height;
	if (!rowBytes || rowBytes > 0x7fffffffUL || !totalBytes || totalBytes > 0x7fffffffUL) return false;
	*pitch = (size_t)rowBytes;
	buffer = (UINT8*)malloc((size_t)totalBytes);
	if (!buffer) return false;
	for (SINT32 y = 0; y < height; ++y) {
		const UINT8* q = npdisp_ddraw_surfacePtr(view, rect->left, rect->top + y, bytesPerPixel);
		memcpy(buffer + (size_t)y * (*pitch), q, *pitch);
	}
	*storage = buffer;
	return true;
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

static void npdisp_ddraw_copyRowSrcColorKey(UINT8* dst, const UINT8* src, SINT32 width, UINT32 bytesPerPixel, const NPDISP_DDCOLORKEY* colorKey)
{
	const UINT32 low = colorKey->dwColorSpaceLowValue;
	const UINT32 high = colorKey->dwColorSpaceHighValue;

	switch (bytesPerPixel) {
	case 1:
		for (SINT32 x = 0; x < width; ++x) {
			const UINT32 pixel = src[x];
			if (pixel < low || pixel > high) dst[x] = src[x];
		}
		break;
	case 2:
		for (SINT32 x = 0; x < width; ++x) {
			const UINT8* s = src + (size_t)x * 2;
			UINT8* d = dst + (size_t)x * 2;
			const UINT32 pixel = (UINT32)s[0] | ((UINT32)s[1] << 8);
			if (pixel < low || pixel > high) { d[0] = s[0]; d[1] = s[1]; }
		}
		break;
	case 3:
		for (SINT32 x = 0; x < width; ++x) {
			const UINT8* s = src + (size_t)x * 3;
			UINT8* d = dst + (size_t)x * 3;
			const UINT32 pixel = (UINT32)s[0] | ((UINT32)s[1] << 8) | ((UINT32)s[2] << 16);
			if (pixel < low || pixel > high) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
		}
		break;
	case 4:
		for (SINT32 x = 0; x < width; ++x) {
			const UINT8* s = src + (size_t)x * 4;
			UINT8* d = dst + (size_t)x * 4;
			const UINT32 pixel = (UINT32)s[0] | ((UINT32)s[1] << 8) | ((UINT32)s[2] << 16) | ((UINT32)s[3] << 24);
			if (pixel < low || pixel > high) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; }
		}
		break;
	}
}

// 等倍SRCCOPYのcolor key処理。source keyだけの一般的な経路はROP演算と座標変換を省く。
static bool npdisp_ddraw_copyRectColorKey(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst, const NPDISP_DDSURFACE_VIEW* srcView, const NPDISP_DDRECTL* src, UINT32 bytesPerPixel, const NPDISP_DDCOLORKEY* srcColorKey, const NPDISP_DDCOLORKEY* dstColorKey)
{
	const SINT32 width = src ? src->right - src->left : 0;
	const SINT32 height = src ? src->bottom - src->top : 0;
	bool overlap;
	UINT8* snapshot = NULL;
	size_t snapshotPitch = 0;

	if (!dstView || !dst || !srcView || !src || !bytesPerPixel || bytesPerPixel > 4 || width <= 0 || height <= 0 ||
		dst->right - dst->left != width || dst->bottom - dst->top != height ||
		(!srcColorKey && !dstColorKey) ||
		(srcColorKey && srcColorKey->dwColorSpaceLowValue > srcColorKey->dwColorSpaceHighValue) ||
		(dstColorKey && dstColorKey->dwColorSpaceLowValue > dstColorKey->dwColorSpaceHighValue)) return false;
	overlap = dstView->hostBase == srcView->hostBase && npdisp_ddraw_rectsOverlap(dst, src);
	if (overlap && !npdisp_ddraw_snapshotRect(srcView, src, bytesPerPixel, &snapshot, &snapshotPitch)) return false;

	for (SINT32 y = 0; y < height; ++y) {
		UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
		const UINT8* q = snapshot ? snapshot + (size_t)y * snapshotPitch :
			npdisp_ddraw_surfacePtr(srcView, src->left, src->top + y, bytesPerPixel);

		if (srcColorKey && !dstColorKey) {
			npdisp_ddraw_copyRowSrcColorKey(d, q, width, bytesPerPixel, srcColorKey);
		}
		else {
			for (SINT32 x = 0; x < width; ++x) {
				const UINT8* sp = q + (size_t)x * bytesPerPixel;
				UINT8* dp = d + (size_t)x * bytesPerPixel;
				if (srcColorKey && npdisp_ddraw_colorKeyMatches(npdisp_ddraw_readPixelValue(sp, bytesPerPixel), srcColorKey)) continue;
				if (dstColorKey && !npdisp_ddraw_colorKeyMatches(npdisp_ddraw_readPixelValue(dp, bytesPerPixel), dstColorKey)) continue;
				memcpy(dp, sp, bytesPerPixel);
			}
		}
	}

	if (snapshot) free(snapshot);
	return true;
}

// SRCCOPY の nearest-neighbor StretchBlt。回転・mirror・汎用ROP演算を通さず、等倍軸と連続するsource行をまとめて処理する。
static bool npdisp_ddraw_stretchRectSrccopy(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst, const NPDISP_DDRECTL* mapDst, const NPDISP_DDSURFACE_VIEW* srcView, const NPDISP_DDRECTL* src, UINT32 bytesPerPixel, const NPDISP_DDCOLORKEY* srcColorKey, const NPDISP_DDCOLORKEY* dstColorKey)
{
	const SINT32 dstWidth = dst ? dst->right - dst->left : 0;
	const SINT32 dstHeight = dst ? dst->bottom - dst->top : 0;
	const SINT32 mapDstWidth = mapDst ? mapDst->right - mapDst->left : 0;
	const SINT32 mapDstHeight = mapDst ? mapDst->bottom - mapDst->top : 0;
	const SINT32 srcWidth = src ? src->right - src->left : 0;
	const SINT32 srcHeight = src ? src->bottom - src->top : 0;
	const size_t rowBytes = (size_t)dstWidth * bytesPerPixel;
	const bool sameXScale = srcWidth == mapDstWidth;
	UINT8* snapshot = NULL;
	size_t snapshotPitch = 0;
	SINT32 mappedX;
	SINT32 mappedXStep;
	SINT32 mappedXRemainStep;
	SINT32 mappedXRemain;
	SINT32 mappedY;
	SINT32 mappedYStep;
	SINT32 mappedYRemainStep;
	SINT32 mappedYRemain;
	SINT32 previousSourceY = -1;
	UINT8* previousDestRow = NULL;
	UINT64 numerator;

	if (!dstView || !dst || !mapDst || !srcView || !src || !bytesPerPixel || bytesPerPixel > 4 ||
		dstWidth <= 0 || dstHeight <= 0 || mapDstWidth <= 0 || mapDstHeight <= 0 || srcWidth <= 0 || srcHeight <= 0 ||
		dst->left < mapDst->left || dst->top < mapDst->top || dst->right > mapDst->right || dst->bottom > mapDst->bottom ||
		(srcColorKey && srcColorKey->dwColorSpaceLowValue > srcColorKey->dwColorSpaceHighValue) ||
		(dstColorKey && dstColorKey->dwColorSpaceLowValue > dstColorKey->dwColorSpaceHighValue)) return false;

	// 同一surfaceでsource/destinationが重なる場合は、Stretch中にsourceを書き潰さないよう元矩形を固定する。
	if (dstView->hostBase == srcView->hostBase && npdisp_ddraw_rectsOverlap(dst, src) &&
		!npdisp_ddraw_snapshotRect(srcView, src, bytesPerPixel, &snapshot, &snapshotPitch)) return false;

	numerator = (UINT64)(UINT32)(dst->left - mapDst->left) * (UINT32)srcWidth;
	mappedX = (SINT32)(numerator / (UINT32)mapDstWidth);
	mappedXRemain = (SINT32)(numerator % (UINT32)mapDstWidth);
	mappedXStep = srcWidth / mapDstWidth;
	mappedXRemainStep = srcWidth % mapDstWidth;
	numerator = (UINT64)(UINT32)(dst->top - mapDst->top) * (UINT32)srcHeight;
	mappedY = (SINT32)(numerator / (UINT32)mapDstHeight);
	mappedYRemain = (SINT32)(numerator % (UINT32)mapDstHeight);
	mappedYStep = srcHeight / mapDstHeight;
	mappedYRemainStep = srcHeight % mapDstHeight;

	for (SINT32 y = 0; y < dstHeight; ++y) {
		UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
		const UINT8* q = snapshot ? snapshot + (size_t)mappedY * snapshotPitch + (size_t)mappedX * bytesPerPixel :
			npdisp_ddraw_surfacePtr(srcView, src->left + mappedX, src->top + mappedY, bytesPerPixel);

		// key無しでは、同じsource行を再利用する縦拡大を前行copyだけで済ませる。
		if (!srcColorKey && !dstColorKey && previousDestRow && mappedY == previousSourceY) {
			memcpy(d, previousDestRow, rowBytes);
		}
		else if (!srcColorKey && !dstColorKey && sameXScale) {
			memcpy(d, q, rowBytes);
		}
		else {
			SINT32 mx = mappedX;
			SINT32 mxRemain = mappedXRemain;
			const UINT32 srcLow = srcColorKey ? srcColorKey->dwColorSpaceLowValue : 0;
			const UINT32 srcHigh = srcColorKey ? srcColorKey->dwColorSpaceHighValue : 0;
			const UINT32 dstLow = dstColorKey ? dstColorKey->dwColorSpaceLowValue : 0;
			const UINT32 dstHigh = dstColorKey ? dstColorKey->dwColorSpaceHighValue : 0;

			switch (bytesPerPixel) {
			case 1:
				for (SINT32 x = 0; x < dstWidth; ++x) {
					const UINT32 sv = q[mx - mappedX];
					if ((!srcColorKey || sv < srcLow || sv > srcHigh) &&
						(!dstColorKey || ((UINT32)d[x] >= dstLow && (UINT32)d[x] <= dstHigh))) d[x] = (UINT8)sv;
					mx += mappedXStep;
					mxRemain += mappedXRemainStep;
					if (mxRemain >= mapDstWidth) { mxRemain -= mapDstWidth; ++mx; }
				}
				break;
			case 2:
				for (SINT32 x = 0; x < dstWidth; ++x) {
					const UINT8* sp = q + (size_t)(mx - mappedX) * 2;
					UINT8* dp = d + (size_t)x * 2;
					const UINT32 sv = (UINT32)sp[0] | ((UINT32)sp[1] << 8);
					const UINT32 dv = dstColorKey ? ((UINT32)dp[0] | ((UINT32)dp[1] << 8)) : 0;
					if ((!srcColorKey || sv < srcLow || sv > srcHigh) && (!dstColorKey || (dv >= dstLow && dv <= dstHigh))) {
						dp[0] = sp[0]; dp[1] = sp[1];
					}
					mx += mappedXStep;
					mxRemain += mappedXRemainStep;
					if (mxRemain >= mapDstWidth) { mxRemain -= mapDstWidth; ++mx; }
				}
				break;
			case 3:
				for (SINT32 x = 0; x < dstWidth; ++x) {
					const UINT8* sp = q + (size_t)(mx - mappedX) * 3;
					UINT8* dp = d + (size_t)x * 3;
					const UINT32 sv = (UINT32)sp[0] | ((UINT32)sp[1] << 8) | ((UINT32)sp[2] << 16);
					const UINT32 dv = dstColorKey ? ((UINT32)dp[0] | ((UINT32)dp[1] << 8) | ((UINT32)dp[2] << 16)) : 0;
					if ((!srcColorKey || sv < srcLow || sv > srcHigh) && (!dstColorKey || (dv >= dstLow && dv <= dstHigh))) {
						dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
					}
					mx += mappedXStep;
					mxRemain += mappedXRemainStep;
					if (mxRemain >= mapDstWidth) { mxRemain -= mapDstWidth; ++mx; }
				}
				break;
			case 4:
				for (SINT32 x = 0; x < dstWidth; ++x) {
					const UINT8* sp = q + (size_t)(mx - mappedX) * 4;
					UINT8* dp = d + (size_t)x * 4;
					const UINT32 sv = (UINT32)sp[0] | ((UINT32)sp[1] << 8) | ((UINT32)sp[2] << 16) | ((UINT32)sp[3] << 24);
					const UINT32 dv = dstColorKey ? ((UINT32)dp[0] | ((UINT32)dp[1] << 8) | ((UINT32)dp[2] << 16) | ((UINT32)dp[3] << 24)) : 0;
					if ((!srcColorKey || sv < srcLow || sv > srcHigh) && (!dstColorKey || (dv >= dstLow && dv <= dstHigh))) {
						dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
					}
					mx += mappedXStep;
					mxRemain += mappedXRemainStep;
					if (mxRemain >= mapDstWidth) { mxRemain -= mapDstWidth; ++mx; }
				}
				break;
			}
		}

		previousSourceY = mappedY;
		previousDestRow = d;
		mappedY += mappedYStep;
		mappedYRemain += mappedYRemainStep;
		if (mappedYRemain >= mapDstHeight) { mappedYRemain -= mapDstHeight; ++mappedY; }
	}

	if (snapshot) free(snapshot);
	return true;
}

static UINT8 npdisp_ddraw_getRop3(UINT32 rop)
{
	return (UINT8)((rop >> 16) & 0xff);
}

static bool npdisp_ddraw_ropUsesPattern(UINT8 rop3)
{
	return (rop3 & 0x0f) != ((rop3 >> 4) & 0x0f);
}

static bool npdisp_ddraw_ropUsesSource(UINT8 rop3)
{
	return (rop3 & 0x33) != ((rop3 >> 2) & 0x33);
}

static bool npdisp_ddraw_ropUsesDest(UINT8 rop3)
{
	return (rop3 & 0x55) != ((rop3 >> 1) & 0x55);
}

static UINT32 npdisp_ddraw_applyRop(UINT8 rop3, UINT32 pattern, UINT32 source, UINT32 dest, UINT32 pixelMask)
{
	const UINT32 np = (~pattern) & pixelMask;
	const UINT32 ns = (~source) & pixelMask;
	const UINT32 nd = (~dest) & pixelMask;
	UINT32 result = 0;

	// ROP3 bit index is (P << 2) | (S << 1) | D.
	if (rop3 & 0x01) result |= np & ns & nd;
	if (rop3 & 0x02) result |= np & ns & dest;
	if (rop3 & 0x04) result |= np & source & nd;
	if (rop3 & 0x08) result |= np & source & dest;
	if (rop3 & 0x10) result |= pattern & ns & nd;
	if (rop3 & 0x20) result |= pattern & ns & dest;
	if (rop3 & 0x40) result |= pattern & source & nd;
	if (rop3 & 0x80) result |= pattern & source & dest;
	return result & pixelMask;
}

static UINT32 npdisp_ddraw_blendComponent(UINT32 source, UINT32 dest, UINT32 sourceWeight, UINT32 destWeight, UINT32 maxValue)
{
	UINT32 value = (source * sourceWeight + dest * destWeight + 127U) / 255U;
	return (value > maxValue) ? maxValue : value;
}

static UINT32 npdisp_ddraw_maskShift(UINT32 mask)
{
	UINT32 shift = 0;
	if (!mask) return 0;
	while (!(mask & 1U)) { mask >>= 1; ++shift; }
	return shift;
}

static UINT32 npdisp_ddraw_blendPixel(UINT32 source, UINT32 dest, UINT8 sourceAlpha, UINT8 destAlpha, bool haveSourceAlpha, bool haveDestAlpha)
{
	UINT32 sourceWeight;
	UINT32 destWeight;
	UINT32 rMask;
	UINT32 gMask;
	UINT32 bMask;
	UINT32 rgbMask;
	UINT32 result;

	if (npdisp.bpp == 15) {
		rMask = 0x00007c00UL; gMask = 0x000003e0UL; bMask = 0x0000001fUL;
	}
	else if (npdisp.bpp == 16) {
		rMask = 0x0000f800UL; gMask = 0x000007e0UL; bMask = 0x0000001fUL;
	}
	else if (npdisp.bpp == 24 || npdisp.bpp == 32) {
		rMask = 0x00ff0000UL; gMask = 0x0000ff00UL; bMask = 0x000000ffUL;
	}
	else return dest;

	if (haveSourceAlpha && haveDestAlpha) {
		sourceWeight = sourceAlpha;
		destWeight = destAlpha;
	}
	else if (haveSourceAlpha) {
		sourceWeight = sourceAlpha;
		destWeight = 255U - sourceWeight;
	}
	else {
		destWeight = destAlpha;
		sourceWeight = 255U - destWeight;
	}

	rgbMask = rMask | gMask | bMask;
	result = dest & ~rgbMask;
	const UINT32 masks[3] = { rMask, gMask, bMask };
	for (UINT32 i = 0; i < 3; ++i) {
		const UINT32 mask = masks[i];
		const UINT32 shift = npdisp_ddraw_maskShift(mask);
		const UINT32 maxValue = mask >> shift;
		const UINT32 s = (source & mask) >> shift;
		const UINT32 d = (dest & mask) >> shift;
		result |= npdisp_ddraw_blendComponent(s, d, sourceWeight, destWeight, maxValue) << shift;
	}
	return result;
}

static bool npdisp_ddraw_getAlphaChannelValue(const NPDISP_DDALPHA_CHANNEL* channel, SINT32 x, SINT32 y, bool flatAddress, UINT8* alpha)
{
	UINT8 value;
	if (!channel || !channel->enabled || !alpha) return false;
	if (channel->constant) value = channel->constantValue;
	else if (!npdisp_ddraw_readAlphaValue(&channel->surface, x, y, flatAddress, &value)) return false;
	if (channel->negate) value = (UINT8)(255U - value);
	*alpha = value;
	return true;
}

static bool npdisp_ddraw_bltAlphaPixels(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst, const NPDISP_DDRECTL* mapDst,
	const NPDISP_DDSURFACE_VIEW* srcView, const NPDISP_DDRECTL* src, const NPDISP_DDRECTL* alphaSrcRect, UINT32 bytesPerPixel,
	const NPDISP_DDCOLORKEY* srcColorKey, const NPDISP_DDCOLORKEY* dstColorKey, UINT32 rotation, bool mirrorX, bool mirrorY,
	const NPDISP_DDALPHA_CHANNEL* srcAlpha, const NPDISP_DDALPHA_CHANNEL* dstAlpha, bool flatAddress)
{
	const SINT32 dstWidth = dst->right - dst->left;
	const SINT32 dstHeight = dst->bottom - dst->top;
	const SINT32 mapDstWidth = mapDst->right - mapDst->left;
	const SINT32 mapDstHeight = mapDst->bottom - mapDst->top;
	const SINT32 srcWidth = src->right - src->left;
	const SINT32 srcHeight = src->bottom - src->top;
	const SINT32 rotatedWidth = (rotation == 90 || rotation == 270) ? srcHeight : srcWidth;
	const SINT32 rotatedHeight = (rotation == 90 || rotation == 270) ? srcWidth : srcHeight;
	UINT8* snapshot = NULL;
	size_t snapshotPitch = 0;
	SINT32 mappedX;
	SINT32 mappedXStep;
	SINT32 mappedXRemainStep;
	SINT32 mappedXRemain;
	SINT32 mappedY;
	SINT32 mappedYStep;
	SINT32 mappedYRemainStep;
	SINT32 mappedYRemain;
	UINT64 numerator;
	SINT32 mapX;
	SINT32 mapY;

	if (!dstView || !dst || !mapDst || !srcView || !src || !alphaSrcRect || !bytesPerPixel || bytesPerPixel > 4 ||
		!srcAlpha || !dstAlpha || (!srcAlpha->enabled && !dstAlpha->enabled) ||
		dstWidth <= 0 || dstHeight <= 0 || mapDstWidth <= 0 || mapDstHeight <= 0 || srcWidth <= 0 || srcHeight <= 0 ||
		(rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)) return false;
	if ((srcColorKey && srcColorKey->dwColorSpaceLowValue > srcColorKey->dwColorSpaceHighValue) ||
		(dstColorKey && dstColorKey->dwColorSpaceLowValue > dstColorKey->dwColorSpaceHighValue)) return false;

	mapX = dst->left - mapDst->left;
	mapY = dst->top - mapDst->top;
	if (mapX < 0 || mapY < 0) return false;
	numerator = (UINT64)(UINT32)mapX * (UINT32)rotatedWidth;
	mappedX = (SINT32)(numerator / (UINT32)mapDstWidth);
	mappedXRemain = (SINT32)(numerator % (UINT32)mapDstWidth);
	mappedXStep = rotatedWidth / mapDstWidth;
	mappedXRemainStep = rotatedWidth % mapDstWidth;
	numerator = (UINT64)(UINT32)mapY * (UINT32)rotatedHeight;
	mappedY = (SINT32)(numerator / (UINT32)mapDstHeight);
	mappedYRemain = (SINT32)(numerator % (UINT32)mapDstHeight);
	mappedYStep = rotatedHeight / mapDstHeight;
	mappedYRemainStep = rotatedHeight % mapDstHeight;

	if (dstView->hostBase == srcView->hostBase && npdisp_ddraw_rectsOverlap(dst, src) &&
		!npdisp_ddraw_snapshotRect(srcView, src, bytesPerPixel, &snapshot, &snapshotPitch)) return false;

	for (SINT32 y = 0; y < dstHeight; ++y) {
		UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
		SINT32 mx = mappedX;
		SINT32 mxRemain = mappedXRemain;
		for (SINT32 x = 0; x < dstWidth; ++x) {
			SINT32 sampleX;
			SINT32 sampleY;
			const UINT8* q;
			UINT32 sourcePixel;
			UINT32 destPixel;
			UINT8 sourceAlpha = 255;
			UINT8 destAlpha = 255;

			npdisp_ddraw_transformBltSample(&sampleX, &sampleY, mx, mappedY, srcWidth, srcHeight, rotation, mirrorX, mirrorY);
			if (snapshot) q = snapshot + (size_t)sampleY * snapshotPitch + (size_t)sampleX * bytesPerPixel;
			else q = npdisp_ddraw_surfacePtr(srcView, src->left + sampleX, src->top + sampleY, bytesPerPixel);
			sourcePixel = npdisp_ddraw_readPixelValue(q, bytesPerPixel);
			if (!npdisp_ddraw_colorKeyMatches(sourcePixel, srcColorKey)) {
				destPixel = npdisp_ddraw_readPixelValue(d, bytesPerPixel);
				if (!dstColorKey || npdisp_ddraw_colorKeyMatches(destPixel, dstColorKey)) {
					if (srcAlpha->enabled && !npdisp_ddraw_getAlphaChannelValue(srcAlpha, alphaSrcRect->left + sampleX, alphaSrcRect->top + sampleY, flatAddress, &sourceAlpha)) {
						if (snapshot) free(snapshot); return false;
					}
					if (dstAlpha->enabled && !npdisp_ddraw_getAlphaChannelValue(dstAlpha, dst->left + x, dst->top + y, flatAddress, &destAlpha)) {
						if (snapshot) free(snapshot); return false;
					}
					npdisp_ddraw_writePixelValue(d, bytesPerPixel, npdisp_ddraw_blendPixel(sourcePixel, destPixel,
						sourceAlpha, destAlpha, srcAlpha->enabled, dstAlpha->enabled));
				}
			}
			d += bytesPerPixel;
			mx += mappedXStep;
			mxRemain += mappedXRemainStep;
			if (mxRemain >= mapDstWidth) { mxRemain -= mapDstWidth; ++mx; }
		}
		mappedY += mappedYStep;
		mappedYRemain += mappedYRemainStep;
		if (mappedYRemain >= mapDstHeight) { mappedYRemain -= mapDstHeight; ++mappedY; }
	}
	if (snapshot) free(snapshot);
	return true;
}

static bool npdisp_ddraw_bltPixels(const NPDISP_DDSURFACE_VIEW* dstView, const NPDISP_DDRECTL* dst, const NPDISP_DDRECTL* mapDst, const NPDISP_DDSURFACE_VIEW* srcView, const NPDISP_DDRECTL* src, const UINT8* patternPixels, size_t patternPitch, UINT32 bytesPerPixel, UINT8 rop3, const NPDISP_DDCOLORKEY* srcColorKey, const NPDISP_DDCOLORKEY* dstColorKey, UINT32 rotation, bool mirrorX, bool mirrorY)
{
	const SINT32 dstWidth = dst->right - dst->left;
	const SINT32 dstHeight = dst->bottom - dst->top;
	const SINT32 mapDstWidth = mapDst ? mapDst->right - mapDst->left : dstWidth;
	const SINT32 mapDstHeight = mapDst ? mapDst->bottom - mapDst->top : dstHeight;
	const bool patternRequired = npdisp_ddraw_ropUsesPattern(rop3);
	const bool sourceRequired = npdisp_ddraw_ropUsesSource(rop3) || srcColorKey != NULL;
	const bool destRequired = npdisp_ddraw_ropUsesDest(rop3) || dstColorKey != NULL;
	SINT32 srcWidth = dstWidth;
	SINT32 srcHeight = dstHeight;
	SINT32 rotatedWidth = srcWidth;
	SINT32 rotatedHeight = srcHeight;
	bool overlap = false;
	const UINT32 pixelMask = npdisp_ddraw_pixelMask(bytesPerPixel);
	UINT8* snapshot = NULL;
	size_t snapshotPitch = 0;
	SINT32 mappedX = 0;
	SINT32 mappedXStep = 0;
	SINT32 mappedXRemainStep = 0;
	SINT32 mappedXRemain = 0;
	SINT32 mappedY = 0;
	SINT32 mappedYStep = 0;
	SINT32 mappedYRemainStep = 0;
	SINT32 mappedYRemain = 0;

	if (dstWidth <= 0 || dstHeight <= 0 || mapDstWidth <= 0 || mapDstHeight <= 0 || !pixelMask ||
		(rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) ||
		(patternRequired && (!patternPixels || patternPitch < (size_t)NPDISP_DD_PATTERN_SIZE * bytesPerPixel))) return false;
	if (sourceRequired) {
		SINT32 mapX;
		SINT32 mapY;
		UINT64 numerator;

		if (!srcView || !src) return false;
		srcWidth = src->right - src->left;
		srcHeight = src->bottom - src->top;
		if (srcWidth <= 0 || srcHeight <= 0) return false;
		if (rotation == 90 || rotation == 270) {
			rotatedWidth = srcHeight;
			rotatedHeight = srcWidth;
		}
		else {
			rotatedWidth = srcWidth;
			rotatedHeight = srcHeight;
		}
		overlap = dstView->hostBase == srcView->hostBase && npdisp_ddraw_rectsOverlap(dst, src);

		// Clipping changes dst, but mapDst remains the original destination rectangle. Start the incremental mapper at the clipped position.
		mapX = dst->left - (mapDst ? mapDst->left : dst->left);
		mapY = dst->top - (mapDst ? mapDst->top : dst->top);
		if (mapX < 0 || mapY < 0) return false;
		numerator = (UINT64)(UINT32)mapX * (UINT32)rotatedWidth;
		mappedX = (SINT32)(numerator / (UINT32)mapDstWidth);
		mappedXRemain = (SINT32)(numerator % (UINT32)mapDstWidth);
		mappedXStep = rotatedWidth / mapDstWidth;
		mappedXRemainStep = rotatedWidth % mapDstWidth;
		numerator = (UINT64)(UINT32)mapY * (UINT32)rotatedHeight;
		mappedY = (SINT32)(numerator / (UINT32)mapDstHeight);
		mappedYRemain = (SINT32)(numerator % (UINT32)mapDstHeight);
		mappedYStep = rotatedHeight / mapDstHeight;
		mappedYRemainStep = rotatedHeight % mapDstHeight;
	}
	if ((srcColorKey && srcColorKey->dwColorSpaceLowValue > srcColorKey->dwColorSpaceHighValue) ||
		(dstColorKey && dstColorKey->dwColorSpaceLowValue > dstColorKey->dwColorSpaceHighValue)) return false;

	// When source and destination overlap in one surface, preserve the source rectangle before drawing.
	if (overlap && !npdisp_ddraw_snapshotRect(srcView, src, bytesPerPixel, &snapshot, &snapshotPitch)) return false;

	for (SINT32 y = 0; y < dstHeight; ++y) {
		UINT8* d = npdisp_ddraw_surfacePtr(dstView, dst->left, dst->top + y, bytesPerPixel);
		SINT32 mx = mappedX;
		SINT32 mxRemain = mappedXRemain;

		for (SINT32 x = 0; x < dstWidth; ++x) {
			UINT32 patternPixel = 0;
			UINT32 sourcePixel = 0;
			UINT32 destPixel = 0;

			if (patternRequired) {
				const UINT8* p = patternPixels + (size_t)((dst->top + y) & (NPDISP_DD_PATTERN_SIZE - 1)) * patternPitch +
					(size_t)((dst->left + x) & (NPDISP_DD_PATTERN_SIZE - 1)) * bytesPerPixel;
				patternPixel = npdisp_ddraw_readPixelValue(p, bytesPerPixel);
			}

			if (sourceRequired) {
				const UINT8* q;
				SINT32 sampleX;
				SINT32 sampleY;
				npdisp_ddraw_transformBltSample(&sampleX, &sampleY, mx, mappedY, srcWidth, srcHeight, rotation, mirrorX, mirrorY);
				if (snapshot) q = snapshot + (size_t)sampleY * snapshotPitch + (size_t)sampleX * bytesPerPixel;
				else q = npdisp_ddraw_surfacePtr(srcView, src->left + sampleX, src->top + sampleY, bytesPerPixel);
				sourcePixel = npdisp_ddraw_readPixelValue(q, bytesPerPixel);
				if (npdisp_ddraw_colorKeyMatches(sourcePixel, srcColorKey)) {
					d += bytesPerPixel;
					mx += mappedXStep;
					mxRemain += mappedXRemainStep;
					if (mxRemain >= mapDstWidth) {
						mxRemain -= mapDstWidth;
						++mx;
					}
					continue;
				}
			}

			if (destRequired) destPixel = npdisp_ddraw_readPixelValue(d, bytesPerPixel);
			if (!dstColorKey || npdisp_ddraw_colorKeyMatches(destPixel, dstColorKey)) {
				npdisp_ddraw_writePixelValue(d, bytesPerPixel,
					npdisp_ddraw_applyRop(rop3, patternPixel, sourcePixel, destPixel, pixelMask));
			}
			d += bytesPerPixel;

			if (sourceRequired) {
				mx += mappedXStep;
				mxRemain += mappedXRemainStep;
				if (mxRemain >= mapDstWidth) {
					mxRemain -= mapDstWidth;
					++mx;
				}
			}
		}

		if (sourceRequired) {
			mappedY += mappedYStep;
			mappedYRemain += mappedYRemainStep;
			if (mappedYRemain >= mapDstHeight) {
				mappedYRemain -= mapDstHeight;
				++mappedY;
			}
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

static bool npdisp_ddraw_setupAlphaChannel(const NPDISP_DDHAL_BLTDATA* data, bool source, UINT32 colorSurfaceAddr,
	const NPDISP_DDSURFACE_VIEW* colorView, bool flatAddress, NPDISP_DDALPHA_CHANNEL* channel)
{
	UINT32 attachedFlag;
	UINT32 constFlag;
	UINT32 surfaceFlag;
	UINT32 negFlag;
	UINT32 selected;
	UINT32 bitDepth;
	UINT32 value;

	if (!data || !colorView || !channel) return false;
	memset(channel, 0, sizeof(*channel));
	if (source) {
		attachedFlag = NPDISP_DDBLT_ALPHASRC;
		constFlag = NPDISP_DDBLT_ALPHASRCCONSTOVERRIDE;
		surfaceFlag = NPDISP_DDBLT_ALPHASRCSURFACEOVERRIDE;
		negFlag = NPDISP_DDBLT_ALPHASRCNEG;
		bitDepth = data->bltFX.dwAlphaSrcConstBitDepth;
		value = data->bltFX.dwAlphaSrcConst;
	}
	else {
		attachedFlag = NPDISP_DDBLT_ALPHADEST;
		constFlag = NPDISP_DDBLT_ALPHADESTCONSTOVERRIDE;
		surfaceFlag = NPDISP_DDBLT_ALPHADESTSURFACEOVERRIDE;
		negFlag = NPDISP_DDBLT_ALPHADESTNEG;
		bitDepth = data->bltFX.dwAlphaDestConstBitDepth;
		value = data->bltFX.dwAlphaDestConst;
	}

	selected = data->dwFlags & (attachedFlag | constFlag | surfaceFlag);
	if (!selected) return (data->dwFlags & negFlag) == 0;
	if (selected & (selected - 1)) return false;
	channel->enabled = true;
	channel->negate = (data->dwFlags & negFlag) != 0;

	if (selected == constFlag) {
		channel->constant = true;
		return npdisp_ddraw_normalizeAlphaConst(bitDepth, value, &channel->constantValue);
	}
	if (selected == surfaceFlag) {
		if (!npdisp_ddraw_getAlphaSurfacePointer(value, &channel->surface, flatAddress)) return false;
	}
	else {
		if (!npdisp_ddraw_findAttachedAlphaSurface(colorSurfaceAddr, &channel->surface, flatAddress)) return false;
	}
	return channel->surface.width == colorView->width && channel->surface.height == colorView->height;
}

static UINT32 npdisp_func_DD_Blt(UINT32 lpDataAddr, bool flatAddress)
{
	NPDISP_DDHAL_BLTDATA data = { 0 };
	NPDISP_DDSURFACE_VIEW dstView = { 0 };
	NPDISP_DDSURFACE_VIEW srcView = { 0 };
	NPDISP_DDSURFACE_VIEW srcExecView = { 0 };
	NPDISP_DDSURFACE_VIEW patternView = { 0 };
	NPDISP_DDALPHA_CHANNEL srcAlpha = { 0 };
	NPDISP_DDALPHA_CHANNEL dstAlpha = { 0 };
	NPDISP_DDRECTL mapSrc = { 0 };
	NPDISP_DDRECTL originalMapSrc = { 0 };
	NPDISP_DDRECTL mapDst = { 0 };
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
	UINT8 patternPixels[NPDISP_DD_PATTERN_SIZE * NPDISP_DD_PATTERN_SIZE * 4] = { 0 };
	UINT8 rop3 = 0xcc;
	UINT32 ddFx = 0;
	UINT32 rotation = 0;
	UINT32 rectCount = 1;
	UINT32 execRectCount = 0;
	bool patternRequired = false;
	bool alphaBlend = false;
	bool sourceRequired = false;
	bool destRequired = false;
	bool clipped = false;
	bool stretch = false;
	bool mirrorX = false;
	bool mirrorY = false;
	bool drewAny = false;

	if (!lpDataAddr || !bytesPerPixel ||
		!npdisp_ddraw_readGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_ddraw_getSurface(data.lpDDDestSurface, &dstView, flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

	const UINT32 commonFlags = NPDISP_DDBLT_ASYNC | NPDISP_DDBLT_WAIT;
	if (data.dwFlags & NPDISP_DDBLT_COLORFILL) {
		if (data.dwFlags & ~(commonFlags | NPDISP_DDBLT_COLORFILL)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		sourceRequired = false;
	}
	else {
		const UINT32 alphaFlags = NPDISP_DDBLT_ALPHADEST | NPDISP_DDBLT_ALPHADESTCONSTOVERRIDE |
			NPDISP_DDBLT_ALPHADESTNEG | NPDISP_DDBLT_ALPHADESTSURFACEOVERRIDE |
			NPDISP_DDBLT_ALPHASRC | NPDISP_DDBLT_ALPHASRCCONSTOVERRIDE |
			NPDISP_DDBLT_ALPHASRCNEG | NPDISP_DDBLT_ALPHASRCSURFACEOVERRIDE;
		const UINT32 supportedFlags = commonFlags | NPDISP_DDBLT_DDFX | NPDISP_DDBLT_ROP | alphaFlags |
			NPDISP_DDBLT_KEYDEST | NPDISP_DDBLT_KEYDESTOVERRIDE |
			NPDISP_DDBLT_KEYSRC | NPDISP_DDBLT_KEYSRCOVERRIDE | NPDISP_DDBLT_RUNTIME_PATTERN_ROP;

		alphaBlend = (data.dwFlags & alphaFlags) != 0;
		if (data.dwFlags & NPDISP_DDBLT_ROP) {
			rop3 = npdisp_ddraw_getRop3(data.bltFX.dwROP);
			patternRequired = npdisp_ddraw_ropUsesPattern(rop3);
		}
		if (data.dwFlags & ~supportedFlags) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if ((data.dwFlags & NPDISP_DDBLT_RUNTIME_PATTERN_ROP) &&
			(!(data.dwFlags & NPDISP_DDBLT_ROP) || !patternRequired || !(data.dwROPFlags & NPDISP_DD_ROPFLAG_HAS_PATTERN))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if ((data.dwFlags & NPDISP_DDBLT_KEYSRC) && (data.dwFlags & NPDISP_DDBLT_KEYSRCOVERRIDE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if ((data.dwFlags & NPDISP_DDBLT_KEYDEST) && (data.dwFlags & NPDISP_DDBLT_KEYDESTOVERRIDE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if (alphaBlend && (npdisp.bpp < 15 || patternRequired || ((data.dwFlags & NPDISP_DDBLT_ROP) && rop3 != 0xcc))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

		if (patternRequired) {
			const UINT32 patternSurfaceAddr = data.bltFX.dwFillColor;
			if (!npdisp_ddraw_getPatternSurface(patternSurfaceAddr, &patternView, flatAddress)) {
				TRACEOUT11(("NPDISP11 DD_BLT_PATTERN_SURFACE_FAIL rop=%02x pattern=%08x ropflags=%08x flat=%u",
					rop3, patternSurfaceAddr, data.dwROPFlags, flatAddress ? 1U : 0U));
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			if (!npdisp_ddraw_snapshotPattern(&patternView, bytesPerPixel, flatAddress, patternPixels,
				(size_t)NPDISP_DD_PATTERN_SIZE * bytesPerPixel)) {
				TRACEOUT11(("NPDISP11 DD_BLT_PATTERN_SNAPSHOT_FAIL rop=%02x pattern=%08x caps=%08x fp=%08x size=%ux%u pitch=%d sysmem=%u",
					rop3, patternSurfaceAddr, patternView.lcl.ddsCaps.dwCaps, patternView.gbl.fpVidMem,
					patternView.width, patternView.height, patternView.pitch, patternView.systemMemory ? 1U : 0U));
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
		}
		if (data.dwFlags & NPDISP_DDBLT_DDFX) {
			const UINT32 rotationMask = NPDISP_DDBLTFX_ROTATE90 | NPDISP_DDBLTFX_ROTATE180 | NPDISP_DDBLTFX_ROTATE270;
			const UINT32 supportedDDFX = NPDISP_DDBLTFX_MIRRORLEFTRIGHT | NPDISP_DDBLTFX_MIRRORUPDOWN | rotationMask;
			UINT32 rotationBits;
			ddFx = data.bltFX.dwDDFX;
			if (ddFx & ~supportedDDFX) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			mirrorX = (ddFx & NPDISP_DDBLTFX_MIRRORLEFTRIGHT) != 0;
			mirrorY = (ddFx & NPDISP_DDBLTFX_MIRRORUPDOWN) != 0;
			rotationBits = ddFx & rotationMask;
			switch (rotationBits) {
			case 0: rotation = 0; break;
			case NPDISP_DDBLTFX_ROTATE90: rotation = 90; break;
			case NPDISP_DDBLTFX_ROTATE180: rotation = 180; break;
			case NPDISP_DDBLTFX_ROTATE270: rotation = 270; break;
			default: return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
		}

		if (data.dwFlags & NPDISP_DDBLT_KEYDESTOVERRIDE) {
			dstColorKey = data.bltFX.ddckDestColorkey;
			dstColorKeyPtr = &dstColorKey;
		}
		else if (data.dwFlags & NPDISP_DDBLT_KEYDEST) {
			if (!npdisp_ddraw_getColorKey(data.lpDDDestSurface, flatAddress, false, &dstColorKey)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			dstColorKeyPtr = &dstColorKey;
		}

		sourceRequired = alphaBlend || npdisp_ddraw_ropUsesSource(rop3) ||
			(data.dwFlags & (NPDISP_DDBLT_KEYSRC | NPDISP_DDBLT_KEYSRCOVERRIDE)) != 0;
		if (sourceRequired) {
			if (!data.lpDDSrcSurface || !npdisp_ddraw_getSurface(data.lpDDSrcSurface, &srcView, flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (data.dwFlags & NPDISP_DDBLT_KEYSRCOVERRIDE) {
			srcColorKey = data.bltFX.ddckSrcColorkey;
			srcColorKeyPtr = &srcColorKey;
		}
		else if (data.dwFlags & NPDISP_DDBLT_KEYSRC) {
			if (!npdisp_ddraw_getColorKey(data.lpDDSrcSurface, flatAddress, true, &srcColorKey)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			srcColorKeyPtr = &srcColorKey;
		}

		if (alphaBlend) {
			if (!npdisp_ddraw_setupAlphaChannel(&data, true, data.lpDDSrcSurface, &srcView, flatAddress, &srcAlpha) ||
				!npdisp_ddraw_setupAlphaChannel(&data, false, data.lpDDDestSurface, &dstView, flatAddress, &dstAlpha) ||
				(!srcAlpha.enabled && !dstAlpha.enabled)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}

	if (dstView.fourCC || (sourceRequired && srcView.fourCC)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;

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
	if (sourceRequired) originalMapSrc = mapSrc;

	const SINT32 mapDstWidth = mapDst.right - mapDst.left;
	const SINT32 mapDstHeight = mapDst.bottom - mapDst.top;
	const SINT32 mapSrcWidth = sourceRequired ? mapSrc.right - mapSrc.left : mapDstWidth;
	const SINT32 mapSrcHeight = sourceRequired ? mapSrc.bottom - mapSrc.top : mapDstHeight;
	const SINT32 rotatedSrcWidth = (rotation == 90 || rotation == 270) ? mapSrcHeight : mapSrcWidth;
	const SINT32 rotatedSrcHeight = (rotation == 90 || rotation == 270) ? mapSrcWidth : mapSrcHeight;
	stretch = sourceRequired && (rotatedSrcWidth != mapDstWidth || rotatedSrcHeight != mapDstHeight);
	destRequired = alphaBlend || npdisp_ddraw_ropUsesDest(rop3) || srcColorKeyPtr != NULL || dstColorKeyPtr != NULL;

	// clipなしはmapDstをそのまま使い、通常Bltで一時rect配列を確保しない。
	if (!clipped) {
		execRectCount = 1;
	}
	else if (rectCount) {
		NPDISP_DDRECTL surfaceBounds;
		surfaceBounds.left = 0;
		surfaceBounds.top = 0;
		surfaceBounds.right = (SINT32)dstView.width;
		surfaceBounds.bottom = (SINT32)dstView.height;
		execRects = (NPDISP_DDRECTL*)malloc((size_t)rectCount * sizeof(*execRects));
		if (!execRects) {
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		for (UINT32 i = 0; i < rectCount; ++i) {
			NPDISP_DDRECTL clippedToMap;
			NPDISP_DDRECTL actual;
			if (!npdisp_ddraw_intersectRect(&clippedToMap, &clipRects[i], &mapDst) ||
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
			npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		for (UINT32 i = 0; i < execRectCount; ++i) {
			srcSystemViews[i] = srcView;
			if (!npdisp_ddraw_mapDestRectToSource(&srcNeedRects[i], clipped ? &execRects[i] : &mapDst, &mapDst, &mapSrc, rotation, mirrorX, mirrorY) ||
				!npdisp_ddraw_mirrorSystemRect(&srcSystemViews[i], &srcNeedRects[i], bytesPerPixel, true, &srcSystemBuffers[i])) {
				npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
				if (execRects) free(execRects);
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
			size_t snapshotPitch = 0;
			if (!npdisp_ddraw_snapshotRect(&srcView, &mapSrc, bytesPerPixel, &sourceSnapshot, &snapshotPitch)) {
				npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
				if (execRects) free(execRects);
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
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
		NPDISP_DDRECTL dst = clipped ? execRects[i] : mapDst;
		UINT8* dstSystemBuffer = NULL;
		// System-memory destinationもこの描画矩形だけをmirrorする。完全上書きなら既存画素は読まない。
		if (dstView.systemMemory && !npdisp_ddraw_mirrorSystemRect(&dstExecView, &dst, bytesPerPixel, destRequired, &dstSystemBuffer)) {
			if (sourceSnapshot) free(sourceSnapshot);
			npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (data.dwFlags & NPDISP_DDBLT_COLORFILL) {
			npdisp_ddraw_fillRect(&dstExecView, &dst, bytesPerPixel, data.bltFX.dwFillColor);
		}
		else if (alphaBlend) {
			if (!npdisp_ddraw_bltAlphaPixels(&dstExecView, &dst, &mapDst, srcRectView, &mapSrc, &originalMapSrc, bytesPerPixel,
				srcColorKeyPtr, dstColorKeyPtr, rotation, mirrorX, mirrorY, &srcAlpha, &dstAlpha, flatAddress)) {
				if (dstSystemBuffer) free(dstSystemBuffer);
				if (sourceSnapshot) free(sourceSnapshot);
				npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
				if (execRects) free(execRects);
				if (clipRects) free(clipRects);
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
		}
		// SRCCOPYは汎用ROP経路を避ける。等倍は矩形copy、Stretchはnearest-neighbor専用経路で処理する。
		else if (rotation == 0 && !mirrorX && !mirrorY && rop3 == 0xcc && sourceRequired) {
			if (stretch) {
				if (!npdisp_ddraw_stretchRectSrccopy(&dstExecView, &dst, &mapDst, srcRectView, &mapSrc, bytesPerPixel, srcColorKeyPtr, dstColorKeyPtr)) {
					if (dstSystemBuffer) free(dstSystemBuffer);
					if (sourceSnapshot) free(sourceSnapshot);
					npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
					if (execRects) free(execRects);
					if (clipRects) free(clipRects);
					return NPDISP_DDHAL_DRIVER_NOTHANDLED;
				}
			}
			else {
				NPDISP_DDRECTL src;
				src.left = mapSrc.left + (dst.left - mapDst.left);
				src.top = mapSrc.top + (dst.top - mapDst.top);
				src.right = src.left + (dst.right - dst.left);
				src.bottom = src.top + (dst.bottom - dst.top);
				if (src.left < 0 || src.top < 0 || src.right > (SINT32)srcRectView->width || src.bottom > (SINT32)srcRectView->height) {
					if (dstSystemBuffer) free(dstSystemBuffer);
					if (sourceSnapshot) free(sourceSnapshot);
					npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
					if (execRects) free(execRects);
					if (clipRects) free(clipRects);
					return NPDISP_DDHAL_DRIVER_NOTHANDLED;
				}
				if (srcColorKeyPtr || dstColorKeyPtr) {
					if (!npdisp_ddraw_copyRectColorKey(&dstExecView, &dst, srcRectView, &src, bytesPerPixel, srcColorKeyPtr, dstColorKeyPtr)) {
						if (dstSystemBuffer) free(dstSystemBuffer);
						if (sourceSnapshot) free(sourceSnapshot);
						npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
						if (execRects) free(execRects);
						if (clipRects) free(clipRects);
						return NPDISP_DDHAL_DRIVER_NOTHANDLED;
					}
				}
				else {
					npdisp_ddraw_copyRect(&dstExecView, &dst, srcRectView, &src, bytesPerPixel);
				}
			}
		}
		else if (!npdisp_ddraw_bltPixels(&dstExecView, &dst, &mapDst, sourceRequired ? srcRectView : NULL,
			sourceRequired ? &mapSrc : NULL, patternRequired ? patternPixels : NULL,
			(size_t)NPDISP_DD_PATTERN_SIZE * bytesPerPixel,
			bytesPerPixel, rop3, srcColorKeyPtr, dstColorKeyPtr, rotation, mirrorX, mirrorY)) {
			if (dstSystemBuffer) free(dstSystemBuffer);
			if (sourceSnapshot) free(sourceSnapshot);
			npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}

		if (dstView.systemMemory && !npdisp_ddraw_commitSystemRect(&dstExecView, &dst, bytesPerPixel)) {
			if (dstSystemBuffer) free(dstSystemBuffer);
			if (sourceSnapshot) free(sourceSnapshot);
			npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
			if (execRects) free(execRects);
			if (clipRects) free(clipRects);
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (dstSystemBuffer) free(dstSystemBuffer);
		drewAny = true;
		if (dstView.visible) npdisp_setDirty(dst.left, dst.top, dst.right, dst.bottom);
		if (dstView.apertureOffset != 0 && dstView.apertureOffset == npdisp.mm_ddOverlayOffset && npdisp.mm_ddOverlayVisible) npdisp_dd_overlayDirty();
	}

	if (sourceSnapshot) free(sourceSnapshot);
	npdisp_ddraw_freeSystemSourceMirrors(srcNeedRects, srcSystemViews, srcSystemBuffers, execRectCount);
	if (execRects) free(execRects);
	if (clipRects) free(clipRects);
	if (drewAny && (dstView.visible || (dstView.apertureOffset != 0 && dstView.apertureOffset == npdisp.mm_ddOverlayOffset && npdisp.mm_ddOverlayVisible))) npdisp.updated = 1;
	if (dstView.systemMemory || (sourceRequired && srcView.systemMemory)) {
		TRACEOUT11(("NPDISP11 DD_BLT_SYSMEM src=%u dst=%u srcptr=%08x dstptr=%08x flags=%08x clipped=%u rects=%u",
			(sourceRequired && srcView.systemMemory) ? 1U : 0U, dstView.systemMemory ? 1U : 0U,
			sourceRequired ? srcView.gbl.fpVidMem : 0U, dstView.gbl.fpVidMem, data.dwFlags, clipped ? 1U : 0U, rectCount));
	}
	if (patternRequired) {
		TRACEOUT11(("NPDISP11 DD_BLT_ROP3 rop=%02x pattern=%08x pattern_sysmem=%u src=%u dst=%u flags=%08x",
			rop3, data.bltFX.dwFillColor, patternView.systemMemory ? 1U : 0U,
			npdisp_ddraw_ropUsesSource(rop3) ? 1U : 0U, npdisp_ddraw_ropUsesDest(rop3) ? 1U : 0U, data.dwFlags));
	}
	if (ddFx) {
		TRACEOUT11(("NPDISP11 DD_BLT_DDFX ddfx=%08x rotation=%u mirrorx=%u mirrory=%u src=%dx%d dst=%dx%d flags=%08x clipped=%u rects=%u",
			ddFx, rotation, mirrorX ? 1U : 0U, mirrorY ? 1U : 0U, mapSrcWidth, mapSrcHeight, mapDstWidth, mapDstHeight,
			data.dwFlags, clipped ? 1U : 0U, rectCount));
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
	if ((currentView.lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_OVERLAY) ||
		(targetView.lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_OVERLAY)) {
		if (currentView.systemMemory || targetView.systemMemory || !currentView.apertureOffset || !targetView.apertureOffset ||
			currentView.width != targetView.width || currentView.height != targetView.height ||
			currentView.pitch != targetView.pitch || currentView.fourCC != targetView.fourCC) {
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (npdisp.mm_ddOverlayVisible && npdisp.mm_ddOverlayOffset == currentView.apertureOffset) {
			npdisp.mm_ddOverlayOffset = targetView.apertureOffset;
			npdisp.mm_ddOverlayPitch = targetView.pitch;
			npdisp.mm_ddOverlayWidth = targetView.width;
			npdisp.mm_ddOverlayHeight = targetView.height;
			npdisp.mm_ddOverlayFourCC = targetView.fourCC;
			npdisp_dd_overlayDirty();
		}
		data.ddRVal = 0;
		if (!npdisp_ddraw_writeGuest(&data, lpDataAddr, sizeof(data), flatAddress)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		TRACEOUT11(("NPDISP11 DD_OVERLAY_FLIP from=%08x to=%08x visible=%u",
			currentView.apertureOffset, targetView.apertureOffset, npdisp.mm_ddOverlayVisible));
		return NPDISP_DDHAL_DRIVER_HANDLED;
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
			halInfo.ddCaps.dwCaps = NPDISP_DDCAPS_BLT | NPDISP_DDCAPS_BLTSTRETCH | NPDISP_DDCAPS_GDI | NPDISP_DDCAPS_OVERLAY |
				NPDISP_DDCAPS_OVERLAYCANTCLIP | NPDISP_DDCAPS_OVERLAYSTRETCH |
				NPDISP_DDCAPS_READSCANLINE | NPDISP_DDCAPS_COLORKEY | NPDISP_DDCAPS_BLTCOLORFILL |
				NPDISP_DDCAPS_CANCLIP | NPDISP_DDCAPS_CANCLIPSTRETCHED;
			if (npdisp.bpp >= 15) halInfo.ddCaps.dwCaps |= NPDISP_DDCAPS_ALPHA;
			halInfo.ddCaps.dwCKeyCaps = NPDISP_DDCKEYCAPS_DESTBLT | NPDISP_DDCKEYCAPS_DESTBLTCLRSPACE |
				NPDISP_DDCKEYCAPS_SRCBLT | NPDISP_DDCKEYCAPS_SRCBLTCLRSPACE |
				NPDISP_DDCKEYCAPS_DESTOVERLAY | NPDISP_DDCKEYCAPS_DESTOVERLAYCLRSPACE |
				NPDISP_DDCKEYCAPS_SRCOVERLAY | NPDISP_DDCKEYCAPS_SRCOVERLAYCLRSPACE;
			halInfo.ddCaps.dwFXCaps = NPDISP_DDFXCAPS_BLTMIRRORLEFTRIGHT | NPDISP_DDFXCAPS_BLTMIRRORUPDOWN |
				NPDISP_DDFXCAPS_BLTROTATION90 | NPDISP_DDFXCAPS_BLTSHRINKX | NPDISP_DDFXCAPS_BLTSHRINKXN |
				NPDISP_DDFXCAPS_BLTSHRINKY | NPDISP_DDFXCAPS_BLTSHRINKYN |
				NPDISP_DDFXCAPS_BLTSTRETCHX | NPDISP_DDFXCAPS_BLTSTRETCHXN |
				NPDISP_DDFXCAPS_BLTSTRETCHY | NPDISP_DDFXCAPS_BLTSTRETCHYN |
				NPDISP_DDFXCAPS_OVERLAYSHRINKX | NPDISP_DDFXCAPS_OVERLAYSHRINKXN |
				NPDISP_DDFXCAPS_OVERLAYSHRINKY | NPDISP_DDFXCAPS_OVERLAYSHRINKYN |
				NPDISP_DDFXCAPS_OVERLAYSTRETCHX | NPDISP_DDFXCAPS_OVERLAYSTRETCHXN |
				NPDISP_DDFXCAPS_OVERLAYSTRETCHY | NPDISP_DDFXCAPS_OVERLAYSTRETCHYN |
				NPDISP_DDFXCAPS_OVERLAYMIRRORLEFTRIGHT | NPDISP_DDFXCAPS_OVERLAYMIRRORUPDOWN;
			if (npdisp.bpp >= 15) {
				halInfo.ddCaps.dwFXCaps |= NPDISP_DDFXCAPS_BLTALPHA;
				halInfo.ddCaps.dwFXAlphaCaps = NPDISP_DDFXALPHACAPS_BLTALPHASURFACES | NPDISP_DDFXALPHACAPS_BLTALPHASURFACESNEG;
				halInfo.ddCaps.dwAlphaBltConstBitDepths = NPDISP_DDBD_2 | NPDISP_DDBD_4 | NPDISP_DDBD_8;
				halInfo.ddCaps.dwAlphaBltSurfaceBitDepths = NPDISP_DDBD_8;
			}
			halInfo.ddCaps.ddsCaps.dwCaps = NPDISP_DDSCAPS_PRIMARYSURFACE;
			halInfo.ddCaps.dwMaxVisibleOverlays = 1;
			halInfo.ddCaps.dwCurrVisibleOverlays = 0;
			halInfo.ddCaps.dwMinOverlayStretch = 1;
			halInfo.ddCaps.dwMaxOverlayStretch = 1000000;
			if (npdisp.version >= 14 && npdisp.bpp >= 15) {
				const UINT32 fourCCAddr = ddModeInfoAddr + NPDISP_DD_FOURCC_SLOT_OFFSET;
				halInfo.ddCaps.dwCaps |= NPDISP_DDCAPS_OVERLAYFOURCC;
				UINT32 fourCC = NPDISP_DD_FOURCC_YUY2;
				halInfo.ddCaps.dwNumFourCCCodes = 1;
				halInfo.lpdwFourCC = fourCCAddr;
				npdisp_writeMemory(&fourCC, fourCCAddr, sizeof(fourCC));
			}
			if (!npdisp_dd_configureVideoMemory(&halInfo)) {
				TRACEOUT11(("NPDISP11 DD_CREATE_HAL_REJECT video memory setup failed"));
				retValue = 0;
				break;
			}
			// 8x8 patternを含む3入力ROP evaluatorで全256種類のROP3を処理する。
			for (UINT32 rop = 0; rop < 256; ++rop) {
				halInfo.ddCaps.dwRops[rop >> 5] |= 1UL << (rop & 31);
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
	case NPDISP_DDBRIDGE_CB_SURF_UPDATEOVERLAY: retValue = npdisp_func_DD_UpdateOverlay(lpDataAddr, true); break;
	case NPDISP_DDBRIDGE_CB_SURF_SETOVERLAYPOS: retValue = npdisp_func_DD_SetOverlayPosition(lpDataAddr, true); break;
	default: break;
	}
	TRACEOUT11(("NPDISP11 DD32_BRIDGE id=%04x data=%08x ret=%08x", callbackId, lpDataAddr, retValue));
	return retValue;
}


#endif
