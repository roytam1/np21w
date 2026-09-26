/**
 * @file	npdisp_ogl.cpp
 * @brief	NPDISP graphics API compatibility bridge implementation
 *
 * This is an independent compatibility implementation.
 * It has not undergone any conformance process,
 * and no claim of conformance is made.
 */

#include "compiler.h"

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_D3D) && defined(SUPPORT_NPDISP_GL)

#include <map>
#include <vector>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "pccore.h"
#include "cpucore.h"

#include "npdispdef.h"
#include "npdisp.h"
#include "npdisp_mem.h"
#include "npdisp_palette.h"
#include "npdisp_d3d.h"
#include "npdisp_d3d_sw.h"
#include "npdisp_ogl.h"

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
#define TRACEOUTOGL(s) npdisp_dd_trace s
#else
#define TRACEOUTOGL(s) (void)s
#endif

#define NPDISP_OGL_BRIDGE_VERSION       0x0001000EUL
#define NPDISP_OGL_CMD_QUERY            0x0001UL
#define NPDISP_OGL_CMD_CONTEXT_CREATE   0x0002UL
#define NPDISP_OGL_CMD_CONTEXT_DESTROY  0x0003UL
#define NPDISP_OGL_CMD_CLEAR            0x0004UL
#define NPDISP_OGL_CMD_DRAW             0x0005UL
#define NPDISP_OGL_CMD_SWAP             0x0006UL
#define NPDISP_OGL_CMD_TEXTURE_UPLOAD   0x0007UL
#define NPDISP_OGL_CMD_READ_PIXELS      0x0008UL
#define NPDISP_OGL_CMD_DRAW_PIXELS      0x0009UL
#define NPDISP_OGL_CMD_COPY_PIXELS      0x000AUL
#define NPDISP_OGL_CMD_LIST_UPLOAD      0x000BUL
#define NPDISP_OGL_CMD_LIST_DELETE      0x000CUL
#define NPDISP_OGL_CMD_LIST_EXEC        0x000DUL
#define NPDISP_OGL_CMD_TEXTURE_DELETE   0x000EUL
#define NPDISP_OGL_PIXEL_BACK           0U
#define NPDISP_OGL_PIXEL_FRONT          1U

#define NPDISP_OGL_CLEAR_COLOR          0x0001UL
#define NPDISP_OGL_CLEAR_DEPTH          0x0002UL
#define NPDISP_OGL_CLEAR_STENCIL        0x0004UL
#define NPDISP_OGL_STENCIL_KEEP         1U
#define NPDISP_OGL_STENCIL_ZERO         2U
#define NPDISP_OGL_STENCIL_REPLACE      3U
#define NPDISP_OGL_STENCIL_INCR         4U
#define NPDISP_OGL_STENCIL_DECR         5U
#define NPDISP_OGL_STENCIL_INVERT       6U

#define NPDISP_OGL_TEXENV_MODULATE      1U
#define NPDISP_OGL_TEXENV_REPLACE       2U
#define NPDISP_OGL_TEXENV_DECAL         3U

#ifndef NPDISP_D3DTOP_BLENDTEXTUREALPHA
#define NPDISP_D3DTOP_BLENDTEXTUREALPHA 13U
#endif
#define NPDISP_OGL_TEXADDR_WRAP         1U
#define NPDISP_OGL_TEXADDR_CLAMP        2U
#define NPDISP_OGL_TEXFILTER_POINT      1U
#define NPDISP_OGL_TEXFILTER_LINEAR     2U

#pragma pack(push, 1)
typedef struct {
	UINT32 size;
	UINT32 version;
	UINT32 width;
	UINT32 height;
	UINT32 bpp;
} NPDISP_OGL_QUERY32;

typedef struct {
	UINT32 size;
	UINT32 context;
} NPDISP_OGL_CONTEXT32;

typedef struct {
	SINT32 left;
	SINT32 top;
	SINT32 right;
	SINT32 bottom;
} NPDISP_OGL_CLIPRECT32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 clipValid;
	UINT32 clipCount;
	UINT32 clipRects;
} NPDISP_OGL_SWAP32;

typedef struct {
	UINT32 size;
	UINT32 context;
	SINT32 x;
	SINT32 y;
	UINT32 width;
	UINT32 height;
	UINT32 flags;
	UINT32 color;
	UINT32 depth;
	UINT32 scissorEnable;
	SINT32 scissorX;
	SINT32 scissorY;
	UINT32 scissorWidth;
	UINT32 scissorHeight;
	UINT32 stencil;
	UINT32 stencilBits;
	UINT32 stencilWriteMask;
	UINT32 colorWriteDisableMask;
} NPDISP_OGL_CLEAR32;

typedef struct {
	float x;
	float y;
	float z;
	float rhw;
	UINT32 diffuse;
	float tu;
	float tv;
	float clip[4];
	float texcoord[4];
	float clipDistance[6];
} NPDISP_OGL_VERTEX32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 primitive;
	UINT32 vertexCount;
	UINT32 vertices;
	SINT32 x;
	SINT32 y;
	UINT32 width;
	UINT32 height;
	UINT32 shadeMode;
	UINT32 depthEnable;
	UINT32 depthWrite;
	UINT32 depthFunc;
	UINT32 blendEnable;
	UINT32 srcBlend;
	UINT32 destBlend;
	UINT32 alphaTestEnable;
	UINT32 alphaFunc;
	UINT32 alphaRef;
	UINT32 cullMode;
	UINT32 textureEnable;
	UINT32 textureId;
	UINT32 textureEnvMode;
	UINT32 textureWrapS;
	UINT32 textureWrapT;
	UINT32 textureMinFilter;
	UINT32 textureMagFilter;
	UINT32 scissorEnable;
	SINT32 scissorX;
	SINT32 scissorY;
	UINT32 scissorWidth;
	UINT32 scissorHeight;
	UINT32 stencilEnable;
	UINT32 stencilBits;
	UINT32 stencilFunc;
	UINT32 stencilRef;
	UINT32 stencilReadMask;
	UINT32 stencilWriteMask;
	UINT32 stencilFail;
	UINT32 stencilZFail;
	UINT32 stencilPass;
	float pointSize;
	float lineWidth;
	UINT32 lineStippleEnable;
	UINT32 lineStippleFactor;
	UINT32 lineStipplePattern;
	UINT32 polygonStippleEnable;
	UINT8 polygonStipple[128];
	UINT32 polygonModeFront;
	UINT32 polygonModeBack;
	UINT32 frontFace;
	UINT32 colorWriteDisableMask;
	UINT32 logicOpEnable;
	UINT32 logicOp;
	UINT32 clipPlaneMask;
	SINT32 viewportX;
	SINT32 viewportY;
	UINT32 viewportWidth;
	UINT32 viewportHeight;
	float depthNear;
	float depthFar;
} NPDISP_OGL_DRAW32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 textureId;
	UINT32 revision;
	UINT32 width;
	UINT32 height;
	UINT32 pixels;
	UINT32 hasAlpha;
} NPDISP_OGL_TEXTURE32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 textureId;
} NPDISP_OGL_TEXTURE_DELETE32;

typedef struct {
	UINT32 size;
	UINT32 context;
	SINT32 x;
	SINT32 y;
	UINT32 width;
	UINT32 height;
	UINT32 pixels;
	UINT32 buffer;
	SINT32 dstX;
	SINT32 dstY;
	UINT32 scissorEnable;
	SINT32 scissorX;
	SINT32 scissorY;
	UINT32 scissorWidth;
	UINT32 scissorHeight;
	float zoomX;
	float zoomY;
	UINT32 colorWriteDisableMask;
	UINT32 logicOpEnable;
	UINT32 logicOp;
} NPDISP_OGL_PIXELS32;

typedef struct {
	UINT32 op;
	UINT32 u[4];
	float f[16];
} NPDISP_OGL_LIST_COMMAND32;

typedef struct {
	UINT32 enabled;
	float ambient[4];
	float diffuse[4];
	float specular[4];
	float position[4];
	float spotDirection[3];
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
} NPDISP_OGL_LIST_LIGHT32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 list;
	UINT32 commandCount;
	UINT32 commands;
} NPDISP_OGL_LIST_UPLOAD32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 list;
	UINT32 range;
} NPDISP_OGL_LIST_DELETE32;

typedef struct {
	UINT32 size;
	UINT32 context;
	UINT32 listCount;
	UINT32 lists;
	NPDISP_OGL_DRAW32 draw;
	float modelview[16];
	float projection[16];
	float texture[16];
	SINT32 viewport[4];
	UINT32 matrixMode;
	float depthNear;
	float depthFar;
	float currentColor[4];
	float currentNormal[3];
	float currentTexCoord[4];
	UINT32 lighting;
	UINT32 normalize;
	UINT32 colorMaterial;
	UINT32 colorMaterialMode;
	float lightModelAmbient[4];
	UINT32 lightModelLocalViewer;
	NPDISP_OGL_LIST_LIGHT32 lights[8];
	float materialAmbient[4];
	float materialDiffuse[4];
	float materialSpecular[4];
	float materialEmission[4];
	float materialShininess;
	UINT32 fog;
	UINT32 fogMode;
	float fogColor[4];
	float fogDensity;
	float fogStart;
	float fogEnd;
	UINT32 texGenEnabled[4];
	UINT32 texGenMode[4];
	float texGenObjectPlane[16];
	float texGenEyePlane[16];
} NPDISP_OGL_LIST_EXEC32;
#pragma pack(pop)

typedef struct {
	std::vector<NPDISP_OGL_LIST_COMMAND32> commands;
} NPDISP_OGL_CACHED_LIST;

typedef struct {
	std::vector<UINT8> pixels;
	UINT32 width;
	UINT32 height;
	UINT32 hasAlpha;
	UINT32 revision;
} NPDISP_OGL_CACHED_TEXTURE;

typedef struct {
	std::vector<UINT8> color;
	std::vector<UINT8> depth;
	UINT32 width;
	UINT32 height;
	UINT32 bpp;
	SINT32 x;
	SINT32 y;
	UINT32 dirtyValid;
	SINT32 dirtyLeft;
	SINT32 dirtyTop;
	SINT32 dirtyRight;
	SINT32 dirtyBottom;
	UINT32 presentClipStateValid;
	UINT32 presentClipValid;
	SINT32 presentX;
	SINT32 presentY;
	std::vector<NPDISP_OGL_CLIPRECT32> presentClipRects;
	std::map<UINT32, NPDISP_OGL_CACHED_TEXTURE> textures;
	std::map<UINT32, NPDISP_OGL_CACHED_LIST> lists;
} NPDISP_OGL_CONTEXT;

#define NPDISP_OGL_STATE_MAGIC            0x31534c47UL
#define NPDISP_OGL_STATE_VERSION          1U

#pragma pack(push, 1)
typedef struct {
	UINT32 magic;
	UINT32 version;
	UINT32 contextCount;
} NPDISP_OGL_STATE_HEADER;

typedef struct {
	UINT32 context;
	UINT32 width;
	UINT32 height;
	UINT32 bpp;
	SINT32 x;
	SINT32 y;
	UINT32 colorSize;
	UINT32 depthSize;
	UINT32 textureCount;
	UINT32 listCount;
} NPDISP_OGL_STATE_CONTEXT;

typedef struct {
	UINT32 textureId;
	UINT32 width;
	UINT32 height;
	UINT32 hasAlpha;
	UINT32 revision;
	UINT32 pixelSize;
} NPDISP_OGL_STATE_TEXTURE;

typedef struct {
	UINT32 listId;
	UINT32 commandCount;
} NPDISP_OGL_STATE_LIST;
#pragma pack(pop)

static std::map<UINT32, NPDISP_OGL_CONTEXT> s_contexts;
static UINT32 s_pixelReadLogCount = 0;
static UINT32 s_pixelDrawLogCount = 0;
static UINT32 s_pixelCopyLogCount = 0;
static UINT32 s_listUploadLogCount = 0;
static UINT32 s_listExecLogCount = 0;
static UINT32 s_diagDirectDraws = 0;
static UINT32 s_diagListDraws = 0;
static UINT32 s_diagClipFast = 0;
static UINT32 s_diagClipPartial = 0;
static UINT32 s_diagClipReject = 0;
static UINT32 s_diagTextureSwitches = 0;
static UINT32 s_diagLastTextureId = 0xffffffffU;
static bool s_diagInListExec = false;
static UINT8 s_palette8DitherMap[16][32768];
static NPDISP_RGB3 s_palette8Snapshot[256];
static bool s_palette8DitherMapValid = false;
static UINT8 s_palette4DitherMap[16][32768];
static NPDISP_RGB3 s_palette4Snapshot[16];
static bool s_palette4DitherMapValid = false;
static UINT8 s_palette1LumaMap[32768];
static bool s_palette1LumaMapValid = false;

#define NPDISP_OGL_PERF_DIAG 1
#if NPDISP_OGL_PERF_DIAG
static LONGLONG s_perfFrequency = 0;
static UINT32 s_perfFrameCount = 0;
static UINT32 s_perfClearCalls = 0;
static UINT32 s_perfClearPixels = 0;
static UINT32 s_perfClearUs = 0;
static UINT32 s_perfClearMaxUs = 0;
static UINT32 s_perfClearMask = 0;
static UINT32 s_perfListCalls = 0;
static UINT32 s_perfListUs = 0;
#endif

static UINT32 npdisp_ogl_pixelBytes(UINT32 bpp);
static UINT32 npdisp_ogl_readNativePixel(const UINT8 *p, UINT32 bpp);
static void npdisp_ogl_writeNativePixel(UINT8 *p, UINT32 bpp, UINT32 color);
static UINT32 npdisp_ogl_readRawPixel(const UINT8 *p, UINT32 bpp);
static void npdisp_ogl_writeRawPixel(UINT8 *p, UINT32 bpp, UINT32 value);
static UINT32 npdisp_ogl_rawColorMask(UINT32 bpp);
static UINT32 npdisp_ogl_logicOp(UINT32 op, UINT32 source, UINT32 dest, UINT32 mask);
static UINT32 npdisp_ogl_applyRawColorMask(UINT32 value, UINT32 oldValue, UINT32 bpp, UINT32 disableMask);

static void npdisp_ogl_markDirty(NPDISP_OGL_CONTEXT *ctx, SINT32 left, SINT32 top, SINT32 right, SINT32 bottom)
{
	if (!ctx || !ctx->width || !ctx->height) return;
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > (SINT32)ctx->width) right = (SINT32)ctx->width;
	if (bottom > (SINT32)ctx->height) bottom = (SINT32)ctx->height;
	if (right <= left || bottom <= top) return;
	if (!ctx->dirtyValid) {
		ctx->dirtyLeft = left; ctx->dirtyTop = top; ctx->dirtyRight = right; ctx->dirtyBottom = bottom;
		ctx->dirtyValid = 1U;
	}
	else {
		if (left < ctx->dirtyLeft) ctx->dirtyLeft = left;
		if (top < ctx->dirtyTop) ctx->dirtyTop = top;
		if (right > ctx->dirtyRight) ctx->dirtyRight = right;
		if (bottom > ctx->dirtyBottom) ctx->dirtyBottom = bottom;
	}
}

static bool npdisp_ogl_updatePresentClipState(NPDISP_OGL_CONTEXT *ctx, UINT32 clipValid, const std::vector<NPDISP_OGL_CLIPRECT32> &rects)
{
	bool changed;
	if (!ctx) return true;
	changed = !ctx->presentClipStateValid || ctx->presentClipValid != clipValid || ctx->presentX != ctx->x || ctx->presentY != ctx->y || ctx->presentClipRects.size() != rects.size();
	if (!changed) {
		for (size_t i = 0; i < rects.size(); ++i) {
			const NPDISP_OGL_CLIPRECT32 &a = ctx->presentClipRects[i];
			const NPDISP_OGL_CLIPRECT32 &b = rects[i];
			if (a.left != b.left || a.top != b.top || a.right != b.right || a.bottom != b.bottom) { changed = true; break; }
		}
	}
	if (changed) {
		ctx->presentClipValid = clipValid;
		ctx->presentX = ctx->x;
		ctx->presentY = ctx->y;
		ctx->presentClipRects = rects;
		ctx->presentClipStateValid = 1U;
	}
	return changed;
}

static bool npdisp_ogl_getTarget(UINT32 context, SINT32 x, SINT32 y, UINT32 width, UINT32 height, NPDISP_D3D_SW_TARGET *target)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(context);
	UINT32 renderBpp;
	UINT32 bytesPerPixel;
	UINT64 bytes;
	if (!target || it == s_contexts.end() || !width || !height || !npdisp.bpp) return false;
	renderBpp = (npdisp.bpp <= 8U) ? 32U : npdisp.bpp;
	bytesPerPixel = npdisp_ogl_pixelBytes(renderBpp);
	if (!bytesPerPixel) return false;
	bytes = (UINT64)width * (UINT64)height * bytesPerPixel;
	if (bytes > 0x7fffffffULL) return false;
	NPDISP_OGL_CONTEXT &ctx = it->second;
	if (ctx.width != width || ctx.height != height || ctx.bpp != renderBpp || ctx.color.size() != (size_t)bytes) {
		ctx.color.assign((size_t)bytes, 0U);
		ctx.depth.clear();
		ctx.width = width;
		ctx.height = height;
		ctx.bpp = renderBpp;
		ctx.dirtyValid = 0U;
		ctx.presentClipStateValid = 0U;
		npdisp_ogl_markDirty(&ctx, 0, 0, (SINT32)width, (SINT32)height);
	}
	ctx.x = x;
	ctx.y = y;
	target->pixels = ctx.color.empty() ? NULL : &ctx.color[0];
	target->width = width;
	target->height = height;
	target->pitch = (SINT32)(width * bytesPerPixel);
	target->bpp = renderBpp;
	return target->pixels != NULL;
}

