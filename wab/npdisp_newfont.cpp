/**
 * @file	npdisp_newfont.cpp
 * @brief	NPDISP NewFontSegテキスト描画
 */

#include	"compiler.h"

#include	<map>
#include	<vector>

#include	"pccore.h"
#include	"wab.h"
#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_mem.h"
#include	"npdisp_palette.h"
#include	"npdisp_newfont.h"

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_NEWFONTSEG)

extern NPDISP_WINDOWS npdispwin;

#define NPDISP_ETO_GLYPH_INDEX  0x0010
#define NPDISP_ETO_BYTE_PACKED  0x0100
#define NPDISP_ETO_BIT_PACKED   0x0200
#define NPDISP_ETO_LEVEL_MODE	0x1000
#define NPDISP_NF_LARGE         0x0080
// LEVEL1/2/3は背景色・文字色を割合25%/50%/75%で合成　アンチエイリアシング用
#define NPDISP_BKMODE_LEVEL1     3
#define NPDISP_BKMODE_LEVEL2     4
#define NPDISP_BKMODE_LEVEL3     5
#define NPDISP_NEWFONT_MAX_CALL_BITS (64UL * 1024UL * 1024UL)

#pragma pack(push, 1)
typedef struct {
	UINT16 nfVersion;
	UINT16 nfFormat;
	UINT16 nfNumGlyphs;
	UINT32 nfGlyphOffset;
	UINT32 nfAWTable;
	UINT16 nfHeight;
	UINT16 nfAscent;
	UINT32 nfUniqueID;
} NPDISP_NEWFONTSEG;
// BYTE_PACKEDは各行をbyte境界　BIT_PACKEDはNumBitsの後に連続したbit列
typedef struct {
	SINT8 orgX;
	SINT8 orgY;
	UINT8 width;
	UINT8 height;
} NPDISP_SMALLROWGLYPH;
typedef struct {
	SINT16 orgX;
	SINT16 orgY;
	UINT16 width;
	UINT16 height;
} NPDISP_LARGEROWGLYPH;
typedef struct {
	SINT8 orgX;
	SINT8 orgY;
	UINT8 width;
	UINT8 height;
	UINT16 pixels;
} NPDISP_SMALLBITGLYPH;
typedef struct {
	SINT16 orgX;
	SINT16 orgY;
	UINT16 width;
	UINT16 height;
	UINT32 pixels;
} NPDISP_LARGEBITGLYPH;
#pragma pack(pop)

static bool npdisp_isNewFontSeg(UINT32 lpFontInfoAddr, NPDISP_NEWFONTSEG* nf)
{
	UINT16 selector;
	if (!lpFontInfoAddr || !nf || !npdisp.isWin9x) return false;
	selector = (UINT16)(lpFontInfoAddr >> 16);
	// NewFontSegと各テーブルのオフセットはフォントセグメント先頭基準
	if (!npdisp_readMemoryWith32Offset(nf, selector, 0, sizeof(*nf))) return false;
	return nf->nfNumGlyphs != 0;
}

static bool npdisp_readNewFontOffset(UINT32 lpFontInfoAddr, const NPDISP_NEWFONTSEG* nf, UINT16 glyph, UINT32* glyphOffset)
{
	UINT16 selector = (UINT16)(lpFontInfoAddr >> 16);
	UINT32 elemSize = (nf->nfFormat & NPDISP_NF_LARGE) ? sizeof(UINT32) : sizeof(UINT16);
	UINT64 tableOffset;
	if (glyph >= nf->nfNumGlyphs) return false;
	tableOffset = (UINT64)nf->nfGlyphOffset + (UINT64)glyph * elemSize;
	if (tableOffset > (UINT64)0xffffffffUL) return false;
	if (nf->nfFormat & NPDISP_NF_LARGE) {
		return npdisp_readMemoryWith32Offset(glyphOffset, selector, (UINT32)tableOffset, sizeof(UINT32)) != 0;
	}
	else {
		UINT16 ofs16;
		if (!npdisp_readMemoryWith32Offset(&ofs16, selector, (UINT32)tableOffset, sizeof(ofs16))) return false;
		*glyphOffset = ofs16;
		return true;
	}
}

static bool npdisp_readNewFontAW(UINT32 lpFontInfoAddr, const NPDISP_NEWFONTSEG* nf, UINT16 glyph, SINT16* aw)
{
	UINT64 tableOffset;
	if (glyph >= nf->nfNumGlyphs) return false;
	tableOffset = (UINT64)nf->nfAWTable + (UINT64)glyph * sizeof(SINT16);
	if (tableOffset > (UINT64)0xffffffffUL) return false;
	return npdisp_readMemoryWith32Offset(aw, (UINT16)(lpFontInfoAddr >> 16), (UINT32)tableOffset, sizeof(SINT16)) != 0;
}

static bool npdisp_isNullNewFontRect(const NPDISP_RECT* rect)
{
	return rect->left == 0 && rect->top == 0 && rect->right == 0 && rect->bottom == 0;
}

static bool npdisp_readNewFontOpaqueRects(UINT32 lpOpaqueRectAddr, std::vector<NPDISP_RECT>* rects)
{
	UINT16 selector;
	UINT32 offset;
	UINT32 maxRects;
	UINT32 i;

	if (!rects) return false;
	rects->clear();
	if (!lpOpaqueRectAddr) return true;
	selector = (UINT16)(lpOpaqueRectAddr >> 16);
	offset = lpOpaqueRectAddr & 0xffff;
	if (offset > 0x10000UL - sizeof(NPDISP_RECT)) return false;
	maxRects = (0x10000UL - offset) / sizeof(NPDISP_RECT);
	for (i = 0; i < maxRects; i++) {
		NPDISP_RECT rect;
		if (!npdisp_readMemoryWith32Offset(&rect, selector, offset + i * sizeof(rect), sizeof(rect))) return false;
		if (npdisp_isNullNewFontRect(&rect)) return true;
		rects->push_back(rect);
	}
	TRACEOUT(("NewFont opaque rectangle list has no terminator"));
	return false;
}

static bool npdisp_getNewFontClippedRect(const NPDISP_RECT* rect, const NPDISP_RECT* clip, bool hasClip, int targetWidth, int targetHeight, RECT* outRect)
{
	int left;
	int top;
	int right;
	int bottom;
	if (!rect || !outRect || targetWidth <= 0 || targetHeight <= 0) return false;
	left = max((int)rect->left, 0);
	top = max((int)rect->top, 0);
	right = min((int)rect->right, targetWidth);
	bottom = min((int)rect->bottom, targetHeight);
	if (hasClip) {
		left = max(left, (int)clip->left);
		top = max(top, (int)clip->top);
		right = min(right, (int)clip->right);
		bottom = min(bottom, (int)clip->bottom);
	}
	if (right <= left || bottom <= top) return false;
	outRect->left = left;
	outRect->top = top;
	outRect->right = right;
	outRect->bottom = bottom;
	return true;
}

static UINT32 npdisp_getNewFontRGBColor(UINT32 color)
{
	if (color & 0xff000000UL) {
		UINT32 index = color & 0xff;
		const NPDISP_RGB3* rgb;
		if (npdisp.bpp == 1) rgb = &npdisp_palette_rgb2[index & 1];
		else if (npdisp.bpp == 4) rgb = &npdisp_palette_rgb16[index & 15];
		else rgb = &npdisp_palette_rgb256[index];
		return ((UINT32)rgb->r) | ((UINT32)rgb->g << 8) | ((UINT32)rgb->b << 16);
	}
	return color & 0xffffffUL;
}

static UINT32 npdisp_getNewFontDIBColor(UINT32 color, const BITMAPINFO* bi, bool useVirtualPalette)
{
	UINT32 bpp;
	UINT32 colorCount;
	UINT32 index;
	const RGBQUAD* rgb;
	if (!bi) return color & 0xffffffUL;
	bpp = bi->bmiHeader.biBitCount;
	if (bpp > 8) return npdisp_getNewFontRGBColor(color);

	if (useVirtualPalette && bpp == 8) {
		if (color & 0xff000000UL) index = color & 0xff;
		else index = npdisp_FindNearest256((UINT8)(color & 0xff), (UINT8)((color >> 8) & 0xff), (UINT8)((color >> 16) & 0xff));
		return index | (index << 8) | (index << 16);
	}

	colorCount = bi->bmiHeader.biClrUsed;
	if (bpp < 31) {
		UINT32 maxColorCount = 1UL << bpp;
		if (colorCount == 0 || colorCount > maxColorCount) colorCount = maxColorCount;
	}
	if (colorCount == 0) return color & 0xffffffUL;
	if (color & 0xff000000UL) {
		index = color & 0xff;
		if (index >= colorCount) index %= colorCount;
		rgb = &bi->bmiColors[index];
		return ((UINT32)rgb->rgbRed) | ((UINT32)rgb->rgbGreen << 8) | ((UINT32)rgb->rgbBlue << 16);
	}
	return color & 0xffffffUL;
}

static bool npdisp_fillNewFontOpaqueRects(HDC tgtDC, const NPDISP_DRAWMODE* drawMode, const std::vector<NPDISP_RECT>& rects, const NPDISP_RECT* clip, bool hasClip, int targetWidth, int targetHeight, const BITMAPINFO* targetBi, bool targetUsesVirtualPalette, bool isDisplayDevice, bool markDirty)
{
	bool isBlack = false;
	bool isWhite = false;
	bool preferDither = false;
	bool canUseBlackWhitePatBlt = isDisplayDevice || (targetBi && targetBi->bmiHeader.biBitCount > 8);
	HBRUSH hBrush = NULL;
	size_t i;

	if (!tgtDC || !drawMode || targetWidth <= 0 || targetHeight <= 0) return false;
	if (rects.empty()) return true;

	if (canUseBlackWhitePatBlt &&
		(drawMode->bkColor & 0xffffff) == 0 && !(drawMode->bkColor & 0xff000000UL)) {
		isBlack = true;
	}
	else if (canUseBlackWhitePatBlt &&
		(drawMode->bkColor & 0xffffff) == 0xffffff && !(drawMode->bkColor & 0xff000000UL)) {
		isWhite = true;
	}
	else {
		UINT32 color;
		if (isDisplayDevice) color = (targetBi && targetBi->bmiHeader.biBitCount > 8) ? npdisp_getNewFontRGBColor(drawMode->bkColor) : npdisp_AdjustColorRefForGDI(drawMode->bkColor, &preferDither);
		else if (targetBi && targetBi->bmiHeader.biBitCount == 1 && npdisp.bpp == 1) color = npdisp_AdjustColorRefForGDI(drawMode->bkColor, &preferDither);
		else color = npdisp_getNewFontDIBColor(drawMode->bkColor, targetBi, targetUsesVirtualPalette);
		if (preferDither) {
			UINT32 actualColor1;
			UINT32 actualColor2;
			double ratio;
			MakePaletteDitherBrushColor(color, &actualColor1, &actualColor2, &ratio);
			hBrush = CreatePaletteDitherBrush(actualColor1, actualColor2, ratio);
		}
		else if (canUseBlackWhitePatBlt && (color & 0xffffff) == 0) {
			isBlack = true;
		}
		else if (canUseBlackWhitePatBlt && (color & 0xffffff) == 0xffffff) {
			isWhite = true;
		}
		else {
			hBrush = CreateSolidBrush(color);
		}
		if (!isBlack && !isWhite && !hBrush) return false;
	}

	for (i = 0; i < rects.size(); i++) {
		RECT gdiRect;
		bool ok = true;
		if (!npdisp_getNewFontClippedRect(&rects[i], clip, hasClip, targetWidth, targetHeight, &gdiRect)) continue;
		if (isBlack) {
			ok = PatBlt(tgtDC, gdiRect.left, gdiRect.top, gdiRect.right - gdiRect.left, gdiRect.bottom - gdiRect.top, BLACKNESS) != 0;
		}
		else if (isWhite) {
			ok = PatBlt(tgtDC, gdiRect.left, gdiRect.top, gdiRect.right - gdiRect.left, gdiRect.bottom - gdiRect.top, WHITENESS) != 0;
		}
		else {
			ok = FillRect(tgtDC, &gdiRect, hBrush) != 0;
		}
		if (!ok) {
			if (hBrush) DeleteObject(hBrush);
			return false;
		}
		if (markDirty) npdisp_setDirty(gdiRect.left, gdiRect.top, gdiRect.right, gdiRect.bottom);
	}
	if (hBrush) DeleteObject(hBrush);
	return true;
}


typedef struct {
	UINT16 glyph;
	SINT16 advance;
	SINT16 orgX;
	SINT16 orgY;
	UINT16 width;
	UINT16 height;
	UINT32 headerOffset;
	UINT32 dataOffset;
	UINT32 dataSize;
	UINT16 rowBytes;
} NPDISP_NEWFONT_RUNGLYPH;

struct NPDISP_NEWFONT_CALL_GLYPH {
	UINT16 glyph;
	bool advanceValid;
	bool headerValid;
	bool bitsValid;
	SINT16 advance;
	SINT16 orgX;
	SINT16 orgY;
	UINT16 width;
	UINT16 height;
	UINT16 rowBytes;
	UINT32 headerOffset;
	UINT32 dataOffset;
	UINT32 dataSize;
	UINT32 bitsOffset;
	NPDISP_NEWFONT_CALL_GLYPH() : glyph(0), advanceValid(false), headerValid(false), bitsValid(false), advance(0), orgX(0), orgY(0), width(0), height(0), rowBytes(0), headerOffset(0), dataOffset(0), dataSize(0), bitsOffset(0) {}
};

struct NPDISP_NEWFONT_CALL_CACHE {
	std::vector<NPDISP_NEWFONT_CALL_GLYPH> glyphs;
	std::vector<UINT8> bitsPool;
};

static NPDISP_NEWFONT_CALL_CACHE npdisp_newFontCall;
static std::vector<NPDISP_NEWFONT_RUNGLYPH> npdisp_newFontRunScratch;
static std::vector<UINT16> npdisp_newFontIndexScratch;
static std::vector<UINT8> npdisp_newFontByteIndexScratch;
static std::vector<SINT16> npdisp_newFontWidthScratch;
static std::vector<NPDISP_RECT> npdisp_newFontOpaqueRectScratch;
static std::vector<UINT32> npdisp_newFontCallIndex;
static std::vector<UINT32> npdisp_newFontCallStamp;
static UINT8* npdisp_newFontMaskBits = NULL;
static UINT32 npdisp_newFontCallGeneration = 0;
static HDC npdisp_newFontMaskDC = NULL;
static HGDIOBJ npdisp_newFontMaskOldBitmap = NULL;
static HBITMAP npdisp_newFontMaskBitmap = NULL;
static HBRUSH npdisp_newFontTextBrush = NULL;
static UINT32 npdisp_newFontTextBrushColor = 0;
static bool npdisp_newFontTextBrushValid = false;
static int npdisp_newFontMaskWidth = 0;
static int npdisp_newFontMaskHeight = 0;
static int npdisp_newFontMaskStride = 0;