static bool npdisp_ogl_getDepth(UINT32 context, UINT32 width, UINT32 height, NPDISP_D3D_SW_DEPTH_TARGET *target)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(context);
	if (!target || it == s_contexts.end() || !width || !height) return false;
	NPDISP_OGL_CONTEXT &ctx = it->second;
	UINT64 bytes = (UINT64)width * (UINT64)height * 2U;
	if (bytes > 0x7fffffffULL) return false;
	if (ctx.width != width || ctx.height != height || ctx.depth.size() != (size_t)bytes) {
		ctx.depth.assign((size_t)bytes, 0xffU);
		ctx.width = width;
		ctx.height = height;
	}
	target->pixels = ctx.depth.empty() ? NULL : &ctx.depth[0];
	target->width = ctx.width;
	target->height = ctx.height;
	target->pitch = (SINT32)(ctx.width * 2U);
	return target->pixels != NULL;
}

static bool npdisp_ogl_clearDepthStencil(NPDISP_D3D_SW_DEPTH_TARGET *target, SINT32 left, SINT32 top, SINT32 right, SINT32 bottom, UINT16 depth, UINT32 stencil, UINT32 flags, UINT32 stencilBits, UINT32 stencilWriteMask)
{
	UINT32 depthMask;
	UINT32 stencilMax = 0U;
	UINT32 stencilShift = 0U;
	UINT16 stencilMask = 0U;
	UINT16 stencilClearMask = 0U;
	UINT16 depthValue;
	if (!target || !target->pixels || target->pitch <= 0 || !target->width || !target->height ||
		(UINT64)target->width * 2U > (UINT32)target->pitch || stencilBits > 4U) return false;
	if ((flags & NPDISP_OGL_CLEAR_STENCIL) && !stencilBits) return false;
	depthMask = stencilBits ? (0xffffU >> stencilBits) : 0xffffU;
	if (stencilBits) {
		stencilMax = (1U << stencilBits) - 1U;
		stencilShift = 16U - stencilBits;
		stencilMask = (UINT16)(stencilMax << stencilShift);
		stencilClearMask = (UINT16)((stencilWriteMask & stencilMax) << stencilShift);
	}
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > (SINT32)target->width) right = (SINT32)target->width;
	if (bottom > (SINT32)target->height) bottom = (SINT32)target->height;
	if (left >= right || top >= bottom) return true;
	depthValue = (depthMask == 0xffffU) ? depth : (UINT16)(((UINT32)depth * depthMask + 32767U) / 65535U);
	if (!stencilBits && (flags & NPDISP_OGL_CLEAR_DEPTH)) {
		for (SINT32 y = top; y < bottom; ++y) {
			UINT16* p = (UINT16*)(target->pixels + (size_t)y * (size_t)target->pitch) + left;
			for (SINT32 x = left; x < right; ++x) *p++ = depthValue;
		}
		return true;
	}
	if (stencilBits && (flags & NPDISP_OGL_CLEAR_DEPTH) && (flags & NPDISP_OGL_CLEAR_STENCIL) && stencilClearMask == stencilMask) {
		UINT16 raw = (UINT16)((depthValue & (UINT16)depthMask) | (((stencil & stencilMax) << stencilShift) & stencilMask));
		for (SINT32 y = top; y < bottom; ++y) {
			UINT16* p = (UINT16*)(target->pixels + (size_t)y * (size_t)target->pitch) + left;
			for (SINT32 x = left; x < right; ++x) *p++ = raw;
		}
		return true;
	}
	for (SINT32 y = top; y < bottom; ++y) {
		UINT8 *p = target->pixels + (size_t)y * (size_t)target->pitch + (size_t)left * 2U;
		for (SINT32 x = left; x < right; ++x, p += 2) {
			UINT16 raw = (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
			if (flags & NPDISP_OGL_CLEAR_DEPTH) raw = (UINT16)((raw & ~(UINT16)depthMask) | (depthValue & (UINT16)depthMask));
			if (flags & NPDISP_OGL_CLEAR_STENCIL) {
				UINT16 stencilValue = (UINT16)((stencil & stencilMax) << stencilShift);
				raw = (UINT16)((raw & ~stencilClearMask) | (stencilValue & stencilClearMask));
			}
			p[0] = (UINT8)raw;
			p[1] = (UINT8)(raw >> 8);
		}
	}
	(void)stencilMask;
	return true;
}

static bool npdisp_ogl_getClipRect(UINT32 enable, SINT32 x, SINT32 y, UINT32 width, UINT32 height, UINT32 targetWidth, UINT32 targetHeight, SINT32 *left, SINT32 *top, SINT32 *right, SINT32 *bottom)
{
	SINT64 x0, x1, y0, y1;
	if (!left || !top || !right || !bottom) return false;
	if (!enable) {
		*left = 0;
		*top = 0;
		*right = (SINT32)targetWidth;
		*bottom = (SINT32)targetHeight;
		return targetWidth && targetHeight;
	}
	x0 = (SINT64)x;
	x1 = x0 + (SINT64)width;
	y0 = (SINT64)targetHeight - ((SINT64)y + (SINT64)height);
	y1 = (SINT64)targetHeight - (SINT64)y;
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > (SINT64)targetWidth) x1 = targetWidth;
	if (y1 > (SINT64)targetHeight) y1 = targetHeight;
	if (x1 < x0) x1 = x0;
	if (y1 < y0) y1 = y0;
	*left = (SINT32)x0;
	*top = (SINT32)y0;
	*right = (SINT32)x1;
	*bottom = (SINT32)y1;
	return x1 > x0 && y1 > y0;
}

static UINT32 npdisp_ogl_depthFunc(UINT32 func)
{
	switch (func) {
	case 1: return NPDISP_D3DCMP_NEVER;
	case 2: return NPDISP_D3DCMP_LESS;
	case 3: return NPDISP_D3DCMP_EQUAL;
	case 4: return NPDISP_D3DCMP_LESSEQUAL;
	case 5: return NPDISP_D3DCMP_GREATER;
	case 6: return NPDISP_D3DCMP_NOTEQUAL;
	case 7: return NPDISP_D3DCMP_GREATEREQUAL;
	case 8: return NPDISP_D3DCMP_ALWAYS;
	default: return NPDISP_D3DCMP_LESS;
	}
}

static UINT32 npdisp_ogl_blend(UINT32 factor)
{
	switch (factor) {
	case 1: return NPDISP_D3DBLEND_ZERO;
	case 2: return NPDISP_D3DBLEND_ONE;
	case 3: return NPDISP_D3DBLEND_SRCCOLOR;
	case 4: return NPDISP_D3DBLEND_INVSRCCOLOR;
	case 5: return NPDISP_D3DBLEND_SRCALPHA;
	case 6: return NPDISP_D3DBLEND_INVSRCALPHA;
	case 7: return NPDISP_D3DBLEND_DESTALPHA;
	case 8: return NPDISP_D3DBLEND_INVDESTALPHA;
	case 9: return NPDISP_D3DBLEND_DESTCOLOR;
	case 10: return NPDISP_D3DBLEND_INVDESTCOLOR;
	case 11: return NPDISP_D3DBLEND_SRCALPHASAT;
	default: return NPDISP_D3DBLEND_ONE;
	}
}

static void npdisp_ogl_makeVertex(NPDISP_D3D_VERTEX *dst, const NPDISP_OGL_VERTEX32 *src)
{
	memset(dst, 0, sizeof(*dst));
	dst->x = src->x;
	dst->y = src->y;
	dst->z = src->z;
	dst->rhw = src->rhw;
	dst->diffuse = src->diffuse;
	dst->tu = src->tu;
	dst->tv = src->tv;
}

static UINT32 npdisp_ogl_stencilOp(UINT32 op)
{
	switch (op) {
	case NPDISP_OGL_STENCIL_KEEP: return NPDISP_D3DSTENCILOP_KEEP;
	case NPDISP_OGL_STENCIL_ZERO: return NPDISP_D3DSTENCILOP_ZERO;
	case NPDISP_OGL_STENCIL_REPLACE: return NPDISP_D3DSTENCILOP_REPLACE;
	case NPDISP_OGL_STENCIL_INCR: return NPDISP_D3DSTENCILOP_INCRSAT;
	case NPDISP_OGL_STENCIL_DECR: return NPDISP_D3DSTENCILOP_DECRSAT;
	case NPDISP_OGL_STENCIL_INVERT: return NPDISP_D3DSTENCILOP_INVERT;
	default: return NPDISP_D3DSTENCILOP_KEEP;
	}
}

static void npdisp_ogl_makeState(NPDISP_D3D_RASTERSTATE *state, const NPDISP_OGL_DRAW32 *draw)
{
	memset(state, 0, sizeof(*state));
	state->fillMode = NPDISP_D3DFILL_SOLID;
	state->shadeMode = draw->shadeMode ? NPDISP_D3DSHADE_FLAT : NPDISP_D3DSHADE_GOURAUD;
	state->cullMode = (draw->cullMode >= NPDISP_D3DCULL_NONE && draw->cullMode <= NPDISP_D3DCULL_CCW) ? draw->cullMode : NPDISP_D3DCULL_NONE;
	state->alphaTestEnable = draw->alphaTestEnable ? 1U : 0U;
	state->alphaRef = draw->alphaRef & 0xffU;
	state->alphaFunc = draw->alphaTestEnable ? npdisp_ogl_depthFunc(draw->alphaFunc) : NPDISP_D3DCMP_ALWAYS;
	state->alphaBlendEnable = draw->blendEnable ? 1U : 0U;
	state->srcBlend = npdisp_ogl_blend(draw->srcBlend);
	state->destBlend = npdisp_ogl_blend(draw->destBlend);
	state->zEnable = draw->depthEnable ? 1U : 0U;
	state->zWriteEnable = draw->depthWrite ? 1U : 0U;
	state->zFunc = npdisp_ogl_depthFunc(draw->depthFunc);
	state->stencilBits = draw->stencilBits <= 4U ? draw->stencilBits : 0U;
	state->depthMask = state->stencilBits ? (0xffffU >> state->stencilBits) : 0xffffU;
	state->stencilEnable = (draw->stencilEnable && state->stencilBits) ? 1U : 0U;
	state->stencilFunc = state->stencilEnable ? npdisp_ogl_depthFunc(draw->stencilFunc) : NPDISP_D3DCMP_ALWAYS;
	state->stencilRef = draw->stencilRef;
	state->stencilReadMask = draw->stencilReadMask;
	state->stencilWriteMask = draw->stencilWriteMask;
	state->stencilFail = npdisp_ogl_stencilOp(draw->stencilFail);
	state->stencilZFail = npdisp_ogl_stencilOp(draw->stencilZFail);
	state->stencilPass = npdisp_ogl_stencilOp(draw->stencilPass);
}

static void npdisp_ogl_makeTexture(NPDISP_D3D_TEXTURE *texture, const NPDISP_OGL_CACHED_TEXTURE *cached, const NPDISP_OGL_DRAW32 *draw)
{
	memset(texture, 0, sizeof(*texture));
	texture->pixels = cached->pixels.empty() ? NULL : &cached->pixels[0];
	texture->width = cached->width;
	texture->height = cached->height;
	texture->pitch = (SINT32)(cached->width * 4U);
	texture->bpp = 32U;
	texture->rMask = 0x00ff0000UL;
	texture->gMask = 0x0000ff00UL;
	texture->bMask = 0x000000ffUL;
	texture->aMask = cached->hasAlpha ? 0xff000000UL : 0U;
	texture->format = NPDISP_D3D_TEXTURE_FORMAT_RGB32;
	texture->texCoordIndex = 0U;
	texture->addressU = (draw->textureWrapS == NPDISP_OGL_TEXADDR_CLAMP) ? NPDISP_D3DTADDRESS_CLAMP : NPDISP_D3DTADDRESS_WRAP;
	texture->addressV = (draw->textureWrapT == NPDISP_OGL_TEXADDR_CLAMP) ? NPDISP_D3DTADDRESS_CLAMP : NPDISP_D3DTADDRESS_WRAP;
	texture->minFilter = (draw->textureMinFilter == NPDISP_OGL_TEXFILTER_LINEAR) ? NPDISP_D3DTFN_LINEAR : NPDISP_D3DTFN_POINT;
	texture->magFilter = (draw->textureMagFilter == NPDISP_OGL_TEXFILTER_LINEAR) ? NPDISP_D3DTFG_LINEAR : NPDISP_D3DTFG_POINT;
	texture->mipFilter = NPDISP_D3DTFP_NONE;
	texture->mipCount = 1U;
	if (draw->textureEnvMode == NPDISP_OGL_TEXENV_REPLACE) {
		texture->colorOp = NPDISP_D3DTOP_SELECTARG1;
		texture->colorArg1 = NPDISP_D3DTA_TEXTURE;
		texture->alphaOp = NPDISP_D3DTOP_SELECTARG1;
		texture->alphaArg1 = cached->hasAlpha ? NPDISP_D3DTA_TEXTURE : NPDISP_D3DTA_DIFFUSE;
	}
	else if (draw->textureEnvMode == NPDISP_OGL_TEXENV_DECAL) {
		texture->colorOp = cached->hasAlpha ? NPDISP_D3DTOP_BLENDTEXTUREALPHA : NPDISP_D3DTOP_SELECTARG1;
		texture->colorArg1 = NPDISP_D3DTA_TEXTURE;
		texture->colorArg2 = NPDISP_D3DTA_DIFFUSE;
		texture->alphaOp = NPDISP_D3DTOP_SELECTARG1;
		texture->alphaArg1 = NPDISP_D3DTA_DIFFUSE;
	}
	else {
		texture->colorOp = NPDISP_D3DTOP_MODULATE;
		texture->colorArg1 = NPDISP_D3DTA_TEXTURE;
		texture->colorArg2 = NPDISP_D3DTA_DIFFUSE;
		texture->alphaOp = NPDISP_D3DTOP_MODULATE;
		texture->alphaArg1 = NPDISP_D3DTA_TEXTURE;
		texture->alphaArg2 = NPDISP_D3DTA_DIFFUSE;
	}
}

static UINT32 npdisp_ogl_rasterSize(float value)
{
	UINT32 size;
	if (!(value > 0.0f)) return 1U;
	size = (UINT32)(value + 0.5f);
	if (size < 1U) size = 1U;
	if (size > 64U) size = 64U;
	return size;
}

static bool npdisp_ogl_pointWide(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const NPDISP_D3D_VERTEX* vertex, const NPDISP_D3D_RASTERSTATE* state, float pointSize)
{
	UINT32 size = npdisp_ogl_rasterSize(pointSize);
	float start = -((float)size - 1.0f) * 0.5f;
	for (UINT32 y = 0; y < size; ++y) {
		for (UINT32 x = 0; x < size; ++x) {
			NPDISP_D3D_VERTEX v = *vertex;
			v.x += start + (float)x;
			v.y += start + (float)y;
			if (!npdisp_d3d_sw_point(target, depth, &v, state)) return false;
		}
	}
	return true;
}

static bool npdisp_ogl_lineWide(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const NPDISP_D3D_VERTEX* v0, const NPDISP_D3D_VERTEX* v1, const NPDISP_D3D_RASTERSTATE* state, float lineWidth)
{
	UINT32 size = npdisp_ogl_rasterSize(lineWidth);
	float dx = v1->x - v0->x;
	float dy = v1->y - v0->y;
	float adx = dx < 0.0f ? -dx : dx;
	float ady = dy < 0.0f ? -dy : dy;
	float start = -((float)size - 1.0f) * 0.5f;
	for (UINT32 i = 0; i < size; ++i) {
		NPDISP_D3D_VERTEX a = *v0;
		NPDISP_D3D_VERTEX b = *v1;
		float off = start + (float)i;
		if (adx >= ady) { a.y += off; b.y += off; }
		else { a.x += off; b.x += off; }
		if (!npdisp_d3d_sw_line(target, depth, &a, &b, state)) return false;
	}
	return true;
}

static UINT32 npdisp_ogl_lerpColor(UINT32 a, UINT32 b, float t)
{
	UINT32 out = 0;
	for (UINT32 shift = 0; shift < 32U; shift += 8U) {
		float av = (float)((a >> shift) & 0xffU);
		float bv = (float)((b >> shift) & 0xffU);
		UINT32 v = (UINT32)(av + (bv - av) * t + 0.5f);
		if (v > 255U) v = 255U;
		out |= v << shift;
	}
	return out;
}

static void npdisp_ogl_lerpVertex(NPDISP_D3D_VERTEX* out, const NPDISP_D3D_VERTEX* a, const NPDISP_D3D_VERTEX* b, float t)
{
	*out = *a;
	out->x = a->x + (b->x - a->x) * t;
	out->y = a->y + (b->y - a->y) * t;
	out->z = a->z + (b->z - a->z) * t;
	out->rhw = a->rhw + (b->rhw - a->rhw) * t;
	out->diffuse = npdisp_ogl_lerpColor(a->diffuse, b->diffuse, t);
	out->tu = a->tu + (b->tu - a->tu) * t;
	out->tv = a->tv + (b->tv - a->tv) * t;
}

static bool npdisp_ogl_lineStyled(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const NPDISP_D3D_VERTEX* v0, const NPDISP_D3D_VERTEX* v1, const NPDISP_D3D_RASTERSTATE* state, const NPDISP_OGL_DRAW32* draw, UINT32* stippleCounter)
{
	if (!draw->lineStippleEnable) return npdisp_ogl_lineWide(target, depth, v0, v1, state, draw->lineWidth);
	UINT32 factor = draw->lineStippleFactor;
	UINT32 pattern = draw->lineStipplePattern & 0xffffU;
	float dx = v1->x - v0->x;
	float dy = v1->y - v0->y;
	float adx = dx < 0.0f ? -dx : dx;
	float ady = dy < 0.0f ? -dy : dy;
	UINT32 steps = (UINT32)((adx > ady ? adx : ady) + 0.5f);
	UINT32 counter = stippleCounter ? *stippleCounter : 0U;
	if (!factor) factor = 1U;
	if (factor > 256U) factor = 256U;
	if (!steps) steps = 1U;
	UINT32 i = 0;
	while (i < steps) {
		UINT32 bit = (counter + i) / factor;
		bool on = ((pattern >> (bit & 15U)) & 1U) != 0;
		UINT32 end = i + 1U;
		while (end < steps) {
			UINT32 nextBit = (counter + end) / factor;
			bool nextOn = ((pattern >> (nextBit & 15U)) & 1U) != 0;
			if (nextOn != on) break;
			++end;
		}
		if (on) {
			NPDISP_D3D_VERTEX a, b;
			float t0 = (float)i / (float)steps;
			float t1 = (float)end / (float)steps;
			npdisp_ogl_lerpVertex(&a, v0, v1, t0);
			npdisp_ogl_lerpVertex(&b, v0, v1, t1);
			if (!npdisp_ogl_lineWide(target, depth, &a, &b, state, draw->lineWidth)) return false;
		}
		i = end;
	}
	if (stippleCounter) *stippleCounter = counter + steps;
	return true;
}