static void npdisp_clearNewFontCallCache(void)
{
	npdisp_newFontCall.glyphs.clear();
	npdisp_newFontCall.bitsPool.clear();
}

static void npdisp_beginNewFontCallCache(const NPDISP_NEWFONTSEG* nf)
{
	npdisp_clearNewFontCallCache();
	if (!nf) return;
	if (npdisp_newFontCallIndex.size() < nf->nfNumGlyphs) {
		npdisp_newFontCallIndex.resize(nf->nfNumGlyphs);
		npdisp_newFontCallStamp.resize(nf->nfNumGlyphs, 0);
	}
	npdisp_newFontCallGeneration++;
	if (npdisp_newFontCallGeneration == 0) {
		if (!npdisp_newFontCallStamp.empty()) memset(&npdisp_newFontCallStamp[0], 0, npdisp_newFontCallStamp.size() * sizeof(UINT32));
		npdisp_newFontCallGeneration = 1;
	}
}

static NPDISP_NEWFONT_CALL_GLYPH* npdisp_getNewFontCallGlyph(UINT16 glyph)
{
	if (glyph < npdisp_newFontCallStamp.size() && npdisp_newFontCallStamp[glyph] == npdisp_newFontCallGeneration) {
		UINT32 index = npdisp_newFontCallIndex[glyph];
		if (index < npdisp_newFontCall.glyphs.size()) return &npdisp_newFontCall.glyphs[index];
	}
	NPDISP_NEWFONT_CALL_GLYPH callGlyph;
	callGlyph.glyph = glyph;
	npdisp_newFontCall.glyphs.push_back(callGlyph);
	if (glyph < npdisp_newFontCallStamp.size()) {
		npdisp_newFontCallIndex[glyph] = (UINT32)(npdisp_newFontCall.glyphs.size() - 1);
		npdisp_newFontCallStamp[glyph] = npdisp_newFontCallGeneration;
	}
	return &npdisp_newFontCall.glyphs.back();
}

static bool npdisp_loadNewFontCallAdvance(UINT32 lpFontInfoAddr, const NPDISP_NEWFONTSEG* nf, UINT16 glyph, NPDISP_NEWFONT_CALL_GLYPH* cg)
{
	if (!cg) return false;
	if (cg->advanceValid) return true;
	if (!npdisp_readNewFontAW(lpFontInfoAddr, nf, glyph, &cg->advance)) return false;
	cg->advanceValid = true;
	return true;
}

static bool npdisp_loadNewFontCallHeader(UINT32 lpFontInfoAddr, const NPDISP_NEWFONTSEG* nf, UINT16 glyph, bool bitPacked, NPDISP_NEWFONT_CALL_GLYPH* cg)
{
	UINT16 selector;
	UINT32 glyphHeaderOffset;
	UINT64 glyphDataOffset;
	UINT64 glyphBits64;
	UINT64 glyphBytes64;
	if (!cg) return false;
	if (cg->headerValid) return true;
	if (!npdisp_readNewFontOffset(lpFontInfoAddr, nf, glyph, &glyphHeaderOffset)) return false;
	selector = (UINT16)(lpFontInfoAddr >> 16);
	cg->headerOffset = glyphHeaderOffset;
	if (bitPacked) {
		UINT32 storedPixels;
		if (nf->nfFormat & NPDISP_NF_LARGE) {
			NPDISP_LARGEBITGLYPH gh;
			if (!npdisp_readMemoryWith32Offset(&gh, selector, glyphHeaderOffset, sizeof(gh))) return false;
			cg->orgX = gh.orgX;
			cg->orgY = gh.orgY;
			cg->width = gh.width;
			cg->height = gh.height;
			storedPixels = gh.pixels;
			glyphDataOffset = (UINT64)glyphHeaderOffset + sizeof(gh);
		}
		else {
			NPDISP_SMALLBITGLYPH gh;
			if (!npdisp_readMemoryWith32Offset(&gh, selector, glyphHeaderOffset, sizeof(gh))) return false;
			cg->orgX = gh.orgX;
			cg->orgY = gh.orgY;
			cg->width = gh.width;
			cg->height = gh.height;
			storedPixels = gh.pixels;
			glyphDataOffset = (UINT64)glyphHeaderOffset + sizeof(gh);
		}
		glyphBits64 = (UINT64)cg->width * (UINT64)cg->height;
		if (glyphBits64 > (UINT64)0xffffffffUL || storedPixels != glyphBits64) return false;
		cg->rowBytes = 0;
		glyphBytes64 = ((UINT64)storedPixels + 7) / 8;
	}
	else {
		if (nf->nfFormat & NPDISP_NF_LARGE) {
			NPDISP_LARGEROWGLYPH gh;
			if (!npdisp_readMemoryWith32Offset(&gh, selector, glyphHeaderOffset, sizeof(gh))) return false;
			cg->orgX = gh.orgX;
			cg->orgY = gh.orgY;
			cg->width = gh.width;
			cg->height = gh.height;
			glyphDataOffset = (UINT64)glyphHeaderOffset + sizeof(gh);
		}
		else {
			NPDISP_SMALLROWGLYPH gh;
			if (!npdisp_readMemoryWith32Offset(&gh, selector, glyphHeaderOffset, sizeof(gh))) return false;
			cg->orgX = gh.orgX;
			cg->orgY = gh.orgY;
			cg->width = gh.width;
			cg->height = gh.height;
			glyphDataOffset = (UINT64)glyphHeaderOffset + sizeof(gh);
		}
		cg->rowBytes = (UINT16)((cg->width + 7) / 8);
		glyphBytes64 = (UINT64)cg->rowBytes * (UINT64)cg->height;
	}
	if (glyphDataOffset > (UINT64)0xffffffffUL) return false;
	if (glyphBytes64 > (UINT64)0xffffffffUL || glyphBytes64 > (UINT64)(size_t)-1) return false;
	cg->dataOffset = (UINT32)glyphDataOffset;
	cg->dataSize = (UINT32)glyphBytes64;
	if ((UINT64)cg->dataOffset + glyphBytes64 > ((UINT64)0xffffffffUL + 1)) return false;
	cg->headerValid = true;
	return true;
}

static bool npdisp_loadNewFontCallBits(UINT32 lpFontInfoAddr, NPDISP_NEWFONT_CALL_GLYPH* cg, const UINT8** bits)
{
	UINT16 selector;
	size_t oldSize;
	if (bits) *bits = NULL;
	if (!cg || !bits || !cg->headerValid) return false;
	if (cg->bitsValid) {
		if (cg->dataSize == 0) return true;
		if ((size_t)cg->bitsOffset + (size_t)cg->dataSize > npdisp_newFontCall.bitsPool.size()) return false;
		*bits = &npdisp_newFontCall.bitsPool[cg->bitsOffset];
		return true;
	}
	if (cg->dataSize == 0) {
		cg->bitsOffset = 0;
		cg->bitsValid = true;
		return true;
	}
	oldSize = npdisp_newFontCall.bitsPool.size();
	if (oldSize + (size_t)cg->dataSize < oldSize || oldSize + (size_t)cg->dataSize > NPDISP_NEWFONT_MAX_CALL_BITS) return false;
	npdisp_newFontCall.bitsPool.resize(oldSize + (size_t)cg->dataSize);
	selector = (UINT16)(lpFontInfoAddr >> 16);
	if (!npdisp_readMemoryWith32Offset(&npdisp_newFontCall.bitsPool[oldSize], selector, cg->dataOffset, cg->dataSize)) {
		npdisp_newFontCall.bitsPool.resize(oldSize);
		return false;
	}
	cg->bitsOffset = (UINT32)oldSize;
	cg->bitsValid = true;
	*bits = &npdisp_newFontCall.bitsPool[cg->bitsOffset];
	return true;
}

static void npdisp_releaseNewFontScratch(void)
{
	npdisp_clearNewFontCallCache();
	if (npdisp_newFontMaskDC) {
		if (npdisp_newFontMaskOldBitmap) SelectObject(npdisp_newFontMaskDC, npdisp_newFontMaskOldBitmap);
		DeleteDC(npdisp_newFontMaskDC);
	}
	if (npdisp_newFontMaskBitmap) DeleteObject(npdisp_newFontMaskBitmap);
	if (npdisp_newFontTextBrush) DeleteObject(npdisp_newFontTextBrush);
	npdisp_newFontMaskDC = NULL;
	npdisp_newFontMaskOldBitmap = NULL;
	npdisp_newFontMaskBitmap = NULL;
	npdisp_newFontTextBrush = NULL;
	npdisp_newFontTextBrushColor = 0;
	npdisp_newFontTextBrushValid = false;
	npdisp_newFontMaskWidth = 0;
	npdisp_newFontMaskHeight = 0;
	npdisp_newFontMaskStride = 0;
	npdisp_newFontMaskBits = NULL;
	std::vector<NPDISP_NEWFONT_RUNGLYPH>().swap(npdisp_newFontRunScratch);
	std::vector<UINT16>().swap(npdisp_newFontIndexScratch);
	std::vector<UINT8>().swap(npdisp_newFontByteIndexScratch);
	std::vector<SINT16>().swap(npdisp_newFontWidthScratch);
	std::vector<NPDISP_RECT>().swap(npdisp_newFontOpaqueRectScratch);
	std::vector<UINT32>().swap(npdisp_newFontCallIndex);
	std::vector<UINT32>().swap(npdisp_newFontCallStamp);
	npdisp_newFontCallGeneration = 0;
}

static bool npdisp_ensureNewFontMask(int width, int height)
{
	int allocWidth;
	int allocHeight;
	HDC createDC;
	HBITMAP newBitmap;
	void* newBits = NULL;
	BITMAPINFO_1BPP bi = { 0 };
	if (width <= 0 || height <= 0) return false;
	if (npdisp_newFontMaskBitmap && npdisp_newFontMaskDC && npdisp_newFontMaskBits && width <= npdisp_newFontMaskWidth && height <= npdisp_newFontMaskHeight) return true;
	allocWidth = (width + 63) & ~63;
	allocHeight = (height + 15) & ~15;
	if (allocWidth < width || allocHeight < height) return false;
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = allocWidth;
	bi.bmiHeader.biHeight = -allocHeight;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 1;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biClrUsed = 2;
	bi.bmiHeader.biClrImportant = 2;
	bi.bmiColors[0].rgbRed = 0;
	bi.bmiColors[0].rgbGreen = 0;
	bi.bmiColors[0].rgbBlue = 0;
	bi.bmiColors[0].rgbReserved = 0;
	bi.bmiColors[1].rgbRed = 0xff;
	bi.bmiColors[1].rgbGreen = 0xff;
	bi.bmiColors[1].rgbBlue = 0xff;
	bi.bmiColors[1].rgbReserved = 0;
	createDC = npdispwin.hdc ? npdispwin.hdc : npdispwin.hdcCache[0];
	newBitmap = CreateDIBSection(createDC, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &newBits, NULL, 0);
	if (!newBitmap || !newBits) {
		if (newBitmap) DeleteObject(newBitmap);
		return false;
	}
	if (!npdisp_newFontMaskDC) {
		npdisp_newFontMaskDC = CreateCompatibleDC(createDC);
		if (!npdisp_newFontMaskDC) {
			DeleteObject(newBitmap);
			return false;
		}
		npdisp_newFontMaskOldBitmap = SelectObject(npdisp_newFontMaskDC, newBitmap);
		if (!npdisp_newFontMaskOldBitmap || npdisp_newFontMaskOldBitmap == HGDI_ERROR) {
			DeleteObject(newBitmap);
			DeleteDC(npdisp_newFontMaskDC);
			npdisp_newFontMaskDC = NULL;
			npdisp_newFontMaskOldBitmap = NULL;
			return false;
		}
	}
	else {
		HGDIOBJ oldSelected = SelectObject(npdisp_newFontMaskDC, newBitmap);
		if (!oldSelected || oldSelected == HGDI_ERROR) {
			DeleteObject(newBitmap);
			return false;
		}
		if (npdisp_newFontMaskBitmap) DeleteObject(npdisp_newFontMaskBitmap);
	}
	npdisp_newFontMaskBitmap = newBitmap;
	npdisp_newFontMaskBits = (UINT8*)newBits;
	npdisp_newFontMaskWidth = allocWidth;
	npdisp_newFontMaskHeight = allocHeight;
	npdisp_newFontMaskStride = ((allocWidth + 31) / 32) * 4;
	if (npdisp_newFontMaskStride <= 0) return false;
	return true;
}

static HBRUSH npdisp_getNewFontTextBrush(UINT32 color)
{
	if ((color & 0xffffffUL) == 0 && (color & 0xff000000UL) == 0) return (HBRUSH)GetStockObject(BLACK_BRUSH);
	if ((color & 0xffffffUL) == 0xffffffUL && (color & 0xff000000UL) == 0) return (HBRUSH)GetStockObject(WHITE_BRUSH);
	if (npdisp_newFontTextBrush && npdisp_newFontTextBrushValid && npdisp_newFontTextBrushColor == color) return npdisp_newFontTextBrush;
	HBRUSH newBrush = CreateSolidBrush(color);
	if (!newBrush) return NULL;
	if (npdisp_newFontTextBrush) DeleteObject(npdisp_newFontTextBrush);
	npdisp_newFontTextBrush = newBrush;
	npdisp_newFontTextBrushColor = color;
	npdisp_newFontTextBrushValid = true;
	return newBrush;
}


static void npdisp_orNewFontBitSpan(UINT8* dstRow, int dstBit, const UINT8* srcRow, int srcRowBytes, int srcBit, int bitCount)
{
	if (srcBit == 0 && bitCount > 0) {
		int dstByte = dstBit >> 3;
		int dstShift = dstBit & 7;
		int srcBytes = (bitCount + 7) >> 3;
		int tailBits = bitCount & 7;
		if (srcBytes > srcRowBytes) srcBytes = srcRowBytes;
		if (dstShift == 0) {
			int i;
			for (i = 0; i < srcBytes; i++) {
				UINT8 data = srcRow[i];
				if (i == srcBytes - 1 && tailBits) data &= (UINT8)(0xff << (8 - tailBits));
				dstRow[dstByte + i] |= data;
			}
			return;
		}
		else {
			int i;
			for (i = 0; i < srcBytes; i++) {
				UINT8 data = srcRow[i];
				UINT8 spill;
				if (i == srcBytes - 1 && tailBits) data &= (UINT8)(0xff << (8 - tailBits));
				dstRow[dstByte + i] |= (UINT8)(data >> dstShift);
				spill = (UINT8)(data << (8 - dstShift));
				if (spill) dstRow[dstByte + i + 1] |= spill;
			}
			return;
		}
	}
	while (bitCount > 0) {
		int dstShift = dstBit & 7;
		int copyBits = 8 - dstShift;
		int srcByte = srcBit >> 3;
		int srcShift = srcBit & 7;
		UINT16 window;
		UINT8 value;
		UINT8 mask;
		if (copyBits > bitCount) copyBits = bitCount;
		window = (UINT16)srcRow[srcByte] << 8;
		if (srcShift && srcByte + 1 < srcRowBytes) window |= srcRow[srcByte + 1];
		value = (UINT8)((window << srcShift) >> 8);
		mask = (UINT8)(0xff << (8 - copyBits));
		mask >>= dstShift;
		dstRow[dstBit >> 3] |= (UINT8)((value >> dstShift) & mask);
		dstBit += copyBits;
		srcBit += copyBits;
		bitCount -= copyBits;
	}
}