static float npdisp_ogl_polygonArea(const NPDISP_D3D_VERTEX* a, const NPDISP_D3D_VERTEX* b, const NPDISP_D3D_VERTEX* c)
{
	return (c->x - a->x) * (b->y - a->y) - (c->y - a->y) * (b->x - a->x);
}

static bool npdisp_ogl_polygonCulled(float area, const NPDISP_D3D_RASTERSTATE* state)
{
	if (state->cullMode == NPDISP_D3DCULL_CW && area < 0.0f) return true;
	if (state->cullMode == NPDISP_D3DCULL_CCW && area > 0.0f) return true;
	return false;
}

static UINT32 npdisp_ogl_polygonMode(const NPDISP_OGL_DRAW32* draw, float area)
{
	bool front = (draw->frontFace == 0x0901U) ? (area > 0.0f) : (area < 0.0f);
	return front ? draw->polygonModeFront : draw->polygonModeBack;
}

static bool npdisp_ogl_polygonTriangle(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const NPDISP_D3D_VERTEX* a, const NPDISP_D3D_VERTEX* b, const NPDISP_D3D_VERTEX* c, const NPDISP_D3D_RASTERSTATE* state, const NPDISP_OGL_DRAW32* draw)
{
	float area = npdisp_ogl_polygonArea(a, b, c);
	UINT32 mode;
	NPDISP_D3D_RASTERSTATE noCull;
	if (area == 0.0f) return true;
	if (npdisp_ogl_polygonCulled(area, state)) return true;
	mode = npdisp_ogl_polygonMode(draw, area);
	if (mode == 0x1B02U) return npdisp_d3d_sw_triangle(target, depth, a, b, c, state);
	noCull = *state; noCull.cullMode = NPDISP_D3DCULL_NONE;
	if (mode == 0x1B01U) { UINT32 phase = 0; return npdisp_ogl_lineStyled(target, depth, a, b, &noCull, draw, &phase) && npdisp_ogl_lineStyled(target, depth, b, c, &noCull, draw, &phase) && npdisp_ogl_lineStyled(target, depth, c, a, &noCull, draw, &phase); }
	if (mode == 0x1B00U) return npdisp_ogl_pointWide(target, depth, a, &noCull, draw->pointSize) && npdisp_ogl_pointWide(target, depth, b, &noCull, draw->pointSize) && npdisp_ogl_pointWide(target, depth, c, &noCull, draw->pointSize);
	return false;
}

static bool npdisp_ogl_polygonQuad(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const NPDISP_D3D_VERTEX* a, const NPDISP_D3D_VERTEX* b, const NPDISP_D3D_VERTEX* c, const NPDISP_D3D_VERTEX* d, const NPDISP_D3D_RASTERSTATE* state, const NPDISP_OGL_DRAW32* draw)
{
	float area = npdisp_ogl_polygonArea(a, b, c);
	UINT32 mode;
	NPDISP_D3D_RASTERSTATE noCull;
	if (area == 0.0f) return true;
	if (npdisp_ogl_polygonCulled(area, state)) return true;
	mode = npdisp_ogl_polygonMode(draw, area);
	if (mode == 0x1B02U) return npdisp_d3d_sw_triangle(target, depth, a, b, c, state) && npdisp_d3d_sw_triangle(target, depth, a, c, d, state);
	noCull = *state; noCull.cullMode = NPDISP_D3DCULL_NONE;
	if (mode == 0x1B01U) { UINT32 phase = 0; return npdisp_ogl_lineStyled(target, depth, a, b, &noCull, draw, &phase) && npdisp_ogl_lineStyled(target, depth, b, c, &noCull, draw, &phase) && npdisp_ogl_lineStyled(target, depth, c, d, &noCull, draw, &phase) && npdisp_ogl_lineStyled(target, depth, d, a, &noCull, draw, &phase); }
	if (mode == 0x1B00U) return npdisp_ogl_pointWide(target, depth, a, &noCull, draw->pointSize) && npdisp_ogl_pointWide(target, depth, b, &noCull, draw->pointSize) && npdisp_ogl_pointWide(target, depth, c, &noCull, draw->pointSize) && npdisp_ogl_pointWide(target, depth, d, &noCull, draw->pointSize);
	return false;
}

static float npdisp_ogl_listClamp(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static float npdisp_ogl_listNormalize3(float *v)
{
	float len = (float)sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
	if (len > 0.0f) { v[0] /= len; v[1] /= len; v[2] /= len; }
	return len;
}

static UINT32 npdisp_ogl_listPackColor(const float *c)
{
	UINT32 a = (UINT32)(npdisp_ogl_listClamp(c[3], 0.0f, 1.0f) * 255.0f + 0.5f);
	UINT32 r = (UINT32)(npdisp_ogl_listClamp(c[0], 0.0f, 1.0f) * 255.0f + 0.5f);
	UINT32 g = (UINT32)(npdisp_ogl_listClamp(c[1], 0.0f, 1.0f) * 255.0f + 0.5f);
	UINT32 b = (UINT32)(npdisp_ogl_listClamp(c[2], 0.0f, 1.0f) * 255.0f + 0.5f);
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static void npdisp_ogl_listTransformVector(const float *m, const float *v, float *out)
{
	out[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2];
	out[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2];
	out[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2];
}

static void npdisp_ogl_listMaterial(const NPDISP_OGL_LIST_EXEC32 *data, const float *currentColor, float *ambient, float *diffuse, float *specular, float *emission)
{
	memcpy(ambient, data->materialAmbient, 4 * sizeof(float));
	memcpy(diffuse, data->materialDiffuse, 4 * sizeof(float));
	memcpy(specular, data->materialSpecular, 4 * sizeof(float));
	memcpy(emission, data->materialEmission, 4 * sizeof(float));
	if (!data->colorMaterial) return;
	switch (data->colorMaterialMode) {
	case 0x1200U: memcpy(ambient, currentColor, 4 * sizeof(float)); break;
	case 0x1201U: memcpy(diffuse, currentColor, 4 * sizeof(float)); break;
	case 0x1202U: memcpy(specular, currentColor, 4 * sizeof(float)); break;
	case 0x1600U: memcpy(emission, currentColor, 4 * sizeof(float)); break;
	case 0x1602U: memcpy(ambient, currentColor, 4 * sizeof(float)); memcpy(diffuse, currentColor, 4 * sizeof(float)); break;
	}
}

static void npdisp_ogl_listVertexColor(const NPDISP_OGL_LIST_EXEC32 *data, const float *currentColor, const float *currentNormal, const float *eye, float *color)
{
	if (!data->lighting) {
		memcpy(color, currentColor, 4 * sizeof(float));
	}
	else {
		float ambient[4], diffuse[4], specular[4], emission[4], normal[3], view[3];
		npdisp_ogl_listMaterial(data, currentColor, ambient, diffuse, specular, emission);
		npdisp_ogl_listTransformVector(data->modelview, currentNormal, normal);
		if (data->normalize) npdisp_ogl_listNormalize3(normal);
		for (int j = 0; j < 3; ++j) color[j] = emission[j] + data->lightModelAmbient[j] * ambient[j];
		color[3] = diffuse[3];
		if (data->lightModelLocalViewer) {
			view[0] = -eye[0]; view[1] = -eye[1]; view[2] = -eye[2];
			if (npdisp_ogl_listNormalize3(view) == 0.0f) { view[0] = 0.0f; view[1] = 0.0f; view[2] = 1.0f; }
		}
		else { view[0] = 0.0f; view[1] = 0.0f; view[2] = 1.0f; }
		for (int i = 0; i < 8; ++i) {
			const NPDISP_OGL_LIST_LIGHT32 &l = data->lights[i];
			float lv[3], halfv[3], dist = 1.0f, att = 1.0f, ndotl, ndoth, spot = 1.0f;
			if (!l.enabled) continue;
			for (int j = 0; j < 3; ++j) color[j] += l.ambient[j] * ambient[j];
			if (l.position[3] == 0.0f) {
				lv[0] = l.position[0]; lv[1] = l.position[1]; lv[2] = l.position[2]; npdisp_ogl_listNormalize3(lv);
			}
			else {
				float invw = 1.0f / l.position[3];
				lv[0] = l.position[0] * invw - eye[0]; lv[1] = l.position[1] * invw - eye[1]; lv[2] = l.position[2] * invw - eye[2];
				dist = npdisp_ogl_listNormalize3(lv); if (dist <= 0.0f) dist = 1.0f;
				att = 1.0f / (l.constantAttenuation + l.linearAttenuation * dist + l.quadraticAttenuation * dist * dist);
				if (l.spotCutoff != 180.0f) {
					float spotDot = -(lv[0] * l.spotDirection[0] + lv[1] * l.spotDirection[1] + lv[2] * l.spotDirection[2]);
					float cutoffCos = (float)cos((double)l.spotCutoff * 3.14159265358979323846 / 180.0);
					if (spotDot < cutoffCos) spot = 0.0f; else if (l.spotExponent != 0.0f) spot = (float)pow((double)spotDot, (double)l.spotExponent);
				}
			}
			ndotl = normal[0] * lv[0] + normal[1] * lv[1] + normal[2] * lv[2];
			if (ndotl < 0.0f) ndotl = 0.0f; if (ndotl > 1.0f) ndotl = 1.0f;
			for (int j = 0; j < 3; ++j) color[j] += att * spot * l.diffuse[j] * diffuse[j] * ndotl;
			if (ndotl > 0.0f && data->materialShininess > 0.0f) {
				halfv[0] = lv[0] + view[0]; halfv[1] = lv[1] + view[1]; halfv[2] = lv[2] + view[2]; npdisp_ogl_listNormalize3(halfv);
				ndoth = normal[0] * halfv[0] + normal[1] * halfv[1] + normal[2] * halfv[2];
				if (ndoth < 0.0f) ndoth = 0.0f; if (ndoth > 1.0f) ndoth = 1.0f;
				ndoth = (float)pow((double)ndoth, (double)data->materialShininess);
				for (int j = 0; j < 3; ++j) color[j] += att * spot * l.specular[j] * specular[j] * ndoth;
			}
		}
		for (int j = 0; j < 4; ++j) color[j] = npdisp_ogl_listClamp(color[j], 0.0f, 1.0f);
	}
	if (data->fog) {
		float z = eye[2] < 0.0f ? -eye[2] : eye[2]; float f;
		if (data->fogMode == 0x2601U) { float d = data->fogEnd - data->fogStart; f = (d == 0.0f) ? 0.0f : (data->fogEnd - z) / d; }
		else if (data->fogMode == 0x0800U) f = (float)exp(-(double)data->fogDensity * z);
		else { float d = data->fogDensity * z; f = (float)exp(-(double)d * d); }
		f = npdisp_ogl_listClamp(f, 0.0f, 1.0f);
		for (int j = 0; j < 3; ++j) color[j] = f * color[j] + (1.0f - f) * data->fogColor[j];
	}
}

static void npdisp_ogl_listTransformVertex(const NPDISP_OGL_LIST_EXEC32 *data, const float *currentColor, const float *currentNormal, const float *currentTexCoord, const float *obj, NPDISP_OGL_VERTEX32 *out)
{
	float eye[4], clip[4], srcTex[4], tex[4], vertexColor[4], invW, ndcX, ndcY, ndcZ;
	memset(out, 0, sizeof(*out));
	for (int row = 0; row < 4; ++row) { eye[row] = 0.0f; for (int k = 0; k < 4; ++k) eye[row] += data->modelview[k * 4 + row] * obj[k]; }
	for (int row = 0; row < 4; ++row) { clip[row] = 0.0f; for (int k = 0; k < 4; ++k) clip[row] += data->projection[k * 4 + row] * eye[k]; }
	invW = (clip[3] == 0.0f) ? 1.0f : 1.0f / clip[3]; ndcX = clip[0] * invW; ndcY = clip[1] * invW; ndcZ = clip[2] * invW;
	out->x = (float)data->viewport[0] + (ndcX + 1.0f) * (float)data->viewport[2] * 0.5f;
	out->y = (float)data->draw.height - ((float)data->viewport[1] + (ndcY + 1.0f) * (float)data->viewport[3] * 0.5f);
	out->z = data->depthNear + (ndcZ + 1.0f) * (data->depthFar - data->depthNear) * 0.5f;
	out->rhw = invW;
	memcpy(out->clip, clip, sizeof(out->clip));
	for (int i = 0; i < 6; ++i) out->clipDistance[i] = 1.0f;
	npdisp_ogl_listVertexColor(data, currentColor, currentNormal, eye, vertexColor); out->diffuse = npdisp_ogl_listPackColor(vertexColor);
	memcpy(srcTex, currentTexCoord, 4 * sizeof(float));
	for (int k = 0; k < 4; ++k) if (data->texGenEnabled[k]) {
		if (data->texGenMode[k] == 0x2401U) srcTex[k] = obj[0] * data->texGenObjectPlane[k * 4] + obj[1] * data->texGenObjectPlane[k * 4 + 1] + obj[2] * data->texGenObjectPlane[k * 4 + 2] + obj[3] * data->texGenObjectPlane[k * 4 + 3];
		else if (data->texGenMode[k] == 0x2400U) srcTex[k] = eye[0] * data->texGenEyePlane[k * 4] + eye[1] * data->texGenEyePlane[k * 4 + 1] + eye[2] * data->texGenEyePlane[k * 4 + 2] + eye[3] * data->texGenEyePlane[k * 4 + 3];
		else if (data->texGenMode[k] == 0x2402U && k < 2) {
			float n[3], refl[3], flen, dotnr, m;
			npdisp_ogl_listTransformVector(data->modelview, currentNormal, n); flen = npdisp_ogl_listNormalize3(n); (void)flen;
			refl[0] = eye[0]; refl[1] = eye[1]; refl[2] = eye[2]; npdisp_ogl_listNormalize3(refl);
			dotnr = n[0] * refl[0] + n[1] * refl[1] + n[2] * refl[2]; refl[0] -= 2.0f * n[0] * dotnr; refl[1] -= 2.0f * n[1] * dotnr; refl[2] -= 2.0f * n[2] * dotnr;
			m = 2.0f * (float)sqrt(refl[0] * refl[0] + refl[1] * refl[1] + (refl[2] + 1.0f) * (refl[2] + 1.0f)); srcTex[k] = (m != 0.0f ? ((k == 0 ? refl[0] : refl[1]) / m) : 0.0f) + 0.5f;
		}
	}
	for (int row = 0; row < 4; ++row) { tex[row] = 0.0f; for (int k = 0; k < 4; ++k) tex[row] += data->texture[k * 4 + row] * srcTex[k]; }
	memcpy(out->texcoord, tex, sizeof(out->texcoord));
	{ float q = tex[3]; if (q == 0.0f) q = 1.0f; out->tu = tex[0] / q; out->tv = tex[1] / q; }
}

static bool npdisp_ogl_prepareMaskedTarget(const NPDISP_D3D_SW_TARGET* target, UINT32 disableMask, std::vector<UINT8>* storage, NPDISP_D3D_SW_TARGET* renderTarget)
{
	UINT32 bytesPerPixel;
	size_t rowBytes;
	if (!target || !target->pixels || !storage || !renderTarget) return false;
	*renderTarget = *target;
	if (!(disableMask & 7U)) return true;
	bytesPerPixel = npdisp_ogl_pixelBytes(target->bpp);
	if (!bytesPerPixel) return false;
	rowBytes = (size_t)target->width * bytesPerPixel;
	if (!rowBytes || target->height > (size_t)-1 / rowBytes) return false;
	storage->resize(rowBytes * target->height);
	for (UINT32 y = 0; y < target->height; ++y) memcpy(&(*storage)[y * rowBytes], target->pixels + y * target->pitch, rowBytes);
	renderTarget->pixels = storage->empty() ? NULL : &(*storage)[0];
	renderTarget->pitch = (SINT32)rowBytes;
	return renderTarget->pixels != NULL;
}

static bool npdisp_ogl_finishMaskedTarget(NPDISP_D3D_SW_TARGET* target, const NPDISP_D3D_SW_TARGET* renderTarget, UINT32 disableMask)
{
	UINT32 bytesPerPixel;
	if (!target || !renderTarget || !target->pixels || !renderTarget->pixels) return false;
	if (!(disableMask & 7U)) return true;
	if ((disableMask & 7U) == 7U) return true;
	bytesPerPixel = npdisp_ogl_pixelBytes(target->bpp);
	if (!bytesPerPixel) return false;
	for (UINT32 y = 0; y < target->height; ++y) {
		UINT8* dp = target->pixels + y * target->pitch;
		const UINT8* sp = renderTarget->pixels + y * renderTarget->pitch;
		for (UINT32 x = 0; x < target->width; ++x, dp += bytesPerPixel, sp += bytesPerPixel) {
			UINT32 color = npdisp_ogl_readNativePixel(sp, target->bpp);
			UINT32 oldColor = npdisp_ogl_readNativePixel(dp, target->bpp);
			if (disableMask & 1U) color = (color & 0xff00ffffUL) | (oldColor & 0x00ff0000UL);
			if (disableMask & 2U) color = (color & 0xffff00ffUL) | (oldColor & 0x0000ff00UL);
			if (disableMask & 4U) color = (color & 0xffffff00UL) | (oldColor & 0x000000ffUL);
			npdisp_ogl_writeNativePixel(dp, target->bpp, color);
		}
	}
	return true;
}

typedef struct {
	NPDISP_D3D_VERTEX v;
	float clip[4];
	float texcoord[4];
	float distance[12];
} NPDISP_OGL_CLIP_VERTEX;


static void npdisp_ogl_makeClipVertex(NPDISP_OGL_CLIP_VERTEX* dst, const NPDISP_OGL_VERTEX32* src, SINT32 clipLeft, SINT32 clipTop)
{
	npdisp_ogl_makeVertex(&dst->v, src);
	dst->v.x -= (float)clipLeft;
	dst->v.y -= (float)clipTop;
	memcpy(dst->clip, src->clip, sizeof(dst->clip));
	memcpy(dst->texcoord, src->texcoord, sizeof(dst->texcoord));
	dst->distance[0] = src->clip[0] + src->clip[3];
	dst->distance[1] = src->clip[3] - src->clip[0];
	dst->distance[2] = src->clip[1] + src->clip[3];
	dst->distance[3] = src->clip[3] - src->clip[1];
	dst->distance[4] = src->clip[2] + src->clip[3];
	dst->distance[5] = src->clip[3] - src->clip[2];
	for (int i = 0; i < 6; ++i) dst->distance[6 + i] = src->clipDistance[i];
}

static UINT32 npdisp_ogl_clipOutcode(const NPDISP_OGL_VERTEX32* src, UINT32 userMask)
{
	UINT32 code = 0;
	if (!src) return 0xfffU;
	if (src->clip[0] < -src->clip[3]) code |= 1U << 0;
	if (src->clip[0] >  src->clip[3]) code |= 1U << 1;
	if (src->clip[1] < -src->clip[3]) code |= 1U << 2;
	if (src->clip[1] >  src->clip[3]) code |= 1U << 3;
	if (src->clip[2] < -src->clip[3]) code |= 1U << 4;
	if (src->clip[2] >  src->clip[3]) code |= 1U << 5;
	for (int i = 0; i < 6; ++i) if ((userMask & (1U << i)) && src->clipDistance[i] < 0.0f) code |= 1U << (6 + i);
	return code;
}

static void npdisp_ogl_updateClipVertex(NPDISP_OGL_CLIP_VERTEX* v, const NPDISP_OGL_DRAW32* draw, SINT32 clipLeft, SINT32 clipTop)
{
	float w = v->clip[3];
	float invw = (w == 0.0f) ? 1.0f : 1.0f / w;
	float ndcX = v->clip[0] * invw;
	float ndcY = v->clip[1] * invw;
	float ndcZ = v->clip[2] * invw;
	float q = v->texcoord[3];
	if (q == 0.0f) q = 1.0f;
	v->v.x = (float)draw->viewportX + (ndcX + 1.0f) * (float)draw->viewportWidth * 0.5f - (float)clipLeft;
	v->v.y = (float)draw->height - ((float)draw->viewportY + (ndcY + 1.0f) * (float)draw->viewportHeight * 0.5f) - (float)clipTop;
	v->v.z = draw->depthNear + (ndcZ + 1.0f) * (draw->depthFar - draw->depthNear) * 0.5f;
	v->v.rhw = invw;
	v->v.tu = v->texcoord[0] / q;
	v->v.tv = v->texcoord[1] / q;
}

static NPDISP_OGL_CLIP_VERTEX npdisp_ogl_clipLerp(const NPDISP_OGL_CLIP_VERTEX& a, const NPDISP_OGL_CLIP_VERTEX& b, float t, const NPDISP_OGL_DRAW32* draw, SINT32 clipLeft, SINT32 clipTop)
{
	NPDISP_OGL_CLIP_VERTEX r;
	memset(&r, 0, sizeof(r));
	for (int i = 0; i < 4; ++i) { r.clip[i] = a.clip[i] + (b.clip[i] - a.clip[i]) * t; r.texcoord[i] = a.texcoord[i] + (b.texcoord[i] - a.texcoord[i]) * t; }
	for (int i = 0; i < 12; ++i) r.distance[i] = a.distance[i] + (b.distance[i] - a.distance[i]) * t;
	r.v.diffuse = npdisp_ogl_lerpColor(a.v.diffuse, b.v.diffuse, t);
	npdisp_ogl_updateClipVertex(&r, draw, clipLeft, clipTop);
	return r;
}

static bool npdisp_ogl_clipLine(NPDISP_OGL_CLIP_VERTEX* a, NPDISP_OGL_CLIP_VERTEX* b, UINT32 mask, const NPDISP_OGL_DRAW32* draw, SINT32 clipLeft, SINT32 clipTop)
{
	for (int plane = 0; plane < 12; ++plane) if (mask & (1U << plane)) {
		float da = a->distance[plane], db = b->distance[plane];
		if (da < 0.0f && db < 0.0f) return false;
		if ((da < 0.0f) != (db < 0.0f)) {
			float den = da - db;
			float t = (den == 0.0f) ? 0.0f : da / den;
			NPDISP_OGL_CLIP_VERTEX v = npdisp_ogl_clipLerp(*a, *b, t, draw, clipLeft, clipTop);
			if (da < 0.0f) *a = v; else *b = v;
		}
	}
	return true;
}

static void npdisp_ogl_clipPolygon(std::vector<NPDISP_OGL_CLIP_VERTEX>& polygon, UINT32 mask, const NPDISP_OGL_DRAW32* draw, SINT32 clipLeft, SINT32 clipTop)
{
	std::vector<NPDISP_OGL_CLIP_VERTEX> output;
	for (int plane = 0; plane < 12 && !polygon.empty(); ++plane) if (mask & (1U << plane)) {
		output.clear();
		NPDISP_OGL_CLIP_VERTEX prev = polygon[polygon.size() - 1U];
		bool prevIn = prev.distance[plane] >= 0.0f;
		for (size_t i = 0; i < polygon.size(); ++i) {
			NPDISP_OGL_CLIP_VERTEX cur = polygon[i];
			bool curIn = cur.distance[plane] >= 0.0f;
			if (curIn != prevIn) {
				float den = prev.distance[plane] - cur.distance[plane];
				float t = (den == 0.0f) ? 0.0f : prev.distance[plane] / den;
				output.push_back(npdisp_ogl_clipLerp(prev, cur, t, draw, clipLeft, clipTop));
			}
			if (curIn) output.push_back(cur);
			prev = cur; prevIn = curIn;
		}
		polygon.swap(output);
	}
}

static bool npdisp_ogl_renderPrimitives(NPDISP_D3D_SW_TARGET* renderTarget, NPDISP_D3D_SW_DEPTH_TARGET* depthPtr, const std::vector<NPDISP_D3D_VERTEX>& vertices, const NPDISP_D3D_RASTERSTATE* state, const NPDISP_OGL_DRAW32& draw)
{
	bool ok = true;
	switch (draw.primitive) {
	case 0: // GL_POINTS
		for (UINT32 i = 0; i < draw.vertexCount && ok; ++i) ok = npdisp_ogl_pointWide(renderTarget, depthPtr, &vertices[i], state, draw.pointSize);
		break;
	case 1: // GL_LINES
		for (UINT32 i = 0; i + 1U < draw.vertexCount && ok; i += 2U) { UINT32 phase = 0; ok = npdisp_ogl_lineStyled(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], state, &draw, &phase); }
		break;
	case 2: // GL_LINE_LOOP
		{ UINT32 phase = 0; for (UINT32 i = 0; i + 1U < draw.vertexCount && ok; ++i) ok = npdisp_ogl_lineStyled(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], state, &draw, &phase);
		if (ok && draw.vertexCount > 1U) ok = npdisp_ogl_lineStyled(renderTarget, depthPtr, &vertices[draw.vertexCount - 1U], &vertices[0], state, &draw, &phase); }
		break;
	case 3: // GL_LINE_STRIP
		{ UINT32 phase = 0; for (UINT32 i = 0; i + 1U < draw.vertexCount && ok; ++i) ok = npdisp_ogl_lineStyled(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], state, &draw, &phase); }
		break;
	case 4: // GL_TRIANGLES
		for (UINT32 i = 0; i + 2U < draw.vertexCount && ok; i += 3U) ok = npdisp_ogl_polygonTriangle(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], &vertices[i + 2U], state, &draw);
		break;
	case 5: // GL_TRIANGLE_STRIP
		for (UINT32 i = 0; i + 2U < draw.vertexCount && ok; ++i) {
			if (i & 1U) ok = npdisp_ogl_polygonTriangle(renderTarget, depthPtr, &vertices[i + 1U], &vertices[i], &vertices[i + 2U], state, &draw);
			else ok = npdisp_ogl_polygonTriangle(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], &vertices[i + 2U], state, &draw);
		}
		break;
	case 6: // GL_TRIANGLE_FAN
		for (UINT32 i = 1; i + 1U < draw.vertexCount && ok; ++i) ok = npdisp_ogl_polygonTriangle(renderTarget, depthPtr, &vertices[0], &vertices[i], &vertices[i + 1U], state, &draw);
		break;
	case 7: // GL_QUADS
		for (UINT32 i = 0; i + 3U < draw.vertexCount && ok; i += 4U) ok = npdisp_ogl_polygonQuad(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], &vertices[i + 2U], &vertices[i + 3U], state, &draw);
		break;
	case 8: // GL_QUAD_STRIP
		for (UINT32 i = 0; i + 3U < draw.vertexCount && ok; i += 2U) ok = npdisp_ogl_polygonQuad(renderTarget, depthPtr, &vertices[i], &vertices[i + 1U], &vertices[i + 3U], &vertices[i + 2U], state, &draw);
		break;
	case 9: // GL_POLYGON
		if (draw.vertexCount >= 3U) {
			float area = npdisp_ogl_polygonArea(&vertices[0], &vertices[1], &vertices[2]);
			UINT32 mode = npdisp_ogl_polygonMode(&draw, area);
			if (!npdisp_ogl_polygonCulled(area, state)) {
				if (mode == 0x1B01U) {
					NPDISP_D3D_RASTERSTATE noCull = *state; noCull.cullMode = NPDISP_D3DCULL_NONE;
					UINT32 phase = 0; for (UINT32 i = 0; i < draw.vertexCount && ok; ++i) ok = npdisp_ogl_lineStyled(renderTarget, depthPtr, &vertices[i], &vertices[(i + 1U) % draw.vertexCount], &noCull, &draw, &phase);
				}
				else if (mode == 0x1B00U) {
					NPDISP_D3D_RASTERSTATE noCull = *state; noCull.cullMode = NPDISP_D3DCULL_NONE;
					for (UINT32 i = 0; i < draw.vertexCount && ok; ++i) ok = npdisp_ogl_pointWide(renderTarget, depthPtr, &vertices[i], &noCull, draw.pointSize);
				}
				else {
					for (UINT32 i = 1; i + 1U < draw.vertexCount && ok; ++i) ok = npdisp_d3d_sw_triangle(renderTarget, depthPtr, &vertices[0], &vertices[i], &vertices[i + 1U], state);
				}
			}
		}
		break;
	default:
		return false;
	}

	return ok;
}