static void npdisp_includeNewFontRect(RECT* bounds, bool* hasBounds, const RECT* rect)
{
	if (!bounds || !hasBounds || !rect || rect->right <= rect->left || rect->bottom <= rect->top) return;
	if (!*hasBounds) {
		*bounds = *rect;
		*hasBounds = true;
		return;
	}
	if (rect->left < bounds->left) bounds->left = rect->left;
	if (rect->top < bounds->top) bounds->top = rect->top;
	if (rect->right > bounds->right) bounds->right = rect->right;
	if (rect->bottom > bounds->bottom) bounds->bottom = rect->bottom;
}

static bool npdisp_getNewFontChannelInfo(UINT32 mask, int* shift, UINT32* maxValue)
{
	int s = 0;
	UINT32 normalized;
	if (!mask || !shift || !maxValue) return false;
	while ((mask & 1) == 0) {
		mask >>= 1;
		s++;
	}
	normalized = mask;
	if ((normalized & (normalized + 1)) != 0) return false;
	*shift = s;
	*maxValue = normalized;
	return true;
}

static UINT8 npdisp_unpackNewFontChannel(UINT32 pixel, UINT32 mask, int shift, UINT32 maxValue)
{
	UINT32 value = (pixel & mask) >> shift;
	return (UINT8)(((UINT64)value * 255 + maxValue / 2) / maxValue);
}

static UINT32 npdisp_packNewFontChannel(UINT8 value, UINT32 mask, int shift, UINT32 maxValue)
{
	UINT32 packed = (UINT32)(((UINT64)value * maxValue + 127) / 255);
	return (packed << shift) & mask;
}

static UINT8 npdisp_blendNewFontComponent(UINT8 dst, UINT8 text, int level)
{
	return (UINT8)(((int)dst * (4 - level) + (int)text * level + 2) / 4);
}

static bool npdisp_blendNewFontMaskBits(void* dstBits, UINT32 dstStride, const BITMAPINFO* bi, int drawLeft, int drawTop, int width, int height, const UINT8* maskBits, int maskStride, COLORREF textColor, int level)
{
	int bpp;
	int y;
	UINT8 textR;
	UINT8 textG;
	UINT8 textB;
	UINT32 redMask = 0;
	UINT32 greenMask = 0;
	UINT32 blueMask = 0;
	int redShift = 0;
	int greenShift = 0;
	int blueShift = 0;
	UINT32 redMax = 0;
	UINT32 greenMax = 0;
	UINT32 blueMax = 0;
	int bitmapHeight;

	if (!dstBits || !bi || !maskBits || dstStride == 0 || maskStride <= 0 || width <= 0 || height <= 0) return false;
	if (level < 1 || level > 3) return false;
	bpp = bi->bmiHeader.biBitCount;
	if (bpp != 16 && bpp != 24 && bpp != 32) return false;
	bitmapHeight = bi->bmiHeader.biHeight < 0 ? -bi->bmiHeader.biHeight : bi->bmiHeader.biHeight;
	if (bitmapHeight <= 0) return false;
	textR = (UINT8)(textColor & 0xff);
	textG = (UINT8)((textColor >> 8) & 0xff);
	textB = (UINT8)((textColor >> 16) & 0xff);

	if (bpp == 16 || bpp == 32) {
		if (bi->bmiHeader.biCompression == BI_BITFIELDS) {
			const DWORD* masks = (const DWORD*)bi->bmiColors;
			redMask = masks[0];
			greenMask = masks[1];
			blueMask = masks[2];
		}
		else if (bpp == 16) {
			redMask = 0x00007c00UL;
			greenMask = 0x000003e0UL;
			blueMask = 0x0000001fUL;
		}
		else {
			redMask = 0x00ff0000UL;
			greenMask = 0x0000ff00UL;
			blueMask = 0x000000ffUL;
		}
		if (!npdisp_getNewFontChannelInfo(redMask, &redShift, &redMax) ||
			!npdisp_getNewFontChannelInfo(greenMask, &greenShift, &greenMax) ||
			!npdisp_getNewFontChannelInfo(blueMask, &blueShift, &blueMax)) return false;
	}

	for (y = 0; y < height; y++) {
		const UINT8* maskRow = maskBits + (size_t)y * maskStride;
		int dstY = drawTop + y;
		int dstRowIndex = (bi->bmiHeader.biHeight < 0) ? dstY : (bitmapHeight - 1 - dstY);
		UINT8* dstRow;
		if (dstY < 0 || dstY >= bitmapHeight || dstRowIndex < 0 || dstRowIndex >= bitmapHeight) return false;
		dstRow = (UINT8*)dstBits + (size_t)dstRowIndex * dstStride;
		int x;
		for (x = 0; x < width; x++) {
			UINT8 mask = (UINT8)(0x80 >> (x & 7));
			if (!(maskRow[x >> 3] & mask)) continue;
			if (bpp == 24) {
				UINT8* pixel = dstRow + (size_t)(drawLeft + x) * 3;
				pixel[2] = npdisp_blendNewFontComponent(pixel[2], textR, level);
				pixel[1] = npdisp_blendNewFontComponent(pixel[1], textG, level);
				pixel[0] = npdisp_blendNewFontComponent(pixel[0], textB, level);
			}
			else if (bpp == 16) {
				UINT16* pixel = (UINT16*)(dstRow + (size_t)(drawLeft + x) * 2);
				UINT32 value = *pixel;
				UINT8 r = npdisp_unpackNewFontChannel(value, redMask, redShift, redMax);
				UINT8 g = npdisp_unpackNewFontChannel(value, greenMask, greenShift, greenMax);
				UINT8 b = npdisp_unpackNewFontChannel(value, blueMask, blueShift, blueMax);
				UINT32 preserved = value & ~(redMask | greenMask | blueMask);
				r = npdisp_blendNewFontComponent(r, textR, level);
				g = npdisp_blendNewFontComponent(g, textG, level);
				b = npdisp_blendNewFontComponent(b, textB, level);
				*pixel = (UINT16)(preserved | npdisp_packNewFontChannel(r, redMask, redShift, redMax) | npdisp_packNewFontChannel(g, greenMask, greenShift, greenMax) | npdisp_packNewFontChannel(b, blueMask, blueShift, blueMax));
			}
			else {
				UINT32* pixel = (UINT32*)(dstRow + (size_t)(drawLeft + x) * 4);
				UINT32 value = *pixel;
				UINT8 r = npdisp_unpackNewFontChannel(value, redMask, redShift, redMax);
				UINT8 g = npdisp_unpackNewFontChannel(value, greenMask, greenShift, greenMax);
				UINT8 b = npdisp_unpackNewFontChannel(value, blueMask, blueShift, blueMax);
				UINT32 preserved = value & ~(redMask | greenMask | blueMask);
				r = npdisp_blendNewFontComponent(r, textR, level);
				g = npdisp_blendNewFontComponent(g, textG, level);
				b = npdisp_blendNewFontComponent(b, textB, level);
				*pixel = preserved | npdisp_packNewFontChannel(r, redMask, redShift, redMax) | npdisp_packNewFontChannel(g, greenMask, greenShift, greenMax) | npdisp_packNewFontChannel(b, blueMask, blueShift, blueMax);
			}
		}
	}
	return true;
}