static bool npdisp_ogl_renderClippedPolygon(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const std::vector<NPDISP_OGL_CLIP_VERTEX>& poly, const NPDISP_D3D_RASTERSTATE* state, const NPDISP_OGL_DRAW32& draw)
{
	if (poly.size() < 3U) return true;
	std::vector<NPDISP_D3D_VERTEX> v(poly.size());
	for (size_t i = 0; i < poly.size(); ++i) v[i] = poly[i].v;
	NPDISP_OGL_DRAW32 d = draw; d.primitive = 9U; d.vertexCount = (UINT32)v.size(); d.clipPlaneMask = 0;
	return npdisp_ogl_renderPrimitives(target, depth, v, state, d);
}


static bool npdisp_ogl_renderWithClip(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, const std::vector<NPDISP_OGL_VERTEX32>& src, const NPDISP_D3D_RASTERSTATE* state, const NPDISP_OGL_DRAW32& draw, SINT32 clipLeft, SINT32 clipTop)
{
	std::vector<NPDISP_OGL_CLIP_VERTEX> base;
	std::vector<NPDISP_OGL_CLIP_VERTEX> poly;
	UINT32 activeMask = 0x003fU | ((draw.clipPlaneMask & 0x3fU) << 6);
	UINT32 orCode = 0, andCode = activeMask;
	for (size_t i = 0; i < src.size(); ++i) { UINT32 code = npdisp_ogl_clipOutcode(&src[i], draw.clipPlaneMask); orCode |= code; andCode &= code; }
	if (!orCode) {
		++s_diagClipFast;
		std::vector<NPDISP_D3D_VERTEX> vertices(src.size());
		for (size_t i = 0; i < src.size(); ++i) { npdisp_ogl_makeVertex(&vertices[i], &src[i]); vertices[i].x -= (float)clipLeft; vertices[i].y -= (float)clipTop; }
		return npdisp_ogl_renderPrimitives(target, depth, vertices, state, draw);
	}
	if (andCode) {
		++s_diagClipReject;
		static UINT32 rejectLogCount = 0;
		if (rejectLogCount < 16U && !src.empty()) {
			TRACEOUTOGL(("NPDISPOGL clip reject ctx=%08x prim=%u verts=%u and=%03x first=(%g,%g,%g,%g)", draw.context, draw.primitive, (UINT32)src.size(), andCode, src[0].clip[0], src[0].clip[1], src[0].clip[2], src[0].clip[3]));
			++rejectLogCount;
		}
		return true;
	}
	++s_diagClipPartial;
	base.resize(src.size());
	for (size_t i = 0; i < src.size(); ++i) npdisp_ogl_makeClipVertex(&base[i], &src[i], clipLeft, clipTop);
	switch (draw.primitive) {
	case 0:
		for (size_t i = 0; i < base.size(); ++i) { bool inside = true; for (int p = 0; p < 12; ++p) if ((activeMask & (1U << p)) && base[i].distance[p] < 0.0f) { inside = false; break; } if (inside && !npdisp_ogl_pointWide(target, depth, &base[i].v, state, draw.pointSize)) return false; }
		return true;
	case 1:
		for (size_t i = 0; i + 1U < base.size(); i += 2U) { NPDISP_OGL_CLIP_VERTEX a=base[i],b=base[i+1U]; UINT32 phase=0; if (npdisp_ogl_clipLine(&a,&b,activeMask,&draw,clipLeft,clipTop) && !npdisp_ogl_lineStyled(target,depth,&a.v,&b.v,state,&draw,&phase)) return false; }
		return true;
	case 2:
	case 3:
		{ UINT32 phase=0; size_t count=base.size(); if (count<2U)return true; size_t limit=(draw.primitive==2U)?count:count-1U; for(size_t i=0;i<limit;++i){size_t j=(i+1U)%count;NPDISP_OGL_CLIP_VERTEX a=base[i],b=base[j];if(npdisp_ogl_clipLine(&a,&b,activeMask,&draw,clipLeft,clipTop)&&!npdisp_ogl_lineStyled(target,depth,&a.v,&b.v,state,&draw,&phase))return false;} return true; }
	case 4:
		for (size_t i=0;i+2U<base.size();i+=3U){poly.clear();poly.push_back(base[i]);poly.push_back(base[i+1U]);poly.push_back(base[i+2U]);npdisp_ogl_clipPolygon(poly,activeMask,&draw,clipLeft,clipTop);if(!npdisp_ogl_renderClippedPolygon(target,depth,poly,state,draw))return false;} return true;
	case 5:
		for (size_t i=0;i+2U<base.size();++i){poly.clear();if(i&1U){poly.push_back(base[i+1U]);poly.push_back(base[i]);poly.push_back(base[i+2U]);}else{poly.push_back(base[i]);poly.push_back(base[i+1U]);poly.push_back(base[i+2U]);}npdisp_ogl_clipPolygon(poly,activeMask,&draw,clipLeft,clipTop);if(!npdisp_ogl_renderClippedPolygon(target,depth,poly,state,draw))return false;} return true;
	case 6:
		for (size_t i=1;i+1U<base.size();++i){poly.clear();poly.push_back(base[0]);poly.push_back(base[i]);poly.push_back(base[i+1U]);npdisp_ogl_clipPolygon(poly,activeMask,&draw,clipLeft,clipTop);if(!npdisp_ogl_renderClippedPolygon(target,depth,poly,state,draw))return false;} return true;
	case 7:
		for(size_t i=0;i+3U<base.size();i+=4U){poly.assign(base.begin()+i,base.begin()+i+4U);npdisp_ogl_clipPolygon(poly,activeMask,&draw,clipLeft,clipTop);if(!npdisp_ogl_renderClippedPolygon(target,depth,poly,state,draw))return false;} return true;
	case 8:
		for(size_t i=0;i+3U<base.size();i+=2U){poly.clear();poly.push_back(base[i]);poly.push_back(base[i+1U]);poly.push_back(base[i+3U]);poly.push_back(base[i+2U]);npdisp_ogl_clipPolygon(poly,activeMask,&draw,clipLeft,clipTop);if(!npdisp_ogl_renderClippedPolygon(target,depth,poly,state,draw))return false;} return true;
	case 9:
		poly=base;npdisp_ogl_clipPolygon(poly,activeMask,&draw,clipLeft,clipTop);return npdisp_ogl_renderClippedPolygon(target,depth,poly,state,draw);
	default:
		return false;
	}
}

static bool npdisp_ogl_drawPrepared(const NPDISP_OGL_DRAW32 &draw, const std::vector<NPDISP_OGL_VERTEX32> &src)
{
	NPDISP_D3D_SW_TARGET target;
	NPDISP_D3D_SW_TARGET renderTarget;
	NPDISP_D3D_SW_DEPTH_TARGET depth;
	NPDISP_D3D_SW_DEPTH_TARGET *depthPtr = NULL;
	NPDISP_D3D_RASTERSTATE state;
	NPDISP_D3D_TEXTURE texture;
	SINT32 clipLeft, clipTop, clipRight, clipBottom;
	UINT32 targetBytesPerPixel;
	std::vector<UINT8> maskedColor;
	std::vector<UINT8> logicColorA;
	std::vector<UINT8> logicColorB;
	std::vector<UINT8> depthBackup;
	if (s_diagInListExec) ++s_diagListDraws; else ++s_diagDirectDraws;
	if (draw.textureEnable && draw.textureId != s_diagLastTextureId) { ++s_diagTextureSwitches; s_diagLastTextureId = draw.textureId; }
	{
		static UINT32 drawDiagCount = 0;
		if (drawDiagCount < 128U && !src.empty()) {
			UINT32 oc=npdisp_ogl_clipOutcode(&src[0],draw.clipPlaneMask);
			TRACEOUTOGL(("NPDISPOGL DRAWDIAG n=%u ctx=%08x source=%s prim=%u verts=%u rect=%d,%d %ux%u tex=%u firstClip=(%g,%g,%g,%g) oc=%03x", drawDiagCount+1U,draw.context,s_diagInListExec?"list":"direct",draw.primitive,draw.vertexCount,draw.x,draw.y,draw.width,draw.height,draw.textureEnable?draw.textureId:0U,src[0].clip[0],src[0].clip[1],src[0].clip[2],src[0].clip[3],oc));
			++drawDiagCount;
		}
	}
	if (!draw.vertexCount || draw.vertexCount > 65536U || src.size() < draw.vertexCount) return false;
	if (s_contexts.find(draw.context) == s_contexts.end() || !npdisp_ogl_getTarget(draw.context, draw.x, draw.y, draw.width, draw.height, &target)) return false;
	if (draw.depthEnable || draw.stencilEnable) {
		if (!npdisp_ogl_getDepth(draw.context, draw.width, draw.height, &depth)) return false;
		depthPtr = &depth;
	}
	if (!npdisp_ogl_getClipRect(draw.scissorEnable, draw.scissorX, draw.scissorY, draw.scissorWidth, draw.scissorHeight, draw.width, draw.height, &clipLeft, &clipTop, &clipRight, &clipBottom)) return true;
	if (draw.scissorEnable && (clipLeft != 0 || clipTop != 0 || clipRight != (SINT32)draw.width || clipBottom != (SINT32)draw.height)) {
		targetBytesPerPixel = (target.bpp + 7U) / 8U;
		if (!targetBytesPerPixel) return false;
		target.pixels += clipTop * target.pitch + clipLeft * (SINT32)targetBytesPerPixel;
		target.width = (UINT32)(clipRight - clipLeft);
		target.height = (UINT32)(clipBottom - clipTop);
		if (depthPtr) {
			depth.pixels += clipTop * depth.pitch + clipLeft * 2;
			depth.width = target.width;
			depth.height = target.height;
		}
	}
	npdisp_ogl_makeState(&state, &draw);
	state.polygonStippleEnable = draw.polygonStippleEnable ? 1U : 0U;
	state.polygonStipplePattern = draw.polygonStipple;
	state.polygonStippleOriginX = clipLeft;
	state.polygonStippleOriginY = clipTop;
	state.polygonStippleHeight = draw.height;
	if (draw.textureEnable) {
		NPDISP_OGL_CONTEXT &ctx = s_contexts[draw.context];
		std::map<UINT32, NPDISP_OGL_CACHED_TEXTURE>::iterator texIt = ctx.textures.find(draw.textureId);
		if (texIt == ctx.textures.end() || texIt->second.pixels.empty() || !texIt->second.width || !texIt->second.height) return false;
		npdisp_ogl_makeTexture(&texture, &texIt->second, &draw);
		state.textures[0] = &texture;
		{
			static UINT32 textureDrawLogCount = 0;
			if (textureDrawLogCount < 32U) {
				TRACEOUTOGL(("NPDISPOGL draw texture ctx=%08x tex=%u rev=%u %ux%u prim=%u verts=%u", draw.context, draw.textureId, texIt->second.revision, texIt->second.width, texIt->second.height, draw.primitive, draw.vertexCount));
				++textureDrawLogCount;
			}
		}
	}

	if (draw.logicOpEnable) {
		UINT32 bytesPerPixel=npdisp_ogl_pixelBytes(target.bpp), rawMask=npdisp_ogl_rawColorMask(target.bpp);
		size_t rowBytes=(size_t)target.width*bytesPerPixel;
		NPDISP_D3D_SW_TARGET targetA=target,targetB=target;
		if(!bytesPerPixel||!rowBytes||target.height>(size_t)-1/rowBytes)return false;
		logicColorA.assign(rowBytes*target.height,0x00U); logicColorB.assign(rowBytes*target.height,0xffU);
		targetA.pixels=&logicColorA[0];targetA.pitch=(SINT32)rowBytes;targetB.pixels=&logicColorB[0];targetB.pitch=(SINT32)rowBytes;
		if(depthPtr){size_t depthRowBytes=(size_t)depth.width*2U;if(!depthRowBytes||depth.height>(size_t)-1/depthRowBytes)return false;depthBackup.resize(depthRowBytes*depth.height);for(UINT32 y=0;y<depth.height;++y)memcpy(&depthBackup[y*depthRowBytes],depth.pixels+y*depth.pitch,depthRowBytes);}
		NPDISP_D3D_RASTERSTATE logicState=state;logicState.alphaBlendEnable=0;
		bool ok=npdisp_ogl_renderWithClip(&targetA,depthPtr,src,&logicState,draw,clipLeft,clipTop);
		if(!ok){if(depthPtr){size_t rb=(size_t)depth.width*2U;for(UINT32 y=0;y<depth.height;++y)memcpy(depth.pixels+y*depth.pitch,&depthBackup[y*rb],rb);}return false;}
		if(depthPtr){size_t rb=(size_t)depth.width*2U;for(UINT32 y=0;y<depth.height;++y)memcpy(depth.pixels+y*depth.pitch,&depthBackup[y*rb],rb);}
		ok=npdisp_ogl_renderWithClip(&targetB,depthPtr,src,&logicState,draw,clipLeft,clipTop);if(!ok)return false;
		for(UINT32 y=0;y<target.height;++y){UINT8*dp=target.pixels+y*target.pitch;const UINT8*ap=targetA.pixels+y*targetA.pitch;const UINT8*bp=targetB.pixels+y*targetB.pitch;for(UINT32 x=0;x<target.width;++x,dp+=bytesPerPixel,ap+=bytesPerPixel,bp+=bytesPerPixel){UINT32 ra=npdisp_ogl_readRawPixel(ap,target.bpp),rb=npdisp_ogl_readRawPixel(bp,target.bpp);if(((ra^rb)&rawMask)==0U){UINT32 old=npdisp_ogl_readRawPixel(dp,target.bpp);UINT32 value=npdisp_ogl_logicOp(draw.logicOp,ra,old,rawMask);value=npdisp_ogl_applyRawColorMask(value,old,target.bpp,draw.colorWriteDisableMask);npdisp_ogl_writeRawPixel(dp,target.bpp,value);}}}
		npdisp_ogl_markDirty(&s_contexts[draw.context], clipLeft, clipTop, clipRight, clipBottom);
		return true;
	}
	if (!npdisp_ogl_prepareMaskedTarget(&target, draw.colorWriteDisableMask, &maskedColor, &renderTarget)) return false;
	{ bool ok=npdisp_ogl_renderWithClip(&renderTarget,depthPtr,src,&state,draw,clipLeft,clipTop);if(!ok)return false; }
	if (!npdisp_ogl_finishMaskedTarget(&target, &renderTarget, draw.colorWriteDisableMask)) return false;
	npdisp_ogl_markDirty(&s_contexts[draw.context], clipLeft, clipTop, clipRight, clipBottom);
	return true;
}

static bool npdisp_ogl_draw(UINT32 lpDataAddr)
{
	NPDISP_OGL_DRAW32 draw;
	std::vector<NPDISP_OGL_VERTEX32> src;
	if (!npdisp_readLinearMemory(&draw, lpDataAddr, sizeof(draw)) || draw.size < sizeof(draw) || !draw.vertices || !draw.vertexCount || draw.vertexCount > 65536U) return false;
	src.resize(draw.vertexCount);
	if (!npdisp_readLinearMemory(&src[0], draw.vertices, (int)(draw.vertexCount * sizeof(NPDISP_OGL_VERTEX32)))) return false;
	return npdisp_ogl_drawPrepared(draw, src);
}

static bool npdisp_ogl_listUpload(const NPDISP_OGL_LIST_UPLOAD32 &data)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(data.context);
	if (it == s_contexts.end() || !data.list || data.commandCount > 262144U) return false;
	NPDISP_OGL_CACHED_LIST list;
	if (data.commandCount) {
		UINT64 bytes = (UINT64)data.commandCount * sizeof(NPDISP_OGL_LIST_COMMAND32);
		if (!data.commands || bytes > 0x7fffffffULL) return false;
		list.commands.resize(data.commandCount);
		if (!npdisp_readLinearMemory(&list.commands[0], data.commands, (int)bytes)) return false;
		for (UINT32 i = 0; i < data.commandCount; ++i) { UINT32 op = list.commands[i].op; if (!((op >= 1U && op <= 6U) || op == 14U || op == 31U)) return false; }
	}
	it->second.lists[data.list] = list;
	return true;
}

static bool npdisp_ogl_listDelete(const NPDISP_OGL_LIST_DELETE32 &data)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(data.context);
	if (it == s_contexts.end()) return false;
	for (UINT32 i = 0; i < data.range; ++i) it->second.lists.erase(data.list + i);
	return true;
}

static void npdisp_ogl_listTranslate(NPDISP_OGL_LIST_EXEC32 *state, float x, float y, float z)
{
	float *m;
	if (state->matrixMode == 0x1700U) m = state->modelview;
	else if (state->matrixMode == 0x1701U) m = state->projection;
	else if (state->matrixMode == 0x1702U) m = state->texture;
	else return;
	float t12 = m[0] * x + m[4] * y + m[8] * z + m[12];
	float t13 = m[1] * x + m[5] * y + m[9] * z + m[13];
	float t14 = m[2] * x + m[6] * y + m[10] * z + m[14];
	float t15 = m[3] * x + m[7] * y + m[11] * z + m[15];
	m[12] = t12; m[13] = t13; m[14] = t14; m[15] = t15;
}

static bool npdisp_ogl_listExec(const NPDISP_OGL_LIST_EXEC32 &data)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(data.context);
	if (data.draw.clipPlaneMask & 0x3fU) return false;
	NPDISP_OGL_LIST_EXEC32 state = data;
	std::vector<UINT32> listNames;
	std::vector<NPDISP_OGL_VERTEX32> vertices;
	float currentColor[4], currentNormal[3], currentTexCoord[4];
	bool inBegin = false;
	UINT32 primitive = 0;
	if (it == s_contexts.end() || !data.listCount || !data.lists || data.listCount > 65536U || !data.draw.width || !data.draw.height) return false;
	vertices.reserve(1024);
	listNames.resize(data.listCount);
	if (!npdisp_readLinearMemory(&listNames[0], data.lists, (int)(data.listCount * sizeof(UINT32)))) return false;
	for (UINT32 i = 0; i < data.listCount; ++i) if (it->second.lists.find(listNames[i]) == it->second.lists.end()) return false;
	memcpy(currentColor, data.currentColor, sizeof(currentColor));
	memcpy(currentNormal, data.currentNormal, sizeof(currentNormal));
	memcpy(currentTexCoord, data.currentTexCoord, sizeof(currentTexCoord));
	for (UINT32 li = 0; li < data.listCount; ++li) {
		const NPDISP_OGL_CACHED_LIST &list = it->second.lists[listNames[li]];
		for (size_t ci = 0; ci < list.commands.size(); ++ci) {
			const NPDISP_OGL_LIST_COMMAND32 &cmd = list.commands[ci];
			switch (cmd.op) {
			case 1U:
				if (inBegin || cmd.u[0] > 9U) return false;
				inBegin = true; primitive = cmd.u[0]; vertices.clear();
				break;
			case 2U:
				if (!inBegin) return false;
				if (!vertices.empty() && !(state.draw.cullMode == 0U && primitive >= 4U)) {
					NPDISP_OGL_DRAW32 draw = state.draw;
					draw.primitive = primitive;
					draw.vertexCount = (UINT32)vertices.size();
					draw.vertices = 0;
					{ bool oldDiag=s_diagInListExec; s_diagInListExec=true; bool ok=npdisp_ogl_drawPrepared(draw, vertices); s_diagInListExec=oldDiag; if(!ok) return false; }
				}
				vertices.clear(); inBegin = false;
				break;
			case 3U:
				memcpy(currentColor, cmd.f, sizeof(currentColor));
				break;
			case 4U:
				memcpy(currentNormal, cmd.f, sizeof(currentNormal));
				break;
			case 5U:
				currentTexCoord[0] = cmd.f[0]; currentTexCoord[1] = cmd.f[1]; currentTexCoord[2] = 0.0f; currentTexCoord[3] = 1.0f;
				break;
			case 6U:
				if (!inBegin || vertices.size() >= 65536U) return false;
				{
					float obj[4] = { cmd.f[0], cmd.f[1], cmd.f[2], cmd.f[3] };
					NPDISP_OGL_VERTEX32 v;
					npdisp_ogl_listTransformVertex(&state, currentColor, currentNormal, currentTexCoord, obj, &v);
					vertices.push_back(v);
				}
				break;
			case 14U:
				if (inBegin) return false;
				npdisp_ogl_listTranslate(&state, cmd.f[0], cmd.f[1], cmd.f[2]);
				break;
			case 31U:
				if (inBegin || (cmd.u[0] != 0x0900U && cmd.u[0] != 0x0901U)) return false;
				if (state.draw.frontFace != cmd.u[0]) {
					if (state.draw.cullMode == NPDISP_D3DCULL_CW) state.draw.cullMode = NPDISP_D3DCULL_CCW;
					else if (state.draw.cullMode == NPDISP_D3DCULL_CCW) state.draw.cullMode = NPDISP_D3DCULL_CW;
					state.draw.frontFace = cmd.u[0];
				}
				break;
			default:
				return false;
			}
		}
	}
	return !inBegin;
}

static const UINT8 s_palette8Bayer4[16] = {
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
};

static void npdisp_ogl_updatePalette8Map(void)
{
	UINT8 baseMap[256];
	if (s_palette8DitherMapValid && memcmp(s_palette8Snapshot, npdisp_palette_rgb256, sizeof(s_palette8Snapshot)) == 0) return;
	memcpy(s_palette8Snapshot, npdisp_palette_rgb256, sizeof(s_palette8Snapshot));
	for (UINT32 i = 0; i < 256U; ++i) {
		UINT32 r = ((i >> 5) & 7U) * 255U / 7U;
		UINT32 g = ((i >> 2) & 7U) * 255U / 7U;
		UINT32 b = (i & 3U) * 255U / 3U;
		UINT32 best = 0U;
		long bestDist = 0x7fffffffL;
		for (UINT32 j = 0; j < 256U; ++j) {
			long dr = (long)r - npdisp_palette_rgb256[j].r;
			long dg = (long)g - npdisp_palette_rgb256[j].g;
			long db = (long)b - npdisp_palette_rgb256[j].b;
			long dist = dr * dr + dg * dg + db * db;
			if (dist < bestDist) {
				bestDist = dist;
				best = j;
			}
		}
		baseMap[i] = (UINT8)best;
	}
	for (UINT32 phase = 0; phase < 16U; ++phase) {
		UINT32 threshold = s_palette8Bayer4[phase];
		for (UINT32 rgb555 = 0; rgb555 < 32768U; ++rgb555) {
			UINT32 r5 = (rgb555 >> 10) & 31U;
			UINT32 g5 = (rgb555 >> 5) & 31U;
			UINT32 b5 = rgb555 & 31U;
			UINT32 r3 = r5 >> 2;
			UINT32 g3 = g5 >> 2;
			UINT32 b2 = b5 >> 3;
			if (r3 < 7U && threshold < (r5 & 3U) * 4U) ++r3;
			if (g3 < 7U && threshold < (g5 & 3U) * 4U) ++g3;
			if (b2 < 3U && threshold < (b5 & 7U) * 2U) ++b2;
			s_palette8DitherMap[phase][rgb555] = baseMap[(r3 << 5) | (g3 << 2) | b2];
		}
	}
	s_palette8DitherMapValid = true;
}

static UINT8 npdisp_ogl_palette8Dither(const UINT8 *src, SINT32 x, SINT32 y)
{
	UINT32 rgb555 = ((UINT32)(src[2] >> 3) << 10) | ((UINT32)(src[1] >> 3) << 5) | (src[0] >> 3);
	UINT32 phase = (((UINT32)y & 3U) << 2) | ((UINT32)x & 3U);
	return s_palette8DitherMap[phase][rgb555];
}


static void npdisp_ogl_updatePalette4Map(void)
{
	if (s_palette4DitherMapValid && memcmp(s_palette4Snapshot, npdisp_palette_rgb16, sizeof(s_palette4Snapshot)) == 0) return;
	memcpy(s_palette4Snapshot, npdisp_palette_rgb16, sizeof(s_palette4Snapshot));
	for (UINT32 rgb555 = 0; rgb555 < 32768U; ++rgb555) {
		double r = (double)(((rgb555 >> 10) & 31U) * 255U) / 31.0;
		double g = (double)(((rgb555 >> 5) & 31U) * 255U) / 31.0;
		double b = (double)((rgb555 & 31U) * 255U) / 31.0;
		UINT32 a = 0U, bidx = 0U;
		double bestDist = 1.0e30;
		for (UINT32 i = 0; i < 16U; ++i) {
			double dr = r - npdisp_palette_rgb16[i].r;
			double dg = g - npdisp_palette_rgb16[i].g;
			double db = b - npdisp_palette_rgb16[i].b;
			double dist = dr * dr + dg * dg + db * db;
			if (dist < bestDist) { bestDist = dist; a = i; }
		}
		bidx = a;
		double bestMix = 0.0;
		double bestSegmentError = bestDist;
		for (UINT32 i = 0; i < 16U; ++i) {
			if (i == a) continue;
			double ar = npdisp_palette_rgb16[a].r, ag = npdisp_palette_rgb16[a].g, ab = npdisp_palette_rgb16[a].b;
			double dr = (double)npdisp_palette_rgb16[i].r - ar;
			double dg = (double)npdisp_palette_rgb16[i].g - ag;
			double db = (double)npdisp_palette_rgb16[i].b - ab;
			double len2 = dr * dr + dg * dg + db * db;
			if (!(len2 > 0.0)) continue;
			double mix = ((r - ar) * dr + (g - ag) * dg + (b - ab) * db) / len2;
			if (mix < 0.0) mix = 0.0; else if (mix > 1.0) mix = 1.0;
			double er = r - (ar + dr * mix);
			double eg = g - (ag + dg * mix);
			double eb = b - (ab + db * mix);
			double err = er * er + eg * eg + eb * eb;
			if (err < bestSegmentError) { bestSegmentError = err; bidx = i; bestMix = mix; }
		}
		for (UINT32 phase = 0; phase < 16U; ++phase) {
			double threshold = ((double)s_palette8Bayer4[phase] * 2.0 + 1.0) / 32.0;
			s_palette4DitherMap[phase][rgb555] = (bidx != a && threshold < bestMix) ? (UINT8)bidx : (UINT8)a;
		}
	}
	s_palette4DitherMapValid = true;
}