static bool npdisp_blendNewFontMaskGDI(HDC tgtDC, int drawLeft, int drawTop, int width, int height, const UINT8* maskBits, int maskStride, COLORREF textColor, int level)
{
	int y;
	UINT8 textR;
	UINT8 textG;
	UINT8 textB;
	if (!tgtDC || !maskBits || maskStride <= 0 || width <= 0 || height <= 0 || level < 1 || level > 3) return false;
	textR = (UINT8)(textColor & 0xff);
	textG = (UINT8)((textColor >> 8) & 0xff);
	textB = (UINT8)((textColor >> 16) & 0xff);
	for (y = 0; y < height; y++) {
		const UINT8* maskRow = maskBits + (size_t)y * maskStride;
		int x;
		for (x = 0; x < width; x++) {
			COLORREF dstColor;
			UINT8 r;
			UINT8 g;
			UINT8 b;
			if (!(maskRow[x >> 3] & (UINT8)(0x80 >> (x & 7)))) continue;
			dstColor = GetPixel(tgtDC, drawLeft + x, drawTop + y);
			if (dstColor == CLR_INVALID) return false;
			r = npdisp_blendNewFontComponent((UINT8)(dstColor & 0xff), textR, level);
			g = npdisp_blendNewFontComponent((UINT8)((dstColor >> 8) & 0xff), textG, level);
			b = npdisp_blendNewFontComponent((UINT8)((dstColor >> 16) & 0xff), textB, level);
			if (!SetPixelV(tgtDC, drawLeft + x, drawTop + y, RGB(r, g, b))) return false;
		}
	}
	return true;
}

static bool npdisp_drawNewFontMaskSolid(HDC tgtDC, int drawLeft, int drawTop, int width, int height, UINT32 textColor)
{
	const DWORD textMaskRop = 0x00e20746UL; /* D = S ? P : D */
	HBRUSH textBrush;
	HGDIOBJ oldBrush;
	int oldBkMode;
	COLORREF oldBkColor;
	COLORREF oldTextColor;
	bool ok = false;

	if (!tgtDC || !npdisp_newFontMaskBitmap || !npdisp_newFontMaskDC || width <= 0 || height <= 0) return false;
	textBrush = npdisp_getNewFontTextBrush(textColor);
	if (!textBrush) return false;
	oldBrush = SelectObject(tgtDC, textBrush);
	if (!oldBrush || oldBrush == HGDI_ERROR) return false;
	oldBkMode = GetBkMode(tgtDC);
	oldBkColor = GetBkColor(tgtDC);
	oldTextColor = GetTextColor(tgtDC);

	SetBkMode(tgtDC, OPAQUE);
	SetBkColor(tgtDC, 0xffffff);
	SetTextColor(tgtDC, 0x000000);
	if (BitBlt(tgtDC, drawLeft, drawTop, width, height, npdisp_newFontMaskDC, 0, 0, textMaskRop)) {
		ok = true;
	}
	else {
		SetBkColor(tgtDC, 0x000000);
		SetTextColor(tgtDC, 0xffffff);
		if (BitBlt(tgtDC, drawLeft, drawTop, width, height, npdisp_newFontMaskDC, 0, 0, SRCAND)) {
			SetBkColor(tgtDC, textColor);
			SetTextColor(tgtDC, 0x000000);
			if (BitBlt(tgtDC, drawLeft, drawTop, width, height, npdisp_newFontMaskDC, 0, 0, SRCPAINT)) ok = true;
		}
	}
	if (oldBkMode) SetBkMode(tgtDC, oldBkMode);
	if (oldBkColor != CLR_INVALID) SetBkColor(tgtDC, oldBkColor);
	if (oldTextColor != CLR_INVALID) SetTextColor(tgtDC, oldTextColor);
	SelectObject(tgtDC, oldBrush);
	return ok;
}