static UINT8 npdisp_ogl_palette4Dither(const UINT8 *src, SINT32 x, SINT32 y)
{
	UINT32 rgb555 = ((UINT32)(src[2] >> 3) << 10) | ((UINT32)(src[1] >> 3) << 5) | (src[0] >> 3);
	UINT32 phase = (((UINT32)y & 3U) << 2) | ((UINT32)x & 3U);
	return s_palette4DitherMap[phase][rgb555];
}

static const UINT8 s_palette1Bayer8[64] = {
	0,48,12,60,3,51,15,63,
	32,16,44,28,35,19,47,31,
	8,56,4,52,11,59,7,55,
	40,24,36,20,43,27,39,23,
	2,50,14,62,1,49,13,61,
	34,18,46,30,33,17,45,29,
	10,58,6,54,9,57,5,53,
	42,26,38,22,41,25,37,21
};

static void npdisp_ogl_updatePalette1Map(void)
{
	if (s_palette1LumaMapValid) return;
	for (UINT32 rgb555 = 0; rgb555 < 32768U; ++rgb555) {
		UINT32 r = ((rgb555 >> 10) & 31U) * 255U / 31U;
		UINT32 g = ((rgb555 >> 5) & 31U) * 255U / 31U;
		UINT32 b = (rgb555 & 31U) * 255U / 31U;
		s_palette1LumaMap[rgb555] = (UINT8)((r * 77U + g * 150U + b * 29U + 128U) >> 8);
	}
	s_palette1LumaMapValid = true;
}

static UINT8 npdisp_ogl_palette1Dither(const UINT8 *src, SINT32 x, SINT32 y)
{
	UINT32 rgb555 = ((UINT32)(src[2] >> 3) << 10) | ((UINT32)(src[1] >> 3) << 5) | (src[0] >> 3);
	UINT32 phase = (((UINT32)y & 7U) << 3) | ((UINT32)x & 7U);
	UINT32 threshold = ((UINT32)s_palette1Bayer8[phase] * 256U + 128U) >> 6;
	return s_palette1LumaMap[rgb555] > threshold ? 1U : 0U;
}

static UINT32 npdisp_ogl_paletteColor(UINT8 index, UINT32 bpp)
{
	const NPDISP_RGB3 *rgb;
	if (bpp == 1U) rgb = &npdisp_palette_rgb2[index & 1U];
	else if (bpp == 4U) rgb = &npdisp_palette_rgb16[index & 15U];
	else rgb = &npdisp_palette_rgb256[index];
	return 0xff000000UL | ((UINT32)rgb->r << 16) | ((UINT32)rgb->g << 8) | rgb->b;
}

static UINT8 npdisp_ogl_readIndexedPixel(const UINT8 *row, UINT32 x, UINT32 bpp)
{
	if (bpp == 8U) return row[x];
	if (bpp == 4U) { UINT8 v = row[x >> 1]; return (x & 1U) ? (v & 0x0fU) : (v >> 4); }
	if (bpp == 1U) return (row[x >> 3] & (UINT8)(0x80U >> (x & 7U))) ? 1U : 0U;
	return 0U;
}

static void npdisp_ogl_writeIndexedPixel(UINT8 *row, UINT32 x, UINT32 bpp, UINT8 index)
{
	if (bpp == 8U) row[x] = index;
	else if (bpp == 4U) {
		UINT8 *p = row + (x >> 1); index &= 0x0fU;
		if (x & 1U) *p = (UINT8)((*p & 0xf0U) | index);
		else *p = (UINT8)((*p & 0x0fU) | (index << 4));
	}
	else if (bpp == 1U) {
		UINT8 *p = row + (x >> 3); UINT8 mask = (UINT8)(0x80U >> (x & 7U));
		if (index & 1U) *p |= mask; else *p &= (UINT8)~mask;
	}
}

#if NPDISP_OGL_PERF_DIAG
static bool npdisp_ogl_perfNow(LONGLONG *value)
{
	LARGE_INTEGER counter, frequency;
	if (!value) return false;
	if (!s_perfFrequency) {
		if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) return false;
		s_perfFrequency = frequency.QuadPart;
	}
	if (!QueryPerformanceCounter(&counter)) return false;
	*value = counter.QuadPart;
	return true;
}

static UINT32 npdisp_ogl_perfUs(LONGLONG start, LONGLONG end)
{
	if (!s_perfFrequency || end <= start) return 0U;
	UINT64 us = (UINT64)(end - start) * 1000000ULL / (UINT64)s_perfFrequency;
	return us > 0xffffffffULL ? 0xffffffffU : (UINT32)us;
}
#endif

static UINT32 npdisp_ogl_pixelBytes(UINT32 bpp)
{
	if (bpp == 15U || bpp == 16U) return 2U;
	if (bpp == 24U) return 3U;
	if (bpp == 32U) return 4U;
	return 0U;
}

static UINT32 npdisp_ogl_readNativePixel(const UINT8 *p, UINT32 bpp)
{
	UINT32 r, g, b;
	if (!p) return 0xff000000UL;
	if (bpp == 15U) {
		UINT16 v = (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
		r = ((v >> 10) & 0x1fU) * 255U / 31U; g = ((v >> 5) & 0x1fU) * 255U / 31U; b = (v & 0x1fU) * 255U / 31U;
	}
	else if (bpp == 16U) {
		UINT16 v = (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
		r = ((v >> 11) & 0x1fU) * 255U / 31U; g = ((v >> 5) & 0x3fU) * 255U / 63U; b = (v & 0x1fU) * 255U / 31U;
	}
	else if (bpp == 24U || bpp == 32U) { b = p[0]; g = p[1]; r = p[2]; }
	else return 0xff000000UL;
	return 0xff000000UL | (r << 16) | (g << 8) | b;
}

static void npdisp_ogl_writeNativePixel(UINT8 *p, UINT32 bpp, UINT32 color)
{
	UINT32 r = (color >> 16) & 0xffU, g = (color >> 8) & 0xffU, b = color & 0xffU;
	if (!p) return;
	if (bpp == 15U) { UINT16 v=(UINT16)(((r>>3)<<10)|((g>>3)<<5)|(b>>3)); p[0]=(UINT8)v; p[1]=(UINT8)(v>>8); }
	else if (bpp == 16U) { UINT16 v=(UINT16)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); p[0]=(UINT8)v; p[1]=(UINT8)(v>>8); }
	else if (bpp == 24U) { p[0]=(UINT8)b; p[1]=(UINT8)g; p[2]=(UINT8)r; }
	else if (bpp == 32U) { p[0]=(UINT8)b; p[1]=(UINT8)g; p[2]=(UINT8)r; p[3]=0; }
}

static UINT32 npdisp_ogl_readRawPixel(const UINT8 *p, UINT32 bpp)
{
	if (!p) return 0;
	if (bpp == 15U || bpp == 16U) return (UINT32)p[0] | ((UINT32)p[1] << 8);
	if (bpp == 24U) return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16);
	if (bpp == 32U) return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
	return 0;
}

static void npdisp_ogl_writeRawPixel(UINT8 *p, UINT32 bpp, UINT32 value)
{
	if (!p) return;
	p[0]=(UINT8)value;
	if (bpp >= 15U) p[1]=(UINT8)(value>>8);
	if (bpp >= 24U) p[2]=(UINT8)(value>>16);
	if (bpp == 32U) p[3]=(UINT8)(value>>24);
}

static UINT32 npdisp_ogl_rawColorMask(UINT32 bpp)
{
	if (bpp == 15U) return 0x00007fffU;
	if (bpp == 16U) return 0x0000ffffU;
	if (bpp == 24U || bpp == 32U) return 0x00ffffffU;
	return 0;
}

static UINT32 npdisp_ogl_rawComponentMask(UINT32 bpp, UINT32 component)
{
	if (bpp == 15U) return component==0U?0x7c00U:(component==1U?0x03e0U:0x001fU);
	if (bpp == 16U) return component==0U?0xf800U:(component==1U?0x07e0U:0x001fU);
	if (bpp == 24U || bpp == 32U) return component==0U?0x00ff0000U:(component==1U?0x0000ff00U:0x000000ffU);
	return 0;
}

static UINT32 npdisp_ogl_logicOp(UINT32 op, UINT32 source, UINT32 dest, UINT32 mask)
{
	UINT32 value;
	source &= mask; dest &= mask;
	switch (op) {
	case 0x1500U: value=0; break;
	case 0x1501U: value=source&dest; break;
	case 0x1502U: value=source&~dest; break;
	case 0x1503U: value=source; break;
	case 0x1504U: value=~source&dest; break;
	case 0x1505U: value=dest; break;
	case 0x1506U: value=source^dest; break;
	case 0x1507U: value=source|dest; break;
	case 0x1508U: value=~(source|dest); break;
	case 0x1509U: value=~(source^dest); break;
	case 0x150aU: value=~dest; break;
	case 0x150bU: value=source|~dest; break;
	case 0x150cU: value=~source; break;
	case 0x150dU: value=~source|dest; break;
	case 0x150eU: value=~(source&dest); break;
	case 0x150fU: value=mask; break;
	default: value=source; break;
	}
	return value & mask;
}

static UINT32 npdisp_ogl_applyRawColorMask(UINT32 value, UINT32 oldValue, UINT32 bpp, UINT32 disableMask)
{
	UINT32 colorMask=npdisp_ogl_rawColorMask(bpp);
	value=(value&colorMask)|(oldValue&~colorMask);
	for(UINT32 c=0;c<3U;++c)if(disableMask&(1U<<c)){UINT32 m=npdisp_ogl_rawComponentMask(bpp,c);value=(value&~m)|(oldValue&m);}
	return value;
}

static UINT32 npdisp_ogl_colorToRaw(UINT32 color, UINT32 bpp)
{
	UINT8 p[4]={0,0,0,0}; npdisp_ogl_writeNativePixel(p,bpp,color); return npdisp_ogl_readRawPixel(p,bpp);
}

static bool npdisp_ogl_readPixels32(const NPDISP_OGL_PIXELS32 *data, std::vector<UINT8> *out)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(data->context);
	UINT32 backBytesPerPixel;
	UINT32 frontBytesPerPixel;
	if (!data || !out || it == s_contexts.end() || !data->width || !data->height) return false;
	NPDISP_OGL_CONTEXT &ctx = it->second;
	backBytesPerPixel = npdisp_ogl_pixelBytes(ctx.bpp);
	frontBytesPerPixel = (npdisp.bpp <= 8U) ? 0U : npdisp_ogl_pixelBytes(npdisp.bpp);
	if (!backBytesPerPixel || (npdisp.bpp > 8U && !frontBytesPerPixel)) return false;
	if ((UINT64)data->width * data->height * 4U > 0x7fffffffULL) return false;
	out->assign((size_t)data->width * data->height * 4U, 0U);
	for (UINT32 sy = 0; sy < data->height; ++sy) {
		SINT32 wy = data->y + (SINT32)sy;
		for (UINT32 sx = 0; sx < data->width; ++sx) {
			SINT32 wx = data->x + (SINT32)sx;
			UINT32 color = 0xff000000UL;
			if (wx >= 0 && wy >= 0 && wx < (SINT32)ctx.width && wy < (SINT32)ctx.height) {
				UINT32 topY = ctx.height - 1U - (UINT32)wy;
				if (data->buffer == NPDISP_OGL_PIXEL_FRONT) {
					if (npdisp.mm_screenPtr && npdispwin.stride && ctx.x >= 0 && ctx.y >= 0) {
						const UINT8 *row = npdisp.mm_screenPtr + ((UINT32)ctx.y + topY) * npdispwin.stride;
						UINT32 screenX = (UINT32)ctx.x + (UINT32)wx;
						if (npdisp.bpp <= 8U) color = npdisp_ogl_paletteColor(npdisp_ogl_readIndexedPixel(row, screenX, npdisp.bpp), npdisp.bpp);
						else color = npdisp_ogl_readNativePixel(row + screenX * frontBytesPerPixel, npdisp.bpp);
					}
				}
				else if (!ctx.color.empty()) {
					const UINT8 *src = &ctx.color[((size_t)topY * ctx.width + (UINT32)wx) * backBytesPerPixel];
					color = npdisp_ogl_readNativePixel(src, ctx.bpp);
				}
			}
			UINT8 *d = &(*out)[((size_t)sy * data->width + sx) * 4U];
			d[0]=(UINT8)color; d[1]=(UINT8)(color>>8); d[2]=(UINT8)(color>>16); d[3]=0xffU;
		}
	}
	return true;
}

static bool npdisp_ogl_pixelInScissor(const NPDISP_OGL_PIXELS32 *data, SINT32 x, SINT32 y)
{
	if (!data->scissorEnable) return true;
	return x >= data->scissorX && y >= data->scissorY && x < data->scissorX + (SINT32)data->scissorWidth && y < data->scissorY + (SINT32)data->scissorHeight;
}

static bool npdisp_ogl_drawPixels32(const NPDISP_OGL_PIXELS32 *data, const UINT8 *pixels)
{
	std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it = s_contexts.find(data->context);
	UINT32 bytesPerPixel;
	if (!data || !pixels || it == s_contexts.end() || !data->width || !data->height || data->zoomX == 0.0f || data->zoomY == 0.0f) return true;
	NPDISP_OGL_CONTEXT &ctx = it->second; bytesPerPixel=npdisp_ogl_pixelBytes(ctx.bpp); if(!bytesPerPixel||ctx.color.empty())return false;
	for (UINT32 sy=0; sy<data->height; ++sy) for (UINT32 sx=0; sx<data->width; ++sx) {
		const UINT8 *sp=pixels+((size_t)sy*data->width+sx)*4U;
		UINT32 color=0xff000000UL|((UINT32)sp[2]<<16)|((UINT32)sp[1]<<8)|sp[0];
		float fx0=(float)data->x+(float)sx*data->zoomX, fx1=(float)data->x+(float)(sx+1U)*data->zoomX;
		float fy0=(float)data->y+(float)sy*data->zoomY, fy1=(float)data->y+(float)(sy+1U)*data->zoomY;
		SINT32 x0=(SINT32)(fx0<fx1?fx0:fx1), x1=(SINT32)(fx0>fx1?fx0:fx1); SINT32 y0=(SINT32)(fy0<fy1?fy0:fy1), y1=(SINT32)(fy0>fy1?fy0:fy1);
		if(x1==x0)x1=x0+1; if(y1==y0)y1=y0+1;
		for(SINT32 wy=y0;wy<y1;++wy)for(SINT32 wx=x0;wx<x1;++wx){if(wx<0||wy<0||wx>=(SINT32)ctx.width||wy>=(SINT32)ctx.height||!npdisp_ogl_pixelInScissor(data,wx,wy))continue;UINT32 topY=ctx.height-1U-(UINT32)wy;UINT8*dp=&ctx.color[((size_t)topY*ctx.width+(UINT32)wx)*bytesPerPixel];UINT32 oldRaw=npdisp_ogl_readRawPixel(dp,ctx.bpp),srcRaw=npdisp_ogl_colorToRaw(color,ctx.bpp),value=srcRaw;if(data->logicOpEnable)value=npdisp_ogl_logicOp(data->logicOp,srcRaw,oldRaw,npdisp_ogl_rawColorMask(ctx.bpp));value=npdisp_ogl_applyRawColorMask(value,oldRaw,ctx.bpp,data->colorWriteDisableMask);npdisp_ogl_writeRawPixel(dp,ctx.bpp,value);}
	}
	npdisp_ogl_markDirty(&ctx, 0, 0, (SINT32)ctx.width, (SINT32)ctx.height);
	return true;
}

static bool npdisp_ogl_clearColorMasked(NPDISP_D3D_SW_TARGET* target, SINT32 left, SINT32 top, SINT32 right, SINT32 bottom, UINT32 color, UINT32 disableMask)
{
	UINT32 bytesPerPixel;
	if (!target || !target->pixels) return false;
	bytesPerPixel = npdisp_ogl_pixelBytes(target->bpp);
	if (!bytesPerPixel) return false;
	if (!(disableMask & 7U) && target->bpp == 32U) {
		if (left < 0) left = 0;
		if (top < 0) top = 0;
		if (right > (SINT32)target->width) right = (SINT32)target->width;
		if (bottom > (SINT32)target->height) bottom = (SINT32)target->height;
		if (left >= right || top >= bottom) return true;
		UINT32 raw = color & 0x00ffffffUL;
		for (SINT32 y = top; y < bottom; ++y) {
			UINT32* p = (UINT32*)(target->pixels + (size_t)y * (size_t)target->pitch) + left;
			for (SINT32 x = left; x < right; ++x) *p++ = raw;
		}
		return true;
	}
	if (!(disableMask & 7U)) return npdisp_d3d_sw_clear(target, left, top, right, bottom, color);
	for (SINT32 y = top; y < bottom; ++y) {
		UINT8* p = target->pixels + y * target->pitch + left * bytesPerPixel;
		for (SINT32 x = left; x < right; ++x, p += bytesPerPixel) {
			UINT32 value = color;
			UINT32 oldColor = npdisp_ogl_readNativePixel(p, target->bpp);
			if (disableMask & 1U) value = (value & 0xff00ffffUL) | (oldColor & 0x00ff0000UL);
			if (disableMask & 2U) value = (value & 0xffff00ffUL) | (oldColor & 0x0000ff00UL);
			if (disableMask & 4U) value = (value & 0xffffff00UL) | (oldColor & 0x000000ffUL);
			npdisp_ogl_writeNativePixel(p, target->bpp, value);
		}
	}
	return true;
}

static bool npdisp_ogl_stateWrite(UINT8 **dst, UINT32 *remain, const void *src, UINT32 size)
{
	if (!dst || !*dst || !remain || (!src && size) || size > *remain) return false;
	if (size) memcpy(*dst, src, size);
	*dst += size;
	*remain -= size;
	return true;
}