static UINT32 npdisp_func_ExtTextOutNewFont(UINT32 lpDestDevAddr, SINT16 wDestXOrg, SINT16 wDestYOrg, UINT32 lpClipRectAddr, UINT32 lpStringAddr, SINT16 wCount, UINT32 lpFontInfoAddr, UINT32 lpDrawModeAddr, UINT32 lpTextXFormAddr, UINT32 lpCharWidthsAddr, UINT32 lpOpaqueRectAddr, UINT16 wOptions, const NPDISP_NEWFONTSEG* nf)
{
	int count = (int)wCount;
	NPDISP_NEWFONT_RUNGLYPH* run = NULL;
	UINT16* inputGlyphs = NULL;
	UINT32 retValue = 0x80000000UL;
	int i;
	SINT64 penX = 0;
	SINT64 minX = 0;
	SINT64 minY = 0;
	SINT64 maxX = 0;
	SINT64 maxY = 0;
	int drawLeft = 0;
	int drawTop = 0;
	int drawRight = 0;
	int drawBottom = 0;
	int width = 0;
	int height = 0;
	int stride = 0;
	bool hasGlyphBounds = false;
	bool hasOutputBounds = false;
	bool outputChanged = false;
	RECT outputBounds = { 0 };
	NPDISP_DRAWMODE drawMode = { 0 };
	NPDISP_RECT clip = { 0 };
	bool hasClip = lpClipRectAddr != 0;
	bool isDisplayDevice = npdisp_isDisplayDevice(lpDestDevAddr);
	NPDISP_PBITMAP_EXT dstPBmp = { 0 };
	NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
	HDC tgtDC = npdispwin.hdc;
	const BITMAPINFO* targetBi = (const BITMAPINFO*)&npdispwin.bi;
	int targetWidth = npdisp.width;
	int targetHeight = npdisp.height;
	bool targetIsRGB = npdispwin.bi.bmiHeader.biBitCount > 8;
	bool targetUsesVirtualPalette = false;
	// 公開仕様ではnfFormatにもpacking種別があるが、実際はExtTextOutのwOptionsで形式を選択
	bool bitPacked = !(wOptions & NPDISP_ETO_BYTE_PACKED) && (wOptions & NPDISP_ETO_BIT_PACKED);
	bool haveDstPBmp = false;

	(void)lpTextXFormAddr;
	npdisp_beginNewFontCallCache(nf);

	if (nf && count > 0) {
		size_t pendingGlyphCapacity = min((size_t)count, (size_t)nf->nfNumGlyphs);
		UINT64 pendingBitsCapacity64 = (UINT64)pendingGlyphCapacity * (UINT64)max((int)nf->nfHeight, 1) * 2;
		size_t pendingBitsCapacity = (size_t)min(pendingBitsCapacity64, (UINT64)(256 * 1024));
		if (npdisp_newFontCall.glyphs.capacity() < pendingGlyphCapacity) npdisp_newFontCall.glyphs.reserve(pendingGlyphCapacity);
		if (npdisp_newFontCall.bitsPool.capacity() < pendingBitsCapacity) npdisp_newFontCall.bitsPool.reserve(pendingBitsCapacity);
	}

	if (lpDrawModeAddr) {
		if (!npdisp_readMemory(&drawMode, lpDrawModeAddr, sizeof(drawMode))) goto exit;
		npdisp_AdjustDrawModeColor(&drawMode);
	}
	else {
		drawMode.bkColor = 0xffffff;
		drawMode.TextColor = 0;
		drawMode.LbkColor = 0xffffff;
		drawMode.LTextColor = 0;
		drawMode.bkMode = 1;
	}
	if (hasClip && !npdisp_readMemory(&clip, lpClipRectAddr, sizeof(clip))) goto exit;

	if (!isDisplayDevice) {
		if (!lpDestDevAddr || !npdisp_readPBitmap(&dstPBmp, lpDestDevAddr)) goto exit;
		haveDstPBmp = true;
		targetWidth = dstPBmp.bmWidth;
		targetHeight = dstPBmp.bmHeight;
		if (targetWidth < 0) targetWidth = -targetWidth;
		if (targetHeight < 0) targetHeight = -targetHeight;
		if (targetWidth <= 0 || targetHeight <= 0) goto exit;
	}
	else if (targetWidth <= 0 || targetHeight <= 0) {
		goto exit;
	}

	if (!npdisp_readNewFontOpaqueRects(lpOpaqueRectAddr, &npdisp_newFontOpaqueRectScratch)) goto exit;
	for (i = 0; i < (int)npdisp_newFontOpaqueRectScratch.size(); i++) {
		RECT rect;
		if (npdisp_getNewFontClippedRect(&npdisp_newFontOpaqueRectScratch[i], &clip, hasClip, targetWidth, targetHeight, &rect)) {
			npdisp_includeNewFontRect(&outputBounds, &hasOutputBounds, &rect);
		}
	}

	if (wCount > 0) {
		if (!nf || !lpStringAddr || count <= 0) goto exit;
		if ((size_t)count > ((size_t)-1) / sizeof(NPDISP_NEWFONT_RUNGLYPH)) goto exit;
		npdisp_newFontRunScratch.resize((size_t)count);
		npdisp_newFontIndexScratch.resize((size_t)count);
		run = &npdisp_newFontRunScratch[0];
		inputGlyphs = &npdisp_newFontIndexScratch[0];
		memset(run, 0, sizeof(NPDISP_NEWFONT_RUNGLYPH) * (size_t)count);

		// 公開仕様ではNF_FROM_BMP時はBYTEとされるが、実際はETO_GLYPH_INDEXの有無でBYTE/WORDを選択
		if (!(wOptions & NPDISP_ETO_GLYPH_INDEX)) {
			npdisp_newFontByteIndexScratch.resize((size_t)count);
			if (!npdisp_readMemory(&npdisp_newFontByteIndexScratch[0], lpStringAddr, count)) goto exit;
			for (i = 0; i < count; i++) inputGlyphs[i] = npdisp_newFontByteIndexScratch[i];
		}
		else {
			if (!npdisp_readMemory(inputGlyphs, lpStringAddr, sizeof(UINT16) * count)) goto exit;
		}
		if (lpCharWidthsAddr) {
			npdisp_newFontWidthScratch.resize((size_t)count);
			if (!npdisp_readMemory(&npdisp_newFontWidthScratch[0], lpCharWidthsAddr, sizeof(SINT16) * count)) goto exit;
		}

		for (i = 0; i < count; i++) {
			UINT16 glyph = inputGlyphs[i];
			NPDISP_NEWFONT_CALL_GLYPH* cg;
			if (glyph >= nf->nfNumGlyphs) {
				glyph = 0;
			}
			run[i].glyph = glyph;
			cg = npdisp_getNewFontCallGlyph(glyph);
			if (lpCharWidthsAddr) {
				run[i].advance = npdisp_newFontWidthScratch[i];
			}
			else {
				if (!npdisp_loadNewFontCallAdvance(lpFontInfoAddr, nf, glyph, cg)) goto exit;
				run[i].advance = cg->advance;
			}
			if (!npdisp_loadNewFontCallHeader(lpFontInfoAddr, nf, glyph, bitPacked, cg)) goto exit;
			run[i].orgX = cg->orgX;
			run[i].orgY = cg->orgY;
			run[i].width = cg->width;
			run[i].height = cg->height;
			run[i].rowBytes = cg->rowBytes;
			run[i].headerOffset = cg->headerOffset;
			run[i].dataOffset = cg->dataOffset;
			run[i].dataSize = cg->dataSize;

			if (run[i].width > 0 && run[i].height > 0) {
				SINT64 left = penX + run[i].orgX;
				SINT64 top = -(SINT64)run[i].orgY;
				SINT64 right = left + run[i].width;
				SINT64 bottom = top + run[i].height;
				if (!hasGlyphBounds) {
					minX = left;
					minY = top;
					maxX = right;
					maxY = bottom;
					hasGlyphBounds = true;
				}
				else {
					if (left < minX) minX = left;
					if (top < minY) minY = top;
					if (right > maxX) maxX = right;
					if (bottom > maxY) maxY = bottom;
				}
			}
			penX += run[i].advance;
		}

		if (hasGlyphBounds) {
			SINT64 left = (SINT64)wDestXOrg + minX;
			SINT64 top = (SINT64)wDestYOrg + minY;
			SINT64 right = (SINT64)wDestXOrg + maxX;
			SINT64 bottom = (SINT64)wDestYOrg + maxY;
			if (left < 0) left = 0;
			if (top < 0) top = 0;
			if (right > targetWidth) right = targetWidth;
			if (bottom > targetHeight) bottom = targetHeight;
			if (hasClip) {
				if (left < clip.left) left = clip.left;
				if (top < clip.top) top = clip.top;
				if (right > clip.right) right = clip.right;
				if (bottom > clip.bottom) bottom = clip.bottom;
			}
			if (right > left && bottom > top) {
				size_t usedMaskBytes;
				RECT textRect;
				UINT8* bits;
				drawLeft = (int)left;
				drawTop = (int)top;
				drawRight = (int)right;
				drawBottom = (int)bottom;
				width = drawRight - drawLeft;
				height = drawBottom - drawTop;
				if (!npdisp_ensureNewFontMask(width, height)) goto exit;
				stride = npdisp_newFontMaskStride;
				usedMaskBytes = (size_t)stride * (size_t)height;
				if (stride <= 0 || usedMaskBytes / (size_t)stride != (size_t)height || !npdisp_newFontMaskBits) goto exit;
				memset(npdisp_newFontMaskBits, 0, usedMaskBytes);
				bits = npdisp_newFontMaskBits;

				penX = 0;
				for (i = 0; i < count; i++) {
					SINT64 glyphLeft64 = (SINT64)wDestXOrg + penX + run[i].orgX;
					SINT64 glyphTop64 = (SINT64)wDestYOrg - run[i].orgY;
					SINT64 glyphRight64 = glyphLeft64 + run[i].width;
					SINT64 glyphBottom64 = glyphTop64 + run[i].height;
					int visibleLeft;
					int visibleTop;
					int visibleRight;
					int visibleBottom;
					const UINT8* srcGlyphBits = NULL;
					NPDISP_NEWFONT_CALL_GLYPH* cg;
					if (run[i].dataSize == 0 || run[i].width == 0 || run[i].height == 0) {
						penX += run[i].advance;
						continue;
					}
					if (glyphRight64 <= drawLeft || glyphLeft64 >= drawRight || glyphBottom64 <= drawTop || glyphTop64 >= drawBottom) {
						penX += run[i].advance;
						continue;
					}
					cg = npdisp_getNewFontCallGlyph(run[i].glyph);
					if (!npdisp_loadNewFontCallBits(lpFontInfoAddr, cg, &srcGlyphBits)) goto exit;
					if (!srcGlyphBits) goto exit;
					visibleLeft = max((int)glyphLeft64, drawLeft);
					visibleTop = max((int)glyphTop64, drawTop);
					visibleRight = min((int)glyphRight64, drawRight);
					visibleBottom = min((int)glyphBottom64, drawBottom);
					if (visibleRight > visibleLeft && visibleBottom > visibleTop) {
						int srcX = visibleLeft - (int)glyphLeft64;
						int srcY = visibleTop - (int)glyphTop64;
						int dstX = visibleLeft - drawLeft;
						int dstY = visibleTop - drawTop;
						int copyWidth = visibleRight - visibleLeft;
						int copyHeight = visibleBottom - visibleTop;
						int y;
						if (bitPacked) {
							for (y = 0; y < copyHeight; y++) {
								UINT64 srcBitOffset = (UINT64)(srcY + y) * (UINT64)run[i].width + (UINT64)srcX;
								size_t srcByteOffset = (size_t)(srcBitOffset >> 3);
								int srcBitInByte = (int)(srcBitOffset & 7);
								UINT8* dstRow = bits + (size_t)(dstY + y) * stride;
								if (srcByteOffset >= run[i].dataSize) goto exit;
								npdisp_orNewFontBitSpan(dstRow, dstX, srcGlyphBits + srcByteOffset, (int)(run[i].dataSize - srcByteOffset), srcBitInByte, copyWidth);
							}
						}
						else {
							for (y = 0; y < copyHeight; y++) {
								size_t srcRowOffset = (size_t)(srcY + y) * run[i].rowBytes;
								const UINT8* srcRow;
								UINT8* dstRow;
								if (srcRowOffset + run[i].rowBytes > run[i].dataSize) goto exit;
								srcRow = srcGlyphBits + srcRowOffset;
								dstRow = bits + (size_t)(dstY + y) * stride;
								npdisp_orNewFontBitSpan(dstRow, dstX, srcRow, run[i].rowBytes, srcX, copyWidth);
							}
						}
					}
					penX += run[i].advance;
				}

				textRect.left = drawLeft;
				textRect.top = drawTop;
				textRect.right = drawRight;
				textRect.bottom = drawBottom;
				npdisp_includeNewFontRect(&outputBounds, &hasOutputBounds, &textRect);
			}
		}
	}

	if (!hasOutputBounds) {
		retValue = 0;
		goto exit;
	}

	if (!isDisplayDevice) {
		int preloadWidth = outputBounds.right - outputBounds.left;
		int preloadHeight = outputBounds.bottom - outputBounds.top;
		if (!haveDstPBmp || preloadWidth <= 0 || preloadHeight <= 0) goto exit;
		npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0, outputBounds.top, preloadHeight, outputBounds.left, preloadWidth);
		if (npdisp.longjmpnum != 0) goto exit;
		if (!npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0, outputBounds.top, preloadHeight, outputBounds.left, preloadWidth)) goto exit;
		tgtDC = bmphdc.hdc;
		targetBi = bmphdc.lpbi;
		targetIsRGB = targetBi && targetBi->bmiHeader.biBitCount > 8;
		targetUsesVirtualPalette = targetBi && targetBi->bmiHeader.biBitCount == 8 && npdisp.usePalette && dstPBmp.bmType != NPDISP_DEVTYPE_DIBENG;
	}
	if (!tgtDC || !targetBi) goto exit;

	if (!npdisp_newFontOpaqueRectScratch.empty()) {
		if (!npdisp_fillNewFontOpaqueRects(tgtDC, &drawMode, npdisp_newFontOpaqueRectScratch, &clip, hasClip, targetWidth, targetHeight, targetBi, targetUsesVirtualPalette, isDisplayDevice, isDisplayDevice)) goto exit;
		outputChanged = true;
	}

	if (drawRight > drawLeft && drawBottom > drawTop) {
		bool drawn = false;
		int aaLevel = 0;
		if (drawMode.bkMode == NPDISP_BKMODE_LEVEL1) aaLevel = 1;
		else if (drawMode.bkMode == NPDISP_BKMODE_LEVEL2) aaLevel = 2;
		else if (drawMode.bkMode == NPDISP_BKMODE_LEVEL3) aaLevel = 3;

		if (aaLevel != 0 && targetIsRGB) {
			void* dstBits = isDisplayDevice ? npdispwin.pBits : bmphdc.pBits;
			UINT32 dstStride = isDisplayDevice ? npdispwin.stride : bmphdc.stride;
			UINT32 textColor = npdisp_getNewFontRGBColor(drawMode.TextColor);
			if (dstBits) drawn = npdisp_blendNewFontMaskBits(dstBits, dstStride, targetBi, drawLeft, drawTop, width, height, npdisp_newFontMaskBits, stride, textColor, aaLevel);
			if (!drawn) drawn = npdisp_blendNewFontMaskGDI(tgtDC, drawLeft, drawTop, width, height, npdisp_newFontMaskBits, stride, textColor, aaLevel);
		}
		else {
			UINT32 textColor;
			if (isDisplayDevice) textColor = targetIsRGB ? npdisp_getNewFontRGBColor(drawMode.TextColor) : drawMode.LTextColor;
			else textColor = npdisp_getNewFontDIBColor(drawMode.TextColor, targetBi, targetUsesVirtualPalette);
			drawn = npdisp_drawNewFontMaskSolid(tgtDC, drawLeft, drawTop, width, height, textColor);
		}
		if (!drawn) goto exit;
		if (isDisplayDevice) npdisp_setDirty(drawLeft, drawTop, drawRight, drawBottom);
		outputChanged = true;
	}

	if (bmphdc.hdc) {
		npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc, outputBounds.top, outputBounds.bottom - outputBounds.top, outputBounds.left, outputBounds.right - outputBounds.left);
		if (npdisp.longjmpnum != 0) goto exit;
	}
	else if (isDisplayDevice && outputChanged) {
		npdisp.updated = 1;
	}
	retValue = 0;