static bool npdisp_ogl_stateRead(const UINT8 **src, UINT32 *remain, void *dst, UINT32 size)
{
	if (!src || !*src || !remain || (!dst && size) || size > *remain) return false;
	if (size) memcpy(dst, *src, size);
	*src += size;
	*remain -= size;
	return true;
}

static bool npdisp_ogl_stateValidBpp(UINT32 bpp)
{
	return !bpp || bpp == 1U || bpp == 4U || bpp == 8U || bpp == 15U || bpp == 16U || bpp == 24U || bpp == 32U;
}

UINT32 npdisp_ogl_stateSize(void)
{
	UINT64 size = sizeof(NPDISP_OGL_STATE_HEADER);
	for (std::map<UINT32, NPDISP_OGL_CONTEXT>::const_iterator ci = s_contexts.begin(); ci != s_contexts.end(); ++ci) {
		const NPDISP_OGL_CONTEXT &ctx = ci->second;
		size += sizeof(NPDISP_OGL_STATE_CONTEXT) + (UINT64)ctx.color.size() + (UINT64)ctx.depth.size();
		for (std::map<UINT32, NPDISP_OGL_CACHED_TEXTURE>::const_iterator ti = ctx.textures.begin(); ti != ctx.textures.end(); ++ti)
			size += sizeof(NPDISP_OGL_STATE_TEXTURE) + (UINT64)ti->second.pixels.size();
		for (std::map<UINT32, NPDISP_OGL_CACHED_LIST>::const_iterator li = ctx.lists.begin(); li != ctx.lists.end(); ++li)
			size += sizeof(NPDISP_OGL_STATE_LIST) + (UINT64)li->second.commands.size() * sizeof(NPDISP_OGL_LIST_COMMAND32);
		if (size > NPDISP_OGL_STATE_MAX_SIZE) return 0;
	}
	return (UINT32)size;
}

bool npdisp_ogl_saveState(UINT8 *dst, UINT32 size)
{
	NPDISP_OGL_STATE_HEADER header = { 0 };
	UINT32 required = npdisp_ogl_stateSize();
	UINT32 remain;
	UINT8 *out;
	if (!dst || !required || size < required || s_contexts.size() > 0xffffffffULL) return false;
	header.magic = NPDISP_OGL_STATE_MAGIC;
	header.version = NPDISP_OGL_STATE_VERSION;
	header.contextCount = (UINT32)s_contexts.size();
	out = dst;
	remain = required;
	if (!npdisp_ogl_stateWrite(&out, &remain, &header, sizeof(header))) return false;
	for (std::map<UINT32, NPDISP_OGL_CONTEXT>::const_iterator ci = s_contexts.begin(); ci != s_contexts.end(); ++ci) {
		const NPDISP_OGL_CONTEXT &ctx = ci->second;
		NPDISP_OGL_STATE_CONTEXT saved = { 0 };
		if (!ci->first || ctx.color.size() > 0xffffffffULL || ctx.depth.size() > 0xffffffffULL ||
			ctx.textures.size() > 0xffffffffULL || ctx.lists.size() > 0xffffffffULL) return false;
		saved.context = ci->first;
		saved.width = ctx.width;
		saved.height = ctx.height;
		saved.bpp = ctx.bpp;
		saved.x = ctx.x;
		saved.y = ctx.y;
		saved.colorSize = (UINT32)ctx.color.size();
		saved.depthSize = (UINT32)ctx.depth.size();
		saved.textureCount = (UINT32)ctx.textures.size();
		saved.listCount = (UINT32)ctx.lists.size();
		if (!npdisp_ogl_stateWrite(&out, &remain, &saved, sizeof(saved)) ||
			(saved.colorSize && !npdisp_ogl_stateWrite(&out, &remain, &ctx.color[0], saved.colorSize)) ||
			(saved.depthSize && !npdisp_ogl_stateWrite(&out, &remain, &ctx.depth[0], saved.depthSize))) return false;
		for (std::map<UINT32, NPDISP_OGL_CACHED_TEXTURE>::const_iterator ti = ctx.textures.begin(); ti != ctx.textures.end(); ++ti) {
			const NPDISP_OGL_CACHED_TEXTURE &tex = ti->second;
			NPDISP_OGL_STATE_TEXTURE savedTex = { 0 };
			if (!ti->first || tex.pixels.size() > 0xffffffffULL) return false;
			savedTex.textureId = ti->first;
			savedTex.width = tex.width;
			savedTex.height = tex.height;
			savedTex.hasAlpha = tex.hasAlpha;
			savedTex.revision = tex.revision;
			savedTex.pixelSize = (UINT32)tex.pixels.size();
			if (!npdisp_ogl_stateWrite(&out, &remain, &savedTex, sizeof(savedTex)) ||
				(savedTex.pixelSize && !npdisp_ogl_stateWrite(&out, &remain, &tex.pixels[0], savedTex.pixelSize))) return false;
		}
		for (std::map<UINT32, NPDISP_OGL_CACHED_LIST>::const_iterator li = ctx.lists.begin(); li != ctx.lists.end(); ++li) {
			const NPDISP_OGL_CACHED_LIST &list = li->second;
			NPDISP_OGL_STATE_LIST savedList = { 0 };
			UINT64 commandBytes = (UINT64)list.commands.size() * sizeof(NPDISP_OGL_LIST_COMMAND32);
			if (!li->first || list.commands.size() > 0xffffffffULL || commandBytes > 0xffffffffULL) return false;
			savedList.listId = li->first;
			savedList.commandCount = (UINT32)list.commands.size();
			if (!npdisp_ogl_stateWrite(&out, &remain, &savedList, sizeof(savedList)) ||
				(commandBytes && !npdisp_ogl_stateWrite(&out, &remain, &list.commands[0], (UINT32)commandBytes))) return false;
		}
	}
	return remain == 0;
}

bool npdisp_ogl_loadState(const UINT8 *src, UINT32 size)
{
	NPDISP_OGL_STATE_HEADER header;
	std::map<UINT32, NPDISP_OGL_CONTEXT> contexts;
	UINT32 remain = size;
	const UINT8 *in = src;
	if (!src || !size || size > NPDISP_OGL_STATE_MAX_SIZE || !npdisp_ogl_stateRead(&in, &remain, &header, sizeof(header)) ||
		header.magic != NPDISP_OGL_STATE_MAGIC || header.version != NPDISP_OGL_STATE_VERSION ||
		header.contextCount > remain / sizeof(NPDISP_OGL_STATE_CONTEXT)) return false;
	for (UINT32 i = 0; i < header.contextCount; ++i) {
		NPDISP_OGL_STATE_CONTEXT saved;
		UINT64 expectedColor = 0;
		UINT64 expectedDepth = 0;
		if (!npdisp_ogl_stateRead(&in, &remain, &saved, sizeof(saved)) || !saved.context || contexts.find(saved.context) != contexts.end() ||
			!npdisp_ogl_stateValidBpp(saved.bpp)) return false;
		if (saved.width && saved.height && saved.bpp) expectedColor = (UINT64)saved.width * saved.height * ((saved.bpp + 7U) / 8U);
		if (saved.width && saved.height) expectedDepth = (UINT64)saved.width * saved.height * 2U;
		if ((saved.colorSize && (expectedColor != saved.colorSize || expectedColor > 0x7fffffffULL)) ||
			(saved.depthSize && (expectedDepth != saved.depthSize || expectedDepth > 0x7fffffffULL)) ||
			saved.colorSize > remain || saved.depthSize > remain - saved.colorSize) return false;
		NPDISP_OGL_CONTEXT &ctx = contexts[saved.context];
		ctx.width = saved.width;
		ctx.height = saved.height;
		ctx.bpp = saved.bpp;
		ctx.x = saved.x;
		ctx.y = saved.y;
		ctx.dirtyValid = 0U;
		ctx.presentClipStateValid = 0U;
		if (saved.colorSize) {
			ctx.color.resize(saved.colorSize);
			if (!npdisp_ogl_stateRead(&in, &remain, &ctx.color[0], saved.colorSize)) return false;
			npdisp_ogl_markDirty(&ctx, 0, 0, (SINT32)ctx.width, (SINT32)ctx.height);
		}
		if (saved.depthSize) {
			ctx.depth.resize(saved.depthSize);
			if (!npdisp_ogl_stateRead(&in, &remain, &ctx.depth[0], saved.depthSize)) return false;
		}
		if (saved.textureCount > remain / sizeof(NPDISP_OGL_STATE_TEXTURE)) return false;
		for (UINT32 t = 0; t < saved.textureCount; ++t) {
			NPDISP_OGL_STATE_TEXTURE savedTex;
			UINT64 expectedPixels;
			if (!npdisp_ogl_stateRead(&in, &remain, &savedTex, sizeof(savedTex)) || !savedTex.textureId ||
				ctx.textures.find(savedTex.textureId) != ctx.textures.end() || !savedTex.width || !savedTex.height ||
				savedTex.width > 2048U || savedTex.height > 2048U) return false;
			expectedPixels = (UINT64)savedTex.width * savedTex.height * 4U;
			if (expectedPixels != savedTex.pixelSize || expectedPixels > 0x7fffffffULL || savedTex.pixelSize > remain) return false;
			NPDISP_OGL_CACHED_TEXTURE &tex = ctx.textures[savedTex.textureId];
			tex.width = savedTex.width;
			tex.height = savedTex.height;
			tex.hasAlpha = savedTex.hasAlpha ? 1U : 0U;
			tex.revision = savedTex.revision;
			if (savedTex.pixelSize) {
				tex.pixels.resize(savedTex.pixelSize);
				if (!npdisp_ogl_stateRead(&in, &remain, &tex.pixels[0], savedTex.pixelSize)) return false;
			}
		}
		if (saved.listCount > remain / sizeof(NPDISP_OGL_STATE_LIST)) return false;
		for (UINT32 l = 0; l < saved.listCount; ++l) {
			NPDISP_OGL_STATE_LIST savedList;
			UINT64 commandBytes;
			if (!npdisp_ogl_stateRead(&in, &remain, &savedList, sizeof(savedList)) || !savedList.listId ||
				ctx.lists.find(savedList.listId) != ctx.lists.end() || savedList.commandCount > 262144U) return false;
			commandBytes = (UINT64)savedList.commandCount * sizeof(NPDISP_OGL_LIST_COMMAND32);
			if (commandBytes > remain || commandBytes > 0x7fffffffULL) return false;
			NPDISP_OGL_CACHED_LIST &list = ctx.lists[savedList.listId];
			if (savedList.commandCount) {
				list.commands.resize(savedList.commandCount);
				if (!npdisp_ogl_stateRead(&in, &remain, &list.commands[0], (UINT32)commandBytes)) return false;
				for (UINT32 c = 0; c < savedList.commandCount; ++c) {
					UINT32 op = list.commands[c].op;
					if (!((op >= 1U && op <= 6U) || op == 14U || op == 31U)) return false;
				}
			}
		}
	}
	if (remain) return false;
	s_contexts.swap(contexts);
	s_palette8DitherMapValid = false;
	s_palette4DitherMapValid = false;
	s_palette1LumaMapValid = false;
	s_pixelReadLogCount = s_pixelDrawLogCount = s_pixelCopyLogCount = 0;
	s_listUploadLogCount = s_listExecLogCount = 0;
	s_diagDirectDraws = s_diagListDraws = s_diagClipFast = s_diagClipPartial = s_diagClipReject = 0;
	s_diagTextureSwitches = 0;
	s_diagLastTextureId = 0xffffffffU;
	s_diagInListExec = false;
	return true;
}

static UINT32 npdisp_ogl_accountListExecTime(LONGLONG elapsedCounter, LONGLONG counterFrequency)
{
	UINT64 elapsedUs;
	UINT64 work64;
	UINT32 workClock;
	if (elapsedCounter <= 0 || counterFrequency <= 0 || !pccore.realclock) return 0;
	elapsedUs = ((UINT64)elapsedCounter / (UINT64)counterFrequency) * 1000000ULL;
	elapsedUs += (((UINT64)elapsedCounter % (UINT64)counterFrequency) * 1000000ULL) / (UINT64)counterFrequency;
	work64 = ((UINT64)pccore.realclock * elapsedUs + 500000ULL) / 1000000ULL;
	if (work64 > 0x3fffffffULL) work64 = 0x3fffffffULL;
	workClock = (UINT32)work64;
	CPU_REMCLOCK -= (SINT32)workClock;
	return workClock;
}

void npdisp_ogl_reset(void)
{
	s_contexts.clear();
	s_palette8DitherMapValid = false;
	s_palette4DitherMapValid = false;
	s_palette1LumaMapValid = false;
#if NPDISP_OGL_PERF_DIAG
	s_perfFrameCount = 0U;
	s_perfClearCalls = s_perfClearPixels = s_perfClearUs = s_perfClearMaxUs = s_perfClearMask = 0U;
	s_perfListCalls = s_perfListUs = 0U;
#endif
	s_pixelReadLogCount = s_pixelDrawLogCount = s_pixelCopyLogCount = 0;
	s_listUploadLogCount = s_listExecLogCount = 0;
}

static UINT32 npdisp_ogl_dispatchInternal(UINT32 command, UINT32 lpDataAddr)
{
	switch (command) {
	case NPDISP_OGL_CMD_QUERY:
	{
		NPDISP_OGL_QUERY32 query;
		if (!npdisp_readLinearMemory(&query, lpDataAddr, sizeof(query)) || query.size < sizeof(query)) return 0;
		query.version = NPDISP_OGL_BRIDGE_VERSION;
		query.width = npdisp.width;
		query.height = npdisp.height;
		query.bpp = npdisp.bpp;
		return npdisp_writeLinearMemory(&query, lpDataAddr, sizeof(query)) ? 1U : 0U;
	}
	case NPDISP_OGL_CMD_CONTEXT_CREATE:
	{
		NPDISP_OGL_CONTEXT32 data;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data) || !data.context) return 0;
		s_contexts[data.context] = NPDISP_OGL_CONTEXT();
		return 1;
	}
	case NPDISP_OGL_CMD_CONTEXT_DESTROY:
	{
		NPDISP_OGL_CONTEXT32 data;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data)) return 0;
		s_contexts.erase(data.context);
		return 1;
	}
	case NPDISP_OGL_CMD_CLEAR:
	{
		NPDISP_OGL_CLEAR32 data;
		NPDISP_D3D_SW_TARGET target;
		SINT32 clipLeft, clipTop, clipRight, clipBottom;
#if NPDISP_OGL_PERF_DIAG
		LONGLONG perfStart = 0, perfEnd = 0;
		bool perfTiming = npdisp_ogl_perfNow(&perfStart);
#endif
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data) || s_contexts.find(data.context) == s_contexts.end()) return 0;
		if (!npdisp_ogl_getClipRect(data.scissorEnable, data.scissorX, data.scissorY, data.scissorWidth, data.scissorHeight, data.width, data.height, &clipLeft, &clipTop, &clipRight, &clipBottom)) return 1;
		bool ok = true;
		if ((data.flags & NPDISP_OGL_CLEAR_COLOR) && (!npdisp_ogl_getTarget(data.context, data.x, data.y, data.width, data.height, &target) || !npdisp_ogl_clearColorMasked(&target, clipLeft, clipTop, clipRight, clipBottom, data.color, data.colorWriteDisableMask))) ok = false;
		if (ok && (data.flags & NPDISP_OGL_CLEAR_COLOR)) npdisp_ogl_markDirty(&s_contexts[data.context], clipLeft, clipTop, clipRight, clipBottom);
		if (ok && (data.flags & (NPDISP_OGL_CLEAR_DEPTH | NPDISP_OGL_CLEAR_STENCIL))) {
			NPDISP_D3D_SW_DEPTH_TARGET depth;
			if (!npdisp_ogl_getDepth(data.context, data.width, data.height, &depth) || !npdisp_ogl_clearDepthStencil(&depth, clipLeft, clipTop, clipRight, clipBottom, (UINT16)data.depth, data.stencil, data.flags, data.stencilBits, data.stencilWriteMask)) ok = false;
		}
#if NPDISP_OGL_PERF_DIAG
		if (perfTiming && npdisp_ogl_perfNow(&perfEnd)) {
			UINT32 us = npdisp_ogl_perfUs(perfStart, perfEnd);
			UINT64 pixels = (UINT64)(clipRight - clipLeft) * (UINT64)(clipBottom - clipTop);
			++s_perfClearCalls;
			s_perfClearPixels += pixels > 0xffffffffULL - s_perfClearPixels ? 0xffffffffU - s_perfClearPixels : (UINT32)pixels;
			s_perfClearUs += us > 0xffffffffU - s_perfClearUs ? 0xffffffffU - s_perfClearUs : us;
			if (us > s_perfClearMaxUs) s_perfClearMaxUs = us;
			s_perfClearMask |= data.flags;
		}