exit:
	if (bmphdc.hdc) npdisp_FreeBitmap(&bmphdc);
	return retValue;
}

bool npdisp_newfont_isPackedExtTextOut(UINT16 wOptions)
{
	return npdisp.isWin9x && (wOptions & (NPDISP_ETO_BYTE_PACKED | NPDISP_ETO_BIT_PACKED));
}

bool npdisp_newfont_tryExtTextOut(UINT32 lpDestDevAddr, SINT16 wDestXOrg, SINT16 wDestYOrg, UINT32 lpClipRectAddr, UINT32 lpStringAddr, SINT16 wCount, UINT32 lpFontInfoAddr, UINT32 lpDrawModeAddr, UINT32 lpTextXFormAddr, UINT32 lpCharWidthsAddr, UINT32 lpOpaqueRectAddr, UINT16 wOptions, UINT32* retValue)
{
	NPDISP_NEWFONTSEG newFont;
	if (!retValue || !npdisp_newfont_isPackedExtTextOut(wOptions)) return false;
	// 負のwCountは従来FONTINFOの文字幅問い合わせ
	if (wCount < 0) return false;
	if (wCount == 0) {
		*retValue = npdisp_func_ExtTextOutNewFont(lpDestDevAddr, wDestXOrg, wDestYOrg, lpClipRectAddr, lpStringAddr, wCount, lpFontInfoAddr, lpDrawModeAddr, lpTextXFormAddr, lpCharWidthsAddr, lpOpaqueRectAddr, wOptions, NULL);
		npdisp_clearNewFontCallCache();
		return true;
	}
	if (!npdisp_isNewFontSeg(lpFontInfoAddr, &newFont)) {
		*retValue = npdisp.longjmpnum ? 0 : 0x80000000UL;
		npdisp_clearNewFontCallCache();
		return true;
	}
	*retValue = npdisp_func_ExtTextOutNewFont(lpDestDevAddr, wDestXOrg, wDestYOrg, lpClipRectAddr, lpStringAddr, wCount, lpFontInfoAddr, lpDrawModeAddr, lpTextXFormAddr, lpCharWidthsAddr, lpOpaqueRectAddr, wOptions, &newFont);
	npdisp_clearNewFontCallCache();
	return true;
}

void npdisp_newfont_reset(void)
{
	npdisp_releaseNewFontScratch();
}

#endif