#endif
		return ok ? 1U : 0U;
	}
	case NPDISP_OGL_CMD_DRAW:
		return npdisp_ogl_draw(lpDataAddr) ? 1U : 0U;
	case NPDISP_OGL_CMD_TEXTURE_UPLOAD:
	{
		NPDISP_OGL_TEXTURE32 data;
		std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it;
		UINT64 bytes;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data) || !data.textureId || !data.width || !data.height) return 0;
		it = s_contexts.find(data.context);
		if (it == s_contexts.end() || data.width > 2048U || data.height > 2048U) return 0;
		std::map<UINT32, NPDISP_OGL_CACHED_TEXTURE>::iterator texIt = it->second.textures.find(data.textureId);
		if (texIt != it->second.textures.end() && texIt->second.revision == data.revision && texIt->second.width == data.width && texIt->second.height == data.height && !texIt->second.pixels.empty()) return 1;
		NPDISP_OGL_CACHED_TEXTURE &cached = it->second.textures[data.textureId];
		bytes = (UINT64)data.width * (UINT64)data.height * 4U;
		if (bytes > 0x7fffffffULL) return 0;
		cached.pixels.assign((size_t)bytes, 0U);
		if (data.pixels && !cached.pixels.empty() && !npdisp_readLinearMemory(&cached.pixels[0], data.pixels, (int)bytes)) {
			it->second.textures.erase(data.textureId);
			return 0;
		}
		cached.width = data.width;
		cached.height = data.height;
		cached.hasAlpha = data.hasAlpha ? 1U : 0U;
		cached.revision = data.revision;
		TRACEOUTOGL(("NPDISPOGL texture upload ctx=%08x tex=%u rev=%u %ux%u alpha=%u", data.context, data.textureId, data.revision, data.width, data.height, cached.hasAlpha));
		return 1;
	}
	case NPDISP_OGL_CMD_TEXTURE_DELETE:
	{
		NPDISP_OGL_TEXTURE_DELETE32 data;
		std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data) || !data.textureId) return 0;
		it = s_contexts.find(data.context);
		if (it == s_contexts.end()) return 0;
		it->second.textures.erase(data.textureId);
		return 1;
	}
	case NPDISP_OGL_CMD_READ_PIXELS:
	{
		NPDISP_OGL_PIXELS32 data; std::vector<UINT8> pixels;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data) || !data.pixels) return 0;
		if (!npdisp_ogl_readPixels32(&data, &pixels)) return 0;
		++s_pixelReadLogCount; if (s_pixelReadLogCount <= 8U) TRACEOUTOGL(("NPDISPOGL read pixels ctx=%08x %dx%d %ux%u buffer=%u", data.context, data.x, data.y, data.width, data.height, data.buffer));
		return npdisp_writeLinearMemory(&pixels[0], data.pixels, (int)pixels.size()) ? 1U : 0U;
	}
	case NPDISP_OGL_CMD_DRAW_PIXELS:
	{
		NPDISP_OGL_PIXELS32 data; std::vector<UINT8> pixels; UINT64 bytes;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data) || !data.pixels) return 0;
		bytes=(UINT64)data.width*data.height*4U; if(!bytes||bytes>0x7fffffffULL)return 0; pixels.resize((size_t)bytes);
		if(!npdisp_readLinearMemory(&pixels[0],data.pixels,(int)bytes))return 0;
		++s_pixelDrawLogCount; if (s_pixelDrawLogCount <= 8U) TRACEOUTOGL(("NPDISPOGL draw pixels ctx=%08x dst=%d,%d %ux%u zoom=%.2f,%.2f", data.context, data.x, data.y, data.width, data.height, data.zoomX, data.zoomY));
		return npdisp_ogl_drawPixels32(&data,&pixels[0])?1U:0U;
	}
	case NPDISP_OGL_CMD_COPY_PIXELS:
	{
		NPDISP_OGL_PIXELS32 data; std::vector<UINT8> pixels;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data)) return 0;
		if(!npdisp_ogl_readPixels32(&data,&pixels))return 0; data.x=data.dstX; data.y=data.dstY;
		++s_pixelCopyLogCount; if (s_pixelCopyLogCount <= 8U) TRACEOUTOGL(("NPDISPOGL copy pixels ctx=%08x dst=%d,%d %ux%u", data.context, data.x, data.y, data.width, data.height));
		return npdisp_ogl_drawPixels32(&data,&pixels[0])?1U:0U;
	}
	case NPDISP_OGL_CMD_LIST_UPLOAD:
	{
		NPDISP_OGL_LIST_UPLOAD32 data;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data)) return 0;
		if (!npdisp_ogl_listUpload(data)) return 0;
		++s_listUploadLogCount;
		if (s_listUploadLogCount <= 8U) TRACEOUTOGL(("NPDISPOGL list upload ctx=%08x list=%u commands=%u", data.context, data.list, data.commandCount));
		return 1;
	}
	case NPDISP_OGL_CMD_LIST_DELETE:
	{
		NPDISP_OGL_LIST_DELETE32 data;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data)) return 0;
		return npdisp_ogl_listDelete(data) ? 1U : 0U;
	}
	case NPDISP_OGL_CMD_LIST_EXEC:
	{
		NPDISP_OGL_LIST_EXEC32 data;
		LARGE_INTEGER startCounter, endCounter, counterFrequency;
		DWORD startTick, elapsed;
		UINT32 workClock = 0;
		bool highResolution;
		bool highResolutionDone = false;
		bool ok;
		if (!npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data)) || data.size < sizeof(data)) return 0;
		startTick = GetTickCount();
		highResolution = QueryPerformanceFrequency(&counterFrequency) && counterFrequency.QuadPart > 0 && QueryPerformanceCounter(&startCounter);
		ok = npdisp_ogl_listExec(data);
		elapsed = GetTickCount() - startTick;
		if (highResolution && QueryPerformanceCounter(&endCounter) && endCounter.QuadPart >= startCounter.QuadPart) {
			highResolutionDone = true;
			workClock = npdisp_ogl_accountListExecTime(endCounter.QuadPart - startCounter.QuadPart, counterFrequency.QuadPart);
#if NPDISP_OGL_PERF_DIAG
			UINT64 us64 = (UINT64)(endCounter.QuadPart - startCounter.QuadPart) * 1000000ULL / (UINT64)counterFrequency.QuadPart;
			UINT32 us = us64 > 0xffffffffULL ? 0xffffffffU : (UINT32)us64;
			++s_perfListCalls;
			s_perfListUs += us > 0xffffffffU - s_perfListUs ? 0xffffffffU - s_perfListUs : us;
#endif
		}
		if (!highResolutionDone && elapsed) workClock = npdisp_ogl_accountListExecTime((LONGLONG)elapsed, 1000);
		++s_listExecLogCount;
		if (s_listExecLogCount <= 16U || elapsed >= 100U) TRACEOUTOGL(("NPDISPOGL LISTTIME n=%u ctx=%08x lists=%u hostMs=%u guestClock=%u", s_listExecLogCount, data.context, data.listCount, elapsed, workClock));
		return ok ? 1U : 0U;
	}
	case NPDISP_OGL_CMD_SWAP:
	{
		NPDISP_OGL_SWAP32 data;
		NPDISP_OGL_CONTEXT32 base;
		std::map<UINT32, NPDISP_OGL_CONTEXT>::iterator it;
		std::vector<NPDISP_OGL_CLIPRECT32> rects;
		UINT32 bytesPerPixel;
		UINT32 sourceBytesPerPixel;
		UINT32 swapPixels = 0U;
		bool indexedTarget;
		bool forceIndexedFull = false;
#if NPDISP_OGL_PERF_DIAG
		LONGLONG swapPerfStart = 0, swapPerfEnd = 0;
		bool swapPerfTiming = false;
#endif
		if (!npdisp_readLinearMemory(&base, lpDataAddr, sizeof(base)) || base.size < sizeof(base)) return 0;
		memset(&data, 0, sizeof(data));
		data.size = base.size;
		data.context = base.context;
		if (base.size >= sizeof(data) && !npdisp_readLinearMemory(&data, lpDataAddr, sizeof(data))) return 0;
		it = s_contexts.find(data.context);
		if (it == s_contexts.end() || !npdisp.mm_screenPtr || !npdispwin.stride) return 0;
		NPDISP_OGL_CONTEXT &ctx = it->second;
		indexedTarget = npdisp.bpp == 1U || npdisp.bpp == 4U || npdisp.bpp == 8U;
		bytesPerPixel = indexedTarget ? 0U : npdisp_ogl_pixelBytes(npdisp.bpp);
		sourceBytesPerPixel = npdisp_ogl_pixelBytes(ctx.bpp);
		if ((!indexedTarget && !bytesPerPixel) || !sourceBytesPerPixel || ctx.color.empty() || !ctx.width || !ctx.height ||
			(indexedTarget ? (ctx.bpp != 32U) : (ctx.bpp != npdisp.bpp))) return 0;
		if (npdisp.bpp == 8U) {
			forceIndexedFull = !s_palette8DitherMapValid || memcmp(s_palette8Snapshot, npdisp_palette_rgb256, sizeof(s_palette8Snapshot)) != 0;
			npdisp_ogl_updatePalette8Map();
		}
		else if (npdisp.bpp == 4U) {
			forceIndexedFull = !s_palette4DitherMapValid || memcmp(s_palette4Snapshot, npdisp_palette_rgb16, sizeof(s_palette4Snapshot)) != 0;
			npdisp_ogl_updatePalette4Map();
		}
		else if (npdisp.bpp == 1U) {
			forceIndexedFull = !s_palette1LumaMapValid;
			npdisp_ogl_updatePalette1Map();
		}
		if (data.clipValid && data.clipCount) {
			if (data.clipCount > 256U || !data.clipRects) return 0;
			rects.resize(data.clipCount);
			if (!npdisp_readLinearMemory(&rects[0], data.clipRects, (int)(data.clipCount * sizeof(rects[0])))) return 0;
		}
		if (indexedTarget && npdisp_ogl_updatePresentClipState(&ctx, data.clipValid ? 1U : 0U, rects)) forceIndexedFull = true;
		{
			static UINT32 swapDiagCount=0;
			static DWORD lastSwapTick=0;
			DWORD nowSwapTick=GetTickCount();
			DWORD hostDt=lastSwapTick ? nowSwapTick-lastSwapTick : 0;
			UINT32 pendingBefore=npdisp.updated ? 1U : 0U;
			lastSwapTick=nowSwapTick;
			if(swapDiagCount<128U || hostDt>=100U){
				if(!rects.empty()) TRACEOUTOGL(("NPDISPOGL SWAPDIAG n=%u hostDt=%u pendingBefore=%u ctx=%08x target=%d,%d %ux%u clipValid=%u clipCount=%u first=%d,%d-%d,%d direct=%u list=%u clipFast=%u clipPartial=%u clipReject=%u texSwitch=%u",swapDiagCount+1U,hostDt,pendingBefore,data.context,ctx.x,ctx.y,ctx.width,ctx.height,data.clipValid,data.clipCount,rects[0].left,rects[0].top,rects[0].right,rects[0].bottom,s_diagDirectDraws,s_diagListDraws,s_diagClipFast,s_diagClipPartial,s_diagClipReject,s_diagTextureSwitches));
				else TRACEOUTOGL(("NPDISPOGL SWAPDIAG n=%u hostDt=%u pendingBefore=%u ctx=%08x target=%d,%d %ux%u clipValid=%u clipCount=%u direct=%u list=%u clipFast=%u clipPartial=%u clipReject=%u texSwitch=%u",swapDiagCount+1U,hostDt,pendingBefore,data.context,ctx.x,ctx.y,ctx.width,ctx.height,data.clipValid,data.clipCount,s_diagDirectDraws,s_diagListDraws,s_diagClipFast,s_diagClipPartial,s_diagClipReject,s_diagTextureSwitches));
			}
			++swapDiagCount;
			s_diagDirectDraws=s_diagListDraws=s_diagClipFast=s_diagClipPartial=s_diagClipReject=s_diagTextureSwitches=0;
			s_diagLastTextureId=0xffffffffU;
		}
#if NPDISP_OGL_PERF_DIAG
		swapPerfTiming = npdisp_ogl_perfNow(&swapPerfStart);
#endif
		if (indexedTarget && !forceIndexedFull && !ctx.dirtyValid) {
			/* Nothing changed in the backing color buffer. */
		}
		else if (data.clipValid) {
			SINT32 dirtyLeft = 0, dirtyTop = 0, dirtyRight = (SINT32)ctx.width, dirtyBottom = (SINT32)ctx.height;
			if (indexedTarget && !forceIndexedFull && ctx.dirtyValid) { dirtyLeft = ctx.dirtyLeft; dirtyTop = ctx.dirtyTop; dirtyRight = ctx.dirtyRight; dirtyBottom = ctx.dirtyBottom; }
			for (UINT32 i = 0; i < rects.size(); ++i) {
				SINT32 left = rects[i].left, top = rects[i].top, right = rects[i].right, bottom = rects[i].bottom;
				SINT32 screenLeft, screenTop, screenRight, screenBottom;
				if (indexedTarget) {
					if (left < dirtyLeft) left = dirtyLeft;
					if (top < dirtyTop) top = dirtyTop;
					if (right > dirtyRight) right = dirtyRight;
					if (bottom > dirtyBottom) bottom = dirtyBottom;
				}
				if (left < 0) left = 0;
				if (top < 0) top = 0;
				if (right > (SINT32)ctx.width) right = (SINT32)ctx.width;
				if (bottom > (SINT32)ctx.height) bottom = (SINT32)ctx.height;
				if (right <= left || bottom <= top) continue;
				screenLeft = ctx.x + left; screenTop = ctx.y + top; screenRight = ctx.x + right; screenBottom = ctx.y + bottom;
				if (screenLeft < 0) { left -= screenLeft; screenLeft = 0; }
				if (screenTop < 0) { top -= screenTop; screenTop = 0; }
				if (screenRight > (SINT32)npdisp.width) { right -= screenRight - (SINT32)npdisp.width; screenRight = (SINT32)npdisp.width; }
				if (screenBottom > (SINT32)npdisp.height) { bottom -= screenBottom - (SINT32)npdisp.height; screenBottom = (SINT32)npdisp.height; }
				if (right <= left || bottom <= top) continue;
				for (SINT32 y = top; y < bottom; ++y) {
					UINT8 *dstRow = npdisp.mm_screenPtr + (UINT32)(ctx.y + y) * npdispwin.stride;
					const UINT8 *src = &ctx.color[((size_t)y * ctx.width + (UINT32)left) * sourceBytesPerPixel];
					if (indexedTarget) {
						for (SINT32 x = left; x < right; ++x, src += 4) {
							SINT32 screenX = ctx.x + x;
							UINT8 index = npdisp.bpp == 8U ? npdisp_ogl_palette8Dither(src, screenX, ctx.y + y) :
								(npdisp.bpp == 4U ? npdisp_ogl_palette4Dither(src, screenX, ctx.y + y) : npdisp_ogl_palette1Dither(src, screenX, ctx.y + y));
							npdisp_ogl_writeIndexedPixel(dstRow, (UINT32)screenX, npdisp.bpp, index);
						}
					}
					else memcpy(dstRow + (UINT32)(ctx.x + left) * bytesPerPixel, src, (size_t)(right - left) * bytesPerPixel);
				}
				if ((UINT64)(right - left) * (UINT64)(bottom - top) > 0xffffffffULL - swapPixels) swapPixels = 0xffffffffU;
				else swapPixels += (UINT32)((right - left) * (bottom - top));
				npdisp_setDirty(screenLeft, screenTop, screenRight, screenBottom);
			}
		}
		else {
			SINT32 left = 0, top = 0, right = (SINT32)ctx.width, bottom = (SINT32)ctx.height;
			if (indexedTarget && !forceIndexedFull && ctx.dirtyValid) { left = ctx.dirtyLeft; top = ctx.dirtyTop; right = ctx.dirtyRight; bottom = ctx.dirtyBottom; }
			SINT32 screenLeft = ctx.x + left, screenTop = ctx.y + top, screenRight = ctx.x + right, screenBottom = ctx.y + bottom;
			if (screenLeft < 0) { left -= screenLeft; screenLeft = 0; }
			if (screenTop < 0) { top -= screenTop; screenTop = 0; }
			if (screenRight > (SINT32)npdisp.width) { right -= screenRight - (SINT32)npdisp.width; screenRight = (SINT32)npdisp.width; }
			if (screenBottom > (SINT32)npdisp.height) { bottom -= screenBottom - (SINT32)npdisp.height; screenBottom = (SINT32)npdisp.height; }
			if (right > left && bottom > top) {
				for (SINT32 y = top; y < bottom; ++y) {
					UINT8 *dstRow = npdisp.mm_screenPtr + (UINT32)(ctx.y + y) * npdispwin.stride;
					const UINT8 *src = &ctx.color[((size_t)y * ctx.width + (UINT32)left) * sourceBytesPerPixel];
					if (indexedTarget) {
						for (SINT32 x = left; x < right; ++x, src += 4) {
							SINT32 screenX = ctx.x + x;
							UINT8 index = npdisp.bpp == 8U ? npdisp_ogl_palette8Dither(src, screenX, ctx.y + y) :
								(npdisp.bpp == 4U ? npdisp_ogl_palette4Dither(src, screenX, ctx.y + y) : npdisp_ogl_palette1Dither(src, screenX, ctx.y + y));
							npdisp_ogl_writeIndexedPixel(dstRow, (UINT32)screenX, npdisp.bpp, index);
						}
					}
					else memcpy(dstRow + (UINT32)(ctx.x + left) * bytesPerPixel, src, (size_t)(right - left) * bytesPerPixel);
				}
				if ((UINT64)(right - left) * (UINT64)(bottom - top) > 0xffffffffULL - swapPixels) swapPixels = 0xffffffffU;
				else swapPixels += (UINT32)((right - left) * (bottom - top));
				npdisp_setDirty(screenLeft, screenTop, screenRight, screenBottom);
			}
		}
		if (indexedTarget) ctx.dirtyValid = 0U;
#if NPDISP_OGL_PERF_DIAG
		UINT32 swapUs = 0U;
		if (swapPerfTiming && npdisp_ogl_perfNow(&swapPerfEnd)) swapUs = npdisp_ogl_perfUs(swapPerfStart, swapPerfEnd);
		++s_perfFrameCount;
		if (s_perfClearCalls >= 4U || s_perfClearUs >= 5000U || s_perfListUs >= 10000U || swapUs >= 10000U || (s_perfClearCalls && !(s_perfFrameCount & 15U))) {
			TRACEOUTOGL(("NPDISPOGL PERF frame=%u bpp=%u clearCalls=%u clearPixels=%u clearMask=%x clearUs=%u clearMaxUs=%u listCalls=%u listUs=%u swapPixels=%u swapUs=%u",
				s_perfFrameCount, npdisp.bpp, s_perfClearCalls, s_perfClearPixels, s_perfClearMask, s_perfClearUs, s_perfClearMaxUs, s_perfListCalls, s_perfListUs, swapPixels, swapUs));
		}
		s_perfClearCalls = s_perfClearPixels = s_perfClearUs = s_perfClearMaxUs = s_perfClearMask = 0U;
		s_perfListCalls = s_perfListUs = 0U;
#endif
		npdisp.updated = 1;
		return 1;
	}
	default:
		return 0;
	}
}


UINT32 npdisp_ogl_dispatch(UINT32 command, UINT32 lpDataAddr)
{
	return npdisp_ogl_dispatchInternal(command, lpDataAddr);
}

#endif
