/**
 * @file	npdisp_d3d_sw.cpp
 * @brief	NPDISP Direct3D software renderer
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_D3D)

#include	<map>
#include	<vector>

#include	"pccore.h"

#include	"npdispdef.h"
#include	"npdisp_d3d.h"
#include	"npdisp_d3d_sw.h"

static UINT32 npdisp_d3d_sw_bytesPerPixel(UINT32 bpp)
{
	if (bpp == 15 || bpp == 16) return 2;
	if (bpp == 24) return 3;
	if (bpp == 32) return 4;
	return 0;
}

static void npdisp_d3d_sw_writePixel(UINT8* p, UINT32 bpp, UINT32 color)
{
	const UINT32 r = (color >> 16) & 0xff;
	const UINT32 g = (color >> 8) & 0xff;
	const UINT32 b = color & 0xff;
	if (bpp == 15) {
		const UINT16 v = (UINT16)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
		p[0] = (UINT8)v;
		p[1] = (UINT8)(v >> 8);
	}
	else if (bpp == 16) {
		const UINT16 v = (UINT16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
		p[0] = (UINT8)v;
		p[1] = (UINT8)(v >> 8);
	}
	else if (bpp == 24) {
		p[0] = (UINT8)b;
		p[1] = (UINT8)g;
		p[2] = (UINT8)r;
	}
	else if (bpp == 32) {
		p[0] = (UINT8)b;
		p[1] = (UINT8)g;
		p[2] = (UINT8)r;
		p[3] = 0;
	}
}

static UINT32 npdisp_d3d_sw_readPixel(const UINT8* p, UINT32 bpp)
{
	UINT32 r, g, b;
	if (bpp == 15) {
		const UINT16 v = (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
		r = ((v >> 10) & 0x1fU) * 255U / 31U;
		g = ((v >> 5) & 0x1fU) * 255U / 31U;
		b = (v & 0x1fU) * 255U / 31U;
	}
	else if (bpp == 16) {
		const UINT16 v = (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
		r = ((v >> 11) & 0x1fU) * 255U / 31U;
		g = ((v >> 5) & 0x3fU) * 255U / 63U;
		b = (v & 0x1fU) * 255U / 31U;
	}
	else if (bpp == 24 || bpp == 32) {
		b = p[0];
		g = p[1];
		r = p[2];
	}
	else return 0xff000000UL;
	return 0xff000000UL | (r << 16) | (g << 8) | b;
}

static UINT32 npdisp_d3d_sw_blendFactor(UINT32 mode, UINT32 src, UINT32 dst, UINT32 shift)
{
	const UINT32 srcComponent = (src >> shift) & 0xffU;
	const UINT32 dstComponent = (dst >> shift) & 0xffU;
	const UINT32 srcAlpha = (src >> 24) & 0xffU;
	const UINT32 dstAlpha = (dst >> 24) & 0xffU;
	switch (mode) {
	case NPDISP_D3DBLEND_ZERO: return 0U;
	case NPDISP_D3DBLEND_ONE: return 255U;
	case NPDISP_D3DBLEND_SRCCOLOR: return srcComponent;
	case NPDISP_D3DBLEND_INVSRCCOLOR: return 255U - srcComponent;
	case NPDISP_D3DBLEND_SRCALPHA: return srcAlpha;
	case NPDISP_D3DBLEND_INVSRCALPHA: return 255U - srcAlpha;
	case NPDISP_D3DBLEND_DESTALPHA: return dstAlpha;
	case NPDISP_D3DBLEND_INVDESTALPHA: return 255U - dstAlpha;
	case NPDISP_D3DBLEND_DESTCOLOR: return dstComponent;
	case NPDISP_D3DBLEND_INVDESTCOLOR: return 255U - dstComponent;
	case NPDISP_D3DBLEND_SRCALPHASAT: {
		UINT32 invDest = 255U - dstAlpha;
		return srcAlpha < invDest ? srcAlpha : invDest;
	}
	default: return 0U;
	}
}

static bool npdisp_d3d_sw_blend(UINT32 src, UINT32 dst, UINT32 srcBlend, UINT32 destBlend, UINT32* color)
{
	UINT32 out = 0xff000000UL;
	if (!color) return false;
	for (UINT32 shift = 0; shift <= 16; shift += 8) {
		UINT32 s = (src >> shift) & 0xffU;
		UINT32 d = (dst >> shift) & 0xffU;
		UINT32 sf = npdisp_d3d_sw_blendFactor(srcBlend, src, dst, shift);
		UINT32 df = npdisp_d3d_sw_blendFactor(destBlend, src, dst, shift);
		UINT32 v = (s * sf + d * df + 127U) / 255U;
		if (v > 255U) v = 255U;
		out |= v << shift;
	}
	*color = out;
	return true;
}

static bool npdisp_d3d_sw_outputPixel(NPDISP_D3D_SW_TARGET* target, UINT8* p, const NPDISP_D3D_RASTERSTATE* state, UINT32 color)
{
	if (!target || !p || !state) return false;
	if (state->alphaBlendEnable) {
		UINT32 blended;
		if (!npdisp_d3d_sw_blend(color, npdisp_d3d_sw_readPixel(p, target->bpp), state->srcBlend, state->destBlend, &blended)) return false;
		color = blended;
	}
	npdisp_d3d_sw_writePixel(p, target->bpp, color);
	return true;
}

static SINT32 npdisp_d3d_sw_floorToInt(float value)
{
	SINT32 i = (SINT32)value;
	if ((float)i > value) --i;
	return i;
}

static UINT32 npdisp_d3d_sw_maskTo8(UINT32 value, UINT32 mask)
{
	UINT32 shift = 0;
	UINT32 maximum;
	UINT32 component;
	if (!mask) return 0xffU;
	while (!(mask & 1U)) {
		mask >>= 1;
		++shift;
	}
	maximum = mask;
	component = (value >> shift) & maximum;
	return maximum ? (component * 255U + maximum / 2U) / maximum : 0U;
}

static UINT32 npdisp_d3d_sw_texturePixel(const NPDISP_D3D_TEXTURE* texture, SINT32 x, SINT32 y, bool* transparent)
{
	const UINT8* p;
	UINT32 bytesPerPixel;
	UINT32 value;
	UINT32 a, r, g, b;
	if (transparent) *transparent = false;
	if (!texture || !texture->pixels || x < 0 || y < 0 || x >= (SINT32)texture->width || y >= (SINT32)texture->height ||
		!((texture->format == NPDISP_D3D_TEXTURE_FORMAT_P8 && texture->bpp == 8U) ||
		(texture->format == NPDISP_D3D_TEXTURE_FORMAT_RGB16 && texture->bpp == 16U) ||
		(texture->format == NPDISP_D3D_TEXTURE_FORMAT_RGB32 && texture->bpp == 32U))) return 0xffffffffUL;
	bytesPerPixel = texture->bpp >> 3;
	p = texture->pixels + (size_t)y * (size_t)texture->pitch + (size_t)x * bytesPerPixel;
	value = p[0];
	if (bytesPerPixel >= 2U) value |= (UINT32)p[1] << 8;
	if (bytesPerPixel == 4U) value |= ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
	if (transparent && texture->colorKeyEnable && ((value & texture->colorKeyMask) >= texture->colorKeyLow) && ((value & texture->colorKeyMask) <= texture->colorKeyHigh)) *transparent = true;
	if (texture->format == NPDISP_D3D_TEXTURE_FORMAT_P8) return texture->palette[value & 0xffU];
	a = texture->aMask ? npdisp_d3d_sw_maskTo8(value, texture->aMask) : 255U;
	r = npdisp_d3d_sw_maskTo8(value, texture->rMask);
	g = npdisp_d3d_sw_maskTo8(value, texture->gMask);
	b = npdisp_d3d_sw_maskTo8(value, texture->bMask);
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static SINT32 npdisp_d3d_sw_addressIndex(SINT32 value, UINT32 size, UINT32 address)
{
	if (!size) return 0;
	if (address == NPDISP_D3DTADDRESS_WRAP) {
		SINT32 result = value % (SINT32)size;
		if (result < 0) result += (SINT32)size;
		return result;
	}
	if (address == NPDISP_D3DTADDRESS_MIRROR) {
		SINT32 period;
		SINT32 result;
		if (size > 0x3fffffffU) return 0;
		period = (SINT32)(size * 2U);
		result = value % period;
		if (result < 0) result += period;
		if (result >= (SINT32)size) result = period - 1 - result;
		return result;
	}
	if (value < 0) return 0;
	if (value >= (SINT32)size) return (SINT32)size - 1;
	return value;
}

static UINT32 npdisp_d3d_sw_lerpColor(UINT32 c0, UINT32 c1, float t)
{
	float inv = 1.0f - t;
	UINT32 a = (UINT32)(((float)((c0 >> 24) & 0xffU) * inv) + ((float)((c1 >> 24) & 0xffU) * t) + 0.5f);
	UINT32 r = (UINT32)(((float)((c0 >> 16) & 0xffU) * inv) + ((float)((c1 >> 16) & 0xffU) * t) + 0.5f);
	UINT32 g = (UINT32)(((float)((c0 >> 8) & 0xffU) * inv) + ((float)((c1 >> 8) & 0xffU) * t) + 0.5f);
	UINT32 b = (UINT32)(((float)(c0 & 0xffU) * inv) + ((float)(c1 & 0xffU) * t) + 0.5f);
	return (a << 24) | (r << 16) | (g << 8) | b;
}

typedef struct {
	float dudx;
	float dvdx;
	float dudy;
	float dvdy;
} NPDISP_D3D_SW_TEXGRAD;

static float npdisp_d3d_sw_abs(float value)
{
	return value < 0.0f ? -value : value;
}

static bool npdisp_d3d_sw_sampleTextureLevel(const NPDISP_D3D_TEXTURE* texture, float u, float v, float w, UINT32* color, bool* transparent)
{
	float x, y;
	SINT32 x0, y0, x1, y1;
	UINT32 bytesPerPixel;
	if (transparent) *transparent = false;
	if (!texture || !color || !texture->pixels || !texture->width || !texture->height || texture->pitch <= 0 ||
		!((texture->format == NPDISP_D3D_TEXTURE_FORMAT_P8 && texture->bpp == 8U) ||
		(texture->format == NPDISP_D3D_TEXTURE_FORMAT_RGB16 && texture->bpp == 16U) ||
		(texture->format == NPDISP_D3D_TEXTURE_FORMAT_RGB32 && texture->bpp == 32U)) ||
		u != u || v != v || u < -1048576.0f || u > 1048576.0f || v < -1048576.0f || v > 1048576.0f) return false;
	bytesPerPixel = texture->bpp >> 3;
	if ((UINT64)texture->width * bytesPerPixel > (UINT32)texture->pitch) return false;
	if (texture->cubeMap) {
		NPDISP_D3D_TEXTURE faceTexture;
		float ax = u < 0.0f ? -u : u;
		float ay = v < 0.0f ? -v : v;
		float az = w < 0.0f ? -w : w;
		float major;
		float cu;
		float cv;
		UINT32 face;
		if (w != w || w < -1048576.0f || w > 1048576.0f) return false;
		if (ax >= ay && ax >= az) {
			major = ax;
			if (u >= 0.0f) { face = 0; cu = -w; cv = -v; }
			else { face = 1; cu = w; cv = -v; }
		}
		else if (ay >= az) {
			major = ay;
			if (v >= 0.0f) { face = 2; cu = u; cv = w; }
			else { face = 3; cu = u; cv = -w; }
		}
		else {
			major = az;
			if (w >= 0.0f) { face = 4; cu = u; cv = -v; }
			else { face = 5; cu = -u; cv = -v; }
		}
		if (major > 0.0000001f) { cu /= major; cv /= major; }
		else { cu = cv = 0.0f; }
		faceTexture = *texture;
		faceTexture.pixels = texture->cubePixels[face];
		faceTexture.cubeMap = 0;
		faceTexture.addressU = NPDISP_D3DTADDRESS_CLAMP;
		faceTexture.addressV = NPDISP_D3DTADDRESS_CLAMP;
		return faceTexture.pixels && npdisp_d3d_sw_sampleTextureLevel(&faceTexture, cu * 0.5f + 0.5f, cv * 0.5f + 0.5f, 0.0f, color, transparent);
	}
	if (texture->magFilter == NPDISP_D3DTFG_LINEAR || texture->minFilter == NPDISP_D3DTFN_LINEAR) {
		if (texture->addressU == NPDISP_D3DTADDRESS_WRAP || texture->addressU == NPDISP_D3DTADDRESS_MIRROR) x = u * (float)texture->width - 0.5f;
		else {
			if (u < 0.0f) u = 0.0f; else if (u > 1.0f) u = 1.0f;
			x = u * (float)texture->width - 0.5f;
		}
		if (texture->addressV == NPDISP_D3DTADDRESS_WRAP || texture->addressV == NPDISP_D3DTADDRESS_MIRROR) y = v * (float)texture->height - 0.5f;
		else {
			if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
			y = v * (float)texture->height - 0.5f;
		}
		x0 = npdisp_d3d_sw_floorToInt(x);
		y0 = npdisp_d3d_sw_floorToInt(y);
		x1 = x0 + 1;
		y1 = y0 + 1;
		{
			float fx = x - (float)x0;
			float fy = y - (float)y0;
			bool t00 = false, t10 = false, t01 = false, t11 = false;
			UINT32 c00 = npdisp_d3d_sw_texturePixel(texture, npdisp_d3d_sw_addressIndex(x0, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y0, texture->height, texture->addressV), &t00);
			UINT32 c10 = npdisp_d3d_sw_texturePixel(texture, npdisp_d3d_sw_addressIndex(x1, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y0, texture->height, texture->addressV), &t10);
			UINT32 c01 = npdisp_d3d_sw_texturePixel(texture, npdisp_d3d_sw_addressIndex(x0, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y1, texture->height, texture->addressV), &t01);
			UINT32 c11 = npdisp_d3d_sw_texturePixel(texture, npdisp_d3d_sw_addressIndex(x1, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y1, texture->height, texture->addressV), &t11);
			if (transparent) *transparent = t00 && t10 && t01 && t11;
			*color = npdisp_d3d_sw_lerpColor(npdisp_d3d_sw_lerpColor(c00, c10, fx), npdisp_d3d_sw_lerpColor(c01, c11, fx), fy);
		}
	}
	else {
		if (texture->addressU == NPDISP_D3DTADDRESS_CLAMP) { if (u < 0.0f) u = 0.0f; else if (u > 1.0f) u = 1.0f; }
		if (texture->addressV == NPDISP_D3DTADDRESS_CLAMP) { if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f; }
		x0 = npdisp_d3d_sw_floorToInt(u * (float)texture->width);
		y0 = npdisp_d3d_sw_floorToInt(v * (float)texture->height);
		*color = npdisp_d3d_sw_texturePixel(texture, npdisp_d3d_sw_addressIndex(x0, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y0, texture->height, texture->addressV), transparent);
	}
	return true;
}

static bool npdisp_d3d_sw_sampleTexture(const NPDISP_D3D_TEXTURE* texture, float u, float v, float w, const NPDISP_D3D_SW_TEXGRAD* grad, UINT32* color, bool* transparent)
{
	NPDISP_D3D_TEXTURE levelTexture;
	UINT32 level;
	float rho;
	float frac;
	if (!texture || !color) return false;
	if (transparent) *transparent = false;
	if (texture->cubeMap || texture->mipFilter == NPDISP_D3DTFP_NONE || texture->mipCount <= 1U || !grad)
		return npdisp_d3d_sw_sampleTextureLevel(texture, u, v, w, color, transparent);
	rho = npdisp_d3d_sw_abs(grad->dudx) * (float)texture->width;
	{
		float value = npdisp_d3d_sw_abs(grad->dvdx) * (float)texture->height;
		if (value > rho) rho = value;
		value = npdisp_d3d_sw_abs(grad->dudy) * (float)texture->width;
		if (value > rho) rho = value;
		value = npdisp_d3d_sw_abs(grad->dvdy) * (float)texture->height;
		if (value > rho) rho = value;
	}
	if (rho <= 1.0f) return npdisp_d3d_sw_sampleTextureLevel(texture, u, v, w, color, transparent);
	level = 0;
	while (rho >= 2.0f && level + 1U < texture->mipCount) {
		rho *= 0.5f;
		++level;
	}
	frac = rho - 1.0f;
	if (frac < 0.0f) frac = 0.0f; else if (frac > 1.0f) frac = 1.0f;
	if (texture->mipFilter == NPDISP_D3DTFP_POINT && frac >= 0.5f && level + 1U < texture->mipCount) ++level;
	levelTexture = *texture;
	levelTexture.pixels = texture->mipPixels[level];
	levelTexture.width = texture->mipWidth[level];
	levelTexture.height = texture->mipHeight[level];
	levelTexture.pitch = texture->mipPitch[level];
	levelTexture.mipCount = 1U;
	levelTexture.mipFilter = NPDISP_D3DTFP_NONE;
	if (!levelTexture.pixels || !levelTexture.width || !levelTexture.height || levelTexture.pitch <= 0) return false;
	if (texture->mipFilter != NPDISP_D3DTFP_LINEAR || level + 1U >= texture->mipCount)
		return npdisp_d3d_sw_sampleTextureLevel(&levelTexture, u, v, w, color, transparent);
	{
		UINT32 c0;
		UINT32 c1;
		bool t0 = false;
		bool t1 = false;
		NPDISP_D3D_TEXTURE nextTexture = levelTexture;
		nextTexture.pixels = texture->mipPixels[level + 1U];
		nextTexture.width = texture->mipWidth[level + 1U];
		nextTexture.height = texture->mipHeight[level + 1U];
		nextTexture.pitch = texture->mipPitch[level + 1U];
		if (!nextTexture.pixels || !nextTexture.width || !nextTexture.height || nextTexture.pitch <= 0 ||
			!npdisp_d3d_sw_sampleTextureLevel(&levelTexture, u, v, w, &c0, &t0) ||
			!npdisp_d3d_sw_sampleTextureLevel(&nextTexture, u, v, w, &c1, &t1)) return false;
		if (transparent) *transparent = t0 && t1;
		*color = npdisp_d3d_sw_lerpColor(c0, c1, frac);
	}
	return true;
}

static UINT32 npdisp_d3d_sw_modulate(UINT32 c0, UINT32 c1)
{
	UINT32 a = ((((c0 >> 24) & 0xffU) * ((c1 >> 24) & 0xffU)) + 127U) / 255U;
	UINT32 r = ((((c0 >> 16) & 0xffU) * ((c1 >> 16) & 0xffU)) + 127U) / 255U;
	UINT32 g = ((((c0 >> 8) & 0xffU) * ((c1 >> 8) & 0xffU)) + 127U) / 255U;
	UINT32 b = (((c0 & 0xffU) * (c1 & 0xffU)) + 127U) / 255U;
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static UINT32 npdisp_d3d_sw_addColor(UINT32 c0, UINT32 c1)
{
	UINT32 a = ((c0 >> 24) & 0xffU) + ((c1 >> 24) & 0xffU);
	UINT32 r = ((c0 >> 16) & 0xffU) + ((c1 >> 16) & 0xffU);
	UINT32 g = ((c0 >> 8) & 0xffU) + ((c1 >> 8) & 0xffU);
	UINT32 b = (c0 & 0xffU) + (c1 & 0xffU);
	if (a > 255U) a = 255U;
	if (r > 255U) r = 255U;
	if (g > 255U) g = 255U;
	if (b > 255U) b = 255U;
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static UINT32 npdisp_d3d_sw_scaleColor(UINT32 color, float scale)
{
	UINT32 a = (color >> 24) & 0xffU;
	float rf, gf, bf;
	UINT32 r, g, b;
	if (scale < 0.0f) scale = 0.0f;
	rf = (float)((color >> 16) & 0xffU) * scale;
	gf = (float)((color >> 8) & 0xffU) * scale;
	bf = (float)(color & 0xffU) * scale;
	if (rf > 255.0f) rf = 255.0f;
	if (gf > 255.0f) gf = 255.0f;
	if (bf > 255.0f) bf = 255.0f;
	r = (UINT32)(rf + 0.5f);
	g = (UINT32)(gf + 0.5f);
	b = (UINT32)(bf + 0.5f);
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static UINT32 npdisp_d3d_sw_textureArg(UINT32 arg, UINT32 diffuse, UINT32 current, UINT32 texture)
{
	switch (arg & NPDISP_D3DTA_SELECTMASK) {
	case NPDISP_D3DTA_DIFFUSE: return diffuse;
	case NPDISP_D3DTA_CURRENT: return current;
	case NPDISP_D3DTA_TEXTURE: return texture;
	default: return current;
	}
}

static float npdisp_d3d_sw_signedMask(UINT32 value, UINT32 mask)
{
	UINT32 shift = 0;
	UINT32 maximum;
	UINT32 sign;
	SINT32 component;
	if (!mask) return 0.0f;
	while (!(mask & 1U)) {
		mask >>= 1;
		++shift;
	}
	maximum = mask;
	sign = (maximum + 1U) >> 1;
	component = (SINT32)((value >> shift) & maximum);
	if ((UINT32)component & sign) component -= (SINT32)(maximum + 1U);
	if (component < 0) return (float)component / (float)sign;
	return sign > 1U ? (float)component / (float)(sign - 1U) : 0.0f;
}

static float npdisp_d3d_sw_unsignedMask(UINT32 value, UINT32 mask)
{
	UINT32 shift = 0;
	UINT32 maximum;
	UINT32 component;
	if (!mask) return 1.0f;
	while (!(mask & 1U)) {
		mask >>= 1;
		++shift;
	}
	maximum = mask;
	component = (value >> shift) & maximum;
	return maximum ? (float)component / (float)maximum : 1.0f;
}

static bool npdisp_d3d_sw_bumpPixel(const NPDISP_D3D_TEXTURE* texture, SINT32 x, SINT32 y, float* du, float* dv, float* luminance)
{
	const UINT8* p;
	UINT32 value;
	if (!texture || !du || !dv || !luminance || !texture->pixels || texture->bpp != 16U ||
		(texture->format != NPDISP_D3D_TEXTURE_FORMAT_U8V8 && texture->format != NPDISP_D3D_TEXTURE_FORMAT_U5V5L6) ||
		x < 0 || y < 0 || x >= (SINT32)texture->width || y >= (SINT32)texture->height) return false;
	p = texture->pixels + (size_t)y * (size_t)texture->pitch + (size_t)x * 2U;
	value = (UINT32)p[0] | ((UINT32)p[1] << 8);
	*du = npdisp_d3d_sw_signedMask(value, texture->duMask);
	*dv = npdisp_d3d_sw_signedMask(value, texture->dvMask);
	*luminance = texture->luminanceMask ? npdisp_d3d_sw_unsignedMask(value, texture->luminanceMask) : 1.0f;
	return true;
}

static bool npdisp_d3d_sw_sampleBump(const NPDISP_D3D_TEXTURE* texture, float u, float v, float* du, float* dv, float* luminance)
{
	float x, y;
	SINT32 x0, y0, x1, y1;
	if (!texture || !du || !dv || !luminance || !texture->pixels || !texture->width || !texture->height ||
		texture->pitch <= 0 || texture->bpp != 16U || (UINT64)texture->width * 2U > (UINT32)texture->pitch) return false;
	if (texture->magFilter == NPDISP_D3DTFG_LINEAR || texture->minFilter == NPDISP_D3DTFN_LINEAR) {
		x = u * (float)texture->width - 0.5f;
		y = v * (float)texture->height - 0.5f;
		x0 = npdisp_d3d_sw_floorToInt(x);
		y0 = npdisp_d3d_sw_floorToInt(y);
		x1 = x0 + 1;
		y1 = y0 + 1;
		{
			float fx = x - (float)x0;
			float fy = y - (float)y0;
			float du00, dv00, l00, du10, dv10, l10, du01, dv01, l01, du11, dv11, l11;
			if (!npdisp_d3d_sw_bumpPixel(texture, npdisp_d3d_sw_addressIndex(x0, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y0, texture->height, texture->addressV), &du00, &dv00, &l00) ||
				!npdisp_d3d_sw_bumpPixel(texture, npdisp_d3d_sw_addressIndex(x1, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y0, texture->height, texture->addressV), &du10, &dv10, &l10) ||
				!npdisp_d3d_sw_bumpPixel(texture, npdisp_d3d_sw_addressIndex(x0, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y1, texture->height, texture->addressV), &du01, &dv01, &l01) ||
				!npdisp_d3d_sw_bumpPixel(texture, npdisp_d3d_sw_addressIndex(x1, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y1, texture->height, texture->addressV), &du11, &dv11, &l11)) return false;
			*du = (du00 * (1.0f - fx) + du10 * fx) * (1.0f - fy) + (du01 * (1.0f - fx) + du11 * fx) * fy;
			*dv = (dv00 * (1.0f - fx) + dv10 * fx) * (1.0f - fy) + (dv01 * (1.0f - fx) + dv11 * fx) * fy;
			*luminance = (l00 * (1.0f - fx) + l10 * fx) * (1.0f - fy) + (l01 * (1.0f - fx) + l11 * fx) * fy;
		}
	}
	else {
		x0 = npdisp_d3d_sw_floorToInt(u * (float)texture->width);
		y0 = npdisp_d3d_sw_floorToInt(v * (float)texture->height);
		if (!npdisp_d3d_sw_bumpPixel(texture, npdisp_d3d_sw_addressIndex(x0, texture->width, texture->addressU), npdisp_d3d_sw_addressIndex(y0, texture->height, texture->addressV), du, dv, luminance)) return false;
	}
	return true;
}

static void npdisp_d3d_sw_textureCoords(UINT32 index, float tu, float tv, float tw, float tu2, float tv2, float tw2, float* u, float* v, float* w)
{
	if (!u || !v || !w) return;
	if (index) {
		*u = tu2;
		*v = tv2;
		*w = tw2;
	}
	else {
		*u = tu;
		*v = tv;
		*w = tw;
	}
}

static bool npdisp_d3d_sw_applyTextureStages(const NPDISP_D3D_RASTERSTATE* state, UINT32 diffuse, float tu, float tv, float tw, float tu2, float tv2, float tw2, const NPDISP_D3D_SW_TEXGRAD* grad0, const NPDISP_D3D_SW_TEXGRAD* grad1, UINT32* color, bool* discard)
{
	UINT32 current = diffuse;
	float bumpDu = 0.0f;
	float bumpDv = 0.0f;
	float bumpLuminance = 1.0f;
	bool bumpPending = false;
	if (!state || !color) return false;
	if (discard) *discard = false;
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		const NPDISP_D3D_TEXTURE* texture = state->textures[stage];
		UINT32 texel, colorArg1, colorArg2, alphaArg1, alphaArg2, colorResult, alphaResult;
		float u, v, w;
		if (!texture) continue;
		if (texture->colorOp == NPDISP_D3DTOP_DISABLE) break;
		npdisp_d3d_sw_textureCoords(texture->texCoordIndex, tu, tv, tw, tu2, tv2, tw2, &u, &v, &w);
		if (texture->colorOp == NPDISP_D3DTOP_BUMPENVMAP || texture->colorOp == NPDISP_D3DTOP_BUMPENVMAPLUMINANCE) {
			float du, dv, luminance;
			if (!npdisp_d3d_sw_sampleBump(texture, u, v, &du, &dv, &luminance)) return false;
			bumpDu = du * texture->bumpMat00 + dv * texture->bumpMat01;
			bumpDv = du * texture->bumpMat10 + dv * texture->bumpMat11;
			bumpLuminance = texture->colorOp == NPDISP_D3DTOP_BUMPENVMAPLUMINANCE ?
				luminance * texture->bumpLScale + texture->bumpLOffset : 1.0f;
			bumpPending = true;
			continue;
		}
		if (bumpPending) {
			u += bumpDu;
			v += bumpDv;
		}
		{
			bool transparent = false;
			if (!npdisp_d3d_sw_sampleTexture(texture, u, v, w, texture->texCoordIndex ? grad1 : grad0, &texel, &transparent)) return false;
			if (transparent) {
				if (discard) *discard = true;
				*color = current;
				return true;
			}
		}
		if (bumpPending) {
			texel = npdisp_d3d_sw_scaleColor(texel, bumpLuminance);
			bumpPending = false;
		}
		colorArg1 = npdisp_d3d_sw_textureArg(texture->colorArg1, diffuse, current, texel);
		colorArg2 = npdisp_d3d_sw_textureArg(texture->colorArg2, diffuse, current, texel);
		switch (texture->colorOp) {
		case NPDISP_D3DTOP_SELECTARG1: colorResult = colorArg1; break;
		case NPDISP_D3DTOP_SELECTARG2: colorResult = colorArg2; break;
		case NPDISP_D3DTOP_MODULATE: colorResult = npdisp_d3d_sw_modulate(colorArg1, colorArg2); break;
		case NPDISP_D3DTOP_ADD: colorResult = npdisp_d3d_sw_addColor(colorArg1, colorArg2); break;
		default: return false;
		}
		alphaResult = current;
		if (texture->alphaOp && texture->alphaOp != NPDISP_D3DTOP_DISABLE) {
			alphaArg1 = npdisp_d3d_sw_textureArg(texture->alphaArg1, diffuse, current, texel);
			alphaArg2 = npdisp_d3d_sw_textureArg(texture->alphaArg2, diffuse, current, texel);
			switch (texture->alphaOp) {
			case NPDISP_D3DTOP_SELECTARG1: alphaResult = alphaArg1; break;
			case NPDISP_D3DTOP_SELECTARG2: alphaResult = alphaArg2; break;
			case NPDISP_D3DTOP_MODULATE: alphaResult = npdisp_d3d_sw_modulate(alphaArg1, alphaArg2); break;
			default: return false;
			}
		}
		current = (alphaResult & 0xff000000UL) | (colorResult & 0x00ffffffUL);
	}
	*color = current;
	return true;
}

static float npdisp_d3d_sw_adjustWrappedCoord(float anchor, float value)
{
	while (value - anchor > 0.5f) value -= 1.0f;
	while (value - anchor < -0.5f) value += 1.0f;
	return value;
}

bool npdisp_d3d_sw_clear(NPDISP_D3D_SW_TARGET* target, SINT32 left, SINT32 top, SINT32 right, SINT32 bottom, UINT32 color)
{
	UINT32 bytesPerPixel;
	if (!target || !target->pixels || target->pitch <= 0 || !target->width || !target->height) return false;
	bytesPerPixel = npdisp_d3d_sw_bytesPerPixel(target->bpp);
	if (!bytesPerPixel || (UINT64)target->width * bytesPerPixel > (UINT32)target->pitch) return false;
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > (SINT32)target->width) right = (SINT32)target->width;
	if (bottom > (SINT32)target->height) bottom = (SINT32)target->height;
	if (left >= right || top >= bottom) return true;
	for (SINT32 y = top; y < bottom; ++y) {
		UINT8* p = target->pixels + (size_t)y * (size_t)target->pitch + (size_t)left * bytesPerPixel;
		for (SINT32 x = left; x < right; ++x, p += bytesPerPixel) npdisp_d3d_sw_writePixel(p, target->bpp, color);
	}
	return true;
}

bool npdisp_d3d_sw_clearDepthStencil(NPDISP_D3D_SW_DEPTH_TARGET* target, SINT32 left, SINT32 top, SINT32 right, SINT32 bottom, UINT16 depth, UINT32 stencil, UINT32 flags, UINT32 depthMask, UINT32 stencilBits)
{
	UINT16 depthValue;
	UINT32 stencilMax = 0U;
	UINT32 stencilShift = 0U;
	UINT16 stencilMask = 0U;
	if (!target || !target->pixels || target->pitch <= 0 || !target->width || !target->height ||
		(UINT64)target->width * 2U > (UINT32)target->pitch || !depthMask || depthMask > 0xffffU || stencilBits > 4U) return false;
	if ((flags & NPDISP_D3DCLEAR_STENCIL) && !stencilBits) return false;
	if (stencilBits) {
		stencilMax = (1U << stencilBits) - 1U;
		stencilShift = 16U - stencilBits;
		stencilMask = (UINT16)(stencilMax << stencilShift);
		if ((UINT16)depthMask != (UINT16)~stencilMask) return false;
	}
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > (SINT32)target->width) right = (SINT32)target->width;
	if (bottom > (SINT32)target->height) bottom = (SINT32)target->height;
	if (left >= right || top >= bottom) return true;
	depthValue = (depthMask == 0xffffU) ? depth : (UINT16)(((UINT32)depth * depthMask + 32767U) / 65535U);
	for (SINT32 y = top; y < bottom; ++y) {
		UINT8* p = target->pixels + (size_t)y * (size_t)target->pitch + (size_t)left * 2U;
		for (SINT32 x = left; x < right; ++x, p += 2) {
			UINT16 raw = (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
			if (flags & NPDISP_D3DCLEAR_ZBUFFER) raw = (UINT16)((raw & ~(UINT16)depthMask) | (depthValue & (UINT16)depthMask));
			if (flags & NPDISP_D3DCLEAR_STENCIL) raw = (UINT16)((raw & ~stencilMask) | (UINT16)((stencil & stencilMax) << stencilShift));
			p[0] = (UINT8)raw;
			p[1] = (UINT8)(raw >> 8);
		}
	}
	return true;
}

static float npdisp_d3d_sw_edge(float ax, float ay, float bx, float by, float px, float py)
{
	return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static bool npdisp_d3d_sw_topLeftEdge(float ax, float ay, float bx, float by)
{
	float dx = bx - ax;
	float dy = by - ay;
	return dy > 0.0f || (dy == 0.0f && dx < 0.0f);
}

static bool npdisp_d3d_sw_validCoord(float value)
{
	return value == value && value >= -1.0e30f && value <= 1.0e30f;
}

static UINT32 npdisp_d3d_sw_interpolateColor(UINT32 c0, UINT32 c1, UINT32 c2, float w0, float w1, float w2)
{
	float a = ((float)((c0 >> 24) & 0xff) * w0) + ((float)((c1 >> 24) & 0xff) * w1) + ((float)((c2 >> 24) & 0xff) * w2);
	float r = ((float)((c0 >> 16) & 0xff) * w0) + ((float)((c1 >> 16) & 0xff) * w1) + ((float)((c2 >> 16) & 0xff) * w2);
	float g = ((float)((c0 >> 8) & 0xff) * w0) + ((float)((c1 >> 8) & 0xff) * w1) + ((float)((c2 >> 8) & 0xff) * w2);
	float b = ((float)(c0 & 0xff) * w0) + ((float)(c1 & 0xff) * w1) + ((float)(c2 & 0xff) * w2);
	if (a < 0.0f) a = 0.0f; else if (a > 255.0f) a = 255.0f;
	if (r < 0.0f) r = 0.0f; else if (r > 255.0f) r = 255.0f;
	if (g < 0.0f) g = 0.0f; else if (g > 255.0f) g = 255.0f;
	if (b < 0.0f) b = 0.0f; else if (b > 255.0f) b = 255.0f;
	return ((UINT32)(a + 0.5f) << 24) | ((UINT32)(r + 0.5f) << 16) | ((UINT32)(g + 0.5f) << 8) | (UINT32)(b + 0.5f);
}

static UINT32 npdisp_d3d_sw_addSpecular(UINT32 color, UINT32 specular)
{
	UINT32 a = color & 0xff000000UL;
	UINT32 r = ((color >> 16) & 0xffU) + ((specular >> 16) & 0xffU);
	UINT32 g = ((color >> 8) & 0xffU) + ((specular >> 8) & 0xffU);
	UINT32 b = (color & 0xffU) + (specular & 0xffU);
	if (r > 255U) r = 255U;
	if (g > 255U) g = 255U;
	if (b > 255U) b = 255U;
	return a | (r << 16) | (g << 8) | b;
}

static bool npdisp_d3d_sw_alphaPass(UINT32 alpha, UINT32 ref, UINT32 func)
{
	switch (func) {
	case NPDISP_D3DCMP_NEVER: return false;
	case NPDISP_D3DCMP_LESS: return alpha < ref;
	case NPDISP_D3DCMP_EQUAL: return alpha == ref;
	case NPDISP_D3DCMP_LESSEQUAL: return alpha <= ref;
	case NPDISP_D3DCMP_GREATER: return alpha > ref;
	case NPDISP_D3DCMP_NOTEQUAL: return alpha != ref;
	case NPDISP_D3DCMP_GREATEREQUAL: return alpha >= ref;
	case NPDISP_D3DCMP_ALWAYS: return true;
	default: return false;
	}
}

static UINT32 npdisp_d3d_sw_applyFog(UINT32 color, UINT32 fogColor, float factor)
{
	UINT32 a = color & 0xff000000UL;
	float inv;
	float r, g, b;
	if (factor < 0.0f) factor = 0.0f; else if (factor > 1.0f) factor = 1.0f;
	inv = 1.0f - factor;
	r = (float)((color >> 16) & 0xffU) * factor + (float)((fogColor >> 16) & 0xffU) * inv;
	g = (float)((color >> 8) & 0xffU) * factor + (float)((fogColor >> 8) & 0xffU) * inv;
	b = (float)(color & 0xffU) * factor + (float)(fogColor & 0xffU) * inv;
	return a | ((UINT32)(r + 0.5f) << 16) | ((UINT32)(g + 0.5f) << 8) | (UINT32)(b + 0.5f);
}

static bool npdisp_d3d_sw_depthPass(UINT16 value, UINT16 current, UINT32 func)
{
	switch (func) {
	case NPDISP_D3DCMP_NEVER: return false;
	case NPDISP_D3DCMP_LESS: return value < current;
	case NPDISP_D3DCMP_EQUAL: return value == current;
	case NPDISP_D3DCMP_LESSEQUAL: return value <= current;
	case NPDISP_D3DCMP_GREATER: return value > current;
	case NPDISP_D3DCMP_NOTEQUAL: return value != current;
	case NPDISP_D3DCMP_GREATEREQUAL: return value >= current;
	case NPDISP_D3DCMP_ALWAYS: return true;
	default: return false;
	}
}

static UINT16 npdisp_d3d_sw_depthValue(float z, UINT32 depthMask)
{
	if (z < 0.0f) z = 0.0f; else if (z > 1.0f) z = 1.0f;
	if (!depthMask || depthMask > 0xffffU) depthMask = 0xffffU;
	return (UINT16)(z * (float)depthMask + 0.5f);
}

static UINT16 npdisp_d3d_sw_readDepthStencil(const UINT8* p)
{
	return (UINT16)((UINT16)p[0] | ((UINT16)p[1] << 8));
}

static void npdisp_d3d_sw_writeDepthStencil(UINT8* p, UINT16 value)
{
	p[0] = (UINT8)value;
	p[1] = (UINT8)(value >> 8);
}

static UINT32 npdisp_d3d_sw_stencilOp(UINT32 value, UINT32 ref, UINT32 op, UINT32 maxValue)
{
	switch (op) {
	case NPDISP_D3DSTENCILOP_KEEP: return value;
	case NPDISP_D3DSTENCILOP_ZERO: return 0U;
	case NPDISP_D3DSTENCILOP_REPLACE: return ref & maxValue;
	case NPDISP_D3DSTENCILOP_INCRSAT: return value < maxValue ? value + 1U : maxValue;
	case NPDISP_D3DSTENCILOP_DECRSAT: return value ? value - 1U : 0U;
	case NPDISP_D3DSTENCILOP_INVERT: return (~value) & maxValue;
	case NPDISP_D3DSTENCILOP_INCR: return (value + 1U) & maxValue;
	case NPDISP_D3DSTENCILOP_DECR: return (value - 1U) & maxValue;
	default: return value;
	}
}

static void npdisp_d3d_sw_updateStencil(UINT8* zp, const NPDISP_D3D_RASTERSTATE* state, UINT32 op)
{
	UINT16 raw;
	UINT32 value;
	UINT32 updated;
	UINT32 maxValue;
	UINT32 writeMask;
	UINT32 shift;
	if (!zp || !state || !state->stencilBits || state->stencilBits > 4U) return;
	maxValue = (1U << state->stencilBits) - 1U;
	writeMask = state->stencilWriteMask & maxValue;
	if (!writeMask) return;
	shift = 16U - state->stencilBits;
	raw = npdisp_d3d_sw_readDepthStencil(zp);
	value = ((UINT32)raw >> shift) & maxValue;
	updated = npdisp_d3d_sw_stencilOp(value, state->stencilRef, op, maxValue);
	updated = (value & ~writeMask) | (updated & writeMask);
	raw = (UINT16)((raw & (UINT16)state->depthMask) | ((updated & maxValue) << shift));
	npdisp_d3d_sw_writeDepthStencil(zp, raw);
}

static bool npdisp_d3d_sw_depthStencilPass(UINT8* zp, UINT16 zValue, const NPDISP_D3D_RASTERSTATE* state)
{
	UINT16 raw;
	if (!state) return false;
	if (!state->zEnable && !state->stencilEnable) return true;
	if (!zp) return false;
	raw = npdisp_d3d_sw_readDepthStencil(zp);
	if (state->stencilEnable) {
		UINT32 maxValue;
		UINT32 readMask;
		UINT32 shift;
		UINT16 currentStencil;
		UINT16 refStencil;
		if (!state->stencilBits || state->stencilBits > 4U) return false;
		maxValue = (1U << state->stencilBits) - 1U;
		readMask = state->stencilReadMask & maxValue;
		shift = 16U - state->stencilBits;
		currentStencil = (UINT16)((((UINT32)raw >> shift) & maxValue) & readMask);
		refStencil = (UINT16)((state->stencilRef & maxValue) & readMask);
		if (!npdisp_d3d_sw_depthPass(refStencil, currentStencil, state->stencilFunc)) {
			npdisp_d3d_sw_updateStencil(zp, state, state->stencilFail);
			return false;
		}
	}
	if (state->zEnable && !npdisp_d3d_sw_depthPass(zValue, (UINT16)(raw & state->depthMask), state->zFunc)) {
		if (state->stencilEnable) npdisp_d3d_sw_updateStencil(zp, state, state->stencilZFail);
		return false;
	}
	if (state->stencilEnable) npdisp_d3d_sw_updateStencil(zp, state, state->stencilPass);
	return true;
}

static void npdisp_d3d_sw_writeDepth(UINT8* zp, UINT16 zValue, const NPDISP_D3D_RASTERSTATE* state)
{
	UINT16 raw;
	if (!zp || !state || !state->zEnable || !state->zWriteEnable) return;
	raw = npdisp_d3d_sw_readDepthStencil(zp);
	raw = (UINT16)((raw & ~(UINT16)state->depthMask) | (zValue & (UINT16)state->depthMask));
	npdisp_d3d_sw_writeDepthStencil(zp, raw);
}

static bool npdisp_d3d_sw_clipLine(float p, float q, float* t0, float* t1)
{
	float r;
	if (!t0 || !t1) return false;
	if (p == 0.0f) return q >= 0.0f;
	r = q / p;
	if (p < 0.0f) {
		if (r > *t1) return false;
		if (r > *t0) *t0 = r;
	}
	else {
		if (r < *t0) return false;
		if (r < *t1) *t1 = r;
	}
	return true;
}

bool npdisp_d3d_sw_point(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3D_VERTEX* vertex, const NPDISP_D3D_RASTERSTATE* state)
{
	UINT32 bytesPerPixel;
	SINT32 x, y;
	UINT32 color;
	UINT16 zValue = 0;
	UINT8* zp = NULL;
	UINT8* p;
	if (!target || !vertex || !state || !target->pixels || target->pitch <= 0) return false;
	bytesPerPixel = npdisp_d3d_sw_bytesPerPixel(target->bpp);
	if (!bytesPerPixel || !target->width || !target->height || (UINT64)target->width * bytesPerPixel > (UINT32)target->pitch) return false;
	if (!npdisp_d3d_sw_validCoord(vertex->x) || !npdisp_d3d_sw_validCoord(vertex->y) ||
		(state->zEnable && !npdisp_d3d_sw_validCoord(vertex->z))) return false;
	x = (SINT32)(vertex->x + 0.5f);
	y = (SINT32)(vertex->y + 0.5f);
	if (x < 0 || y < 0 || x >= (SINT32)target->width || y >= (SINT32)target->height) return true;
	if (state->zEnable || state->stencilEnable) {
		if (!depthTarget || !depthTarget->pixels || depthTarget->pitch <= 0 ||
			depthTarget->width < target->width || depthTarget->height < target->height ||
			(UINT64)depthTarget->width * 2U > (UINT32)depthTarget->pitch) return false;
		if (state->zEnable) zValue = npdisp_d3d_sw_depthValue(vertex->z, state->depthMask);
		zp = depthTarget->pixels + (size_t)y * (size_t)depthTarget->pitch + (size_t)x * 2U;
		if (!npdisp_d3d_sw_depthStencilPass(zp, zValue, state)) return true;
	}
	color = vertex->diffuse;
	{
		bool discard = false;
		if (!npdisp_d3d_sw_applyTextureStages(state, color, vertex->tu, vertex->tv, vertex->tw, vertex->tu2, vertex->tv2, vertex->tw2, NULL, NULL, &color, &discard)) return false;
		if (discard) return true;
	}
	if (state->specularEnable) color = npdisp_d3d_sw_addSpecular(color, vertex->specular);
	if (state->alphaTestEnable && !npdisp_d3d_sw_alphaPass((color >> 24) & 0xffU, state->alphaRef, state->alphaFunc)) return true;
	if (state->fogEnable) color = npdisp_d3d_sw_applyFog(color, state->fogColor, (float)((vertex->specular >> 24) & 0xffU) / 255.0f);
	if (state->zEnable && state->zWriteEnable) npdisp_d3d_sw_writeDepth(zp, zValue, state);
	p = target->pixels + (size_t)y * (size_t)target->pitch + (size_t)x * bytesPerPixel;
	return npdisp_d3d_sw_outputPixel(target, p, state, color);
}

bool npdisp_d3d_sw_line(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3D_VERTEX* v0, const NPDISP_D3D_VERTEX* v1, const NPDISP_D3D_RASTERSTATE* state)
{
	UINT32 bytesPerPixel;
	float dx, dy, t0 = 0.0f, t1 = 1.0f, clippedDx, clippedDy, adx, ady;
	UINT32 steps;
	UINT32 maxSteps;
	UINT32 sampleCount;
	float tu0 = v0 ? v0->tu : 0.0f;
	float tv0 = v0 ? v0->tv : 0.0f;
	float tw0 = v0 ? v0->tw : 0.0f;
	float tu1 = v1 ? v1->tu : 0.0f;
	float tv1 = v1 ? v1->tv : 0.0f;
	float tw1 = v1 ? v1->tw : 0.0f;
	float tu20 = v0 ? v0->tu2 : 0.0f;
	float tv20 = v0 ? v0->tv2 : 0.0f;
	float tw20 = v0 ? v0->tw2 : 0.0f;
	float tu21 = v1 ? v1->tu2 : 0.0f;
	float tv21 = v1 ? v1->tv2 : 0.0f;
	float tw21 = v1 ? v1->tw2 : 0.0f;
	if (!target || !v0 || !v1 || !state || !target->pixels || target->pitch <= 0) return false;
	bytesPerPixel = npdisp_d3d_sw_bytesPerPixel(target->bpp);
	if (!bytesPerPixel || !target->width || !target->height || (UINT64)target->width * bytesPerPixel > (UINT32)target->pitch) return false;
	if ((state->zEnable || state->stencilEnable) && (!depthTarget || !depthTarget->pixels || depthTarget->pitch <= 0 ||
		depthTarget->width < target->width || depthTarget->height < target->height || (UINT64)depthTarget->width * 2U > (UINT32)depthTarget->pitch)) return false;
	if (!npdisp_d3d_sw_validCoord(v0->x) || !npdisp_d3d_sw_validCoord(v0->y) ||
		!npdisp_d3d_sw_validCoord(v1->x) || !npdisp_d3d_sw_validCoord(v1->y) ||
		(state->zEnable && (!npdisp_d3d_sw_validCoord(v0->z) || !npdisp_d3d_sw_validCoord(v1->z)))) return false;
	dx = v1->x - v0->x;
	dy = v1->y - v0->y;
	if (!npdisp_d3d_sw_validCoord(dx) || !npdisp_d3d_sw_validCoord(dy)) return false;
	if (state->coordWrap[0] & 0x00000001UL) tu1 = npdisp_d3d_sw_adjustWrappedCoord(tu0, tu1);
	if (state->coordWrap[0] & 0x00000002UL) tv1 = npdisp_d3d_sw_adjustWrappedCoord(tv0, tv1);
	if (state->coordWrap[1] & 0x00000001UL) tu21 = npdisp_d3d_sw_adjustWrappedCoord(tu20, tu21);
	if (state->coordWrap[1] & 0x00000002UL) tv21 = npdisp_d3d_sw_adjustWrappedCoord(tv20, tv21);
	if (!npdisp_d3d_sw_clipLine(-dx, v0->x, &t0, &t1) ||
		!npdisp_d3d_sw_clipLine(dx, (float)(target->width - 1U) - v0->x, &t0, &t1) ||
		!npdisp_d3d_sw_clipLine(-dy, v0->y, &t0, &t1) ||
		!npdisp_d3d_sw_clipLine(dy, (float)(target->height - 1U) - v0->y, &t0, &t1)) return true;
	clippedDx = dx * (t1 - t0);
	clippedDy = dy * (t1 - t0);
	if (!npdisp_d3d_sw_validCoord(clippedDx) || !npdisp_d3d_sw_validCoord(clippedDy)) return false;
	adx = clippedDx < 0.0f ? -clippedDx : clippedDx;
	ady = clippedDy < 0.0f ? -clippedDy : clippedDy;
	maxSteps = (target->width > target->height ? target->width : target->height) - 1U;
	steps = (adx > ady ? adx : ady) >= (float)maxSteps ? maxSteps : (UINT32)((adx > ady ? adx : ady) + 0.5f);
	sampleCount = steps + 1U;
	for (UINT32 i = 0; i < sampleCount; ++i) {
		float u = steps ? (float)i / (float)steps : 0.0f;
		float t = t0 + (t1 - t0) * u;
		float xf = v0->x + dx * t;
		float yf = v0->y + dy * t;
		SINT32 x = (SINT32)(xf + 0.5f);
		SINT32 y = (SINT32)(yf + 0.5f);
		UINT32 color;
		UINT32 specular;
		UINT16 zValue = 0;
		UINT8* zp = NULL;
		if (x < 0 || y < 0 || x >= (SINT32)target->width || y >= (SINT32)target->height) continue;
		if (state->zEnable || state->stencilEnable) {
			if (state->zEnable) zValue = npdisp_d3d_sw_depthValue(v0->z + (v1->z - v0->z) * t, state->depthMask);
			zp = depthTarget->pixels + (size_t)y * (size_t)depthTarget->pitch + (size_t)x * 2U;
			if (!npdisp_d3d_sw_depthStencilPass(zp, zValue, state)) continue;
		}
		if (state->shadeMode == NPDISP_D3DSHADE_FLAT) {
			color = v0->diffuse;
			specular = v0->specular;
		}
		else {
			color = npdisp_d3d_sw_interpolateColor(v0->diffuse, v1->diffuse, v1->diffuse, 1.0f - t, t, 0.0f);
			specular = npdisp_d3d_sw_interpolateColor(v0->specular, v1->specular, v1->specular, 1.0f - t, t, 0.0f);
		}
		{
			float q0 = (1.0f - t) * v0->rhw;
			float q1 = t * v1->rhw;
			float q = q0 + q1;
			float tu, tv, tw, tu2, tv2, tw2;
			if (q > 0.0000001f || q < -0.0000001f) {
				tu = (tu0 * q0 + tu1 * q1) / q;
				tv = (tv0 * q0 + tv1 * q1) / q;
				tw = (tw0 * q0 + tw1 * q1) / q;
				tu2 = (tu20 * q0 + tu21 * q1) / q;
				tv2 = (tv20 * q0 + tv21 * q1) / q;
				tw2 = (tw20 * q0 + tw21 * q1) / q;
			}
			else {
				tu = tu0 * (1.0f - t) + tu1 * t;
				tv = tv0 * (1.0f - t) + tv1 * t;
				tw = tw0 * (1.0f - t) + tw1 * t;
				tu2 = tu20 * (1.0f - t) + tu21 * t;
				tv2 = tv20 * (1.0f - t) + tv21 * t;
				tw2 = tw20 * (1.0f - t) + tw21 * t;
			}
			bool discard = false;
			if (!npdisp_d3d_sw_applyTextureStages(state, color, tu, tv, tw, tu2, tv2, tw2, NULL, NULL, &color, &discard)) return false;
			if (discard) continue;
		}
		if (state->specularEnable) color = npdisp_d3d_sw_addSpecular(color, specular);
		if (state->alphaTestEnable && !npdisp_d3d_sw_alphaPass((color >> 24) & 0xffU, state->alphaRef, state->alphaFunc)) continue;
		if (state->fogEnable) {
			float fogFactor = ((float)((v0->specular >> 24) & 0xffU) * (1.0f - t) + (float)((v1->specular >> 24) & 0xffU) * t) / 255.0f;
			color = npdisp_d3d_sw_applyFog(color, state->fogColor, fogFactor);
		}
		if (state->zEnable && state->zWriteEnable) npdisp_d3d_sw_writeDepth(zp, zValue, state);
		UINT8* p = target->pixels + (size_t)y * (size_t)target->pitch + (size_t)x * bytesPerPixel;
		if (!npdisp_d3d_sw_outputPixel(target, p, state, color)) return false;
	}
	return true;
}

bool npdisp_d3d_sw_triangle(NPDISP_D3D_SW_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3D_VERTEX* v0, const NPDISP_D3D_VERTEX* v1, const NPDISP_D3D_VERTEX* v2, const NPDISP_D3D_RASTERSTATE* state)
{
	UINT32 bytesPerPixel;
	float area;
	float invArea;
	float minxf, minyf, maxxf, maxyf;
	float tu0 = v0 ? v0->tu : 0.0f;
	float tv0 = v0 ? v0->tv : 0.0f;
	float tw0 = v0 ? v0->tw : 0.0f;
	float tu1 = v1 ? v1->tu : 0.0f;
	float tv1 = v1 ? v1->tv : 0.0f;
	float tw1 = v1 ? v1->tw : 0.0f;
	float tu2 = v2 ? v2->tu : 0.0f;
	float tv2 = v2 ? v2->tv : 0.0f;
	float tw2 = v2 ? v2->tw : 0.0f;
	float su0 = v0 ? v0->tu2 : 0.0f;
	float sv0 = v0 ? v0->tv2 : 0.0f;
	float sw0 = v0 ? v0->tw2 : 0.0f;
	float su1 = v1 ? v1->tu2 : 0.0f;
	float sv1 = v1 ? v1->tv2 : 0.0f;
	float sw1 = v1 ? v1->tw2 : 0.0f;
	float su2 = v2 ? v2->tu2 : 0.0f;
	float sv2 = v2 ? v2->tv2 : 0.0f;
	float sw2 = v2 ? v2->tw2 : 0.0f;
	float dw0dx, dw0dy, dw1dx, dw1dy, dw2dx, dw2dy;
	float dQdx, dQdy;
	float dU0dx, dU0dy, dV0dx, dV0dy;
	float dU1dx, dU1dy, dV1dx, dV1dy;
	bool needGrad0 = false;
	bool needGrad1 = false;
	bool topLeft0, topLeft1, topLeft2;
	SINT32 left, top, right, bottom;
	if (!target || !v0 || !v1 || !v2 || !state || !target->pixels || target->pitch <= 0) return false;
	bytesPerPixel = npdisp_d3d_sw_bytesPerPixel(target->bpp);
	if (!bytesPerPixel || !target->width || !target->height || (UINT64)target->width * bytesPerPixel > (UINT32)target->pitch) return false;
	if ((state->zEnable || state->stencilEnable) && (!depthTarget || !depthTarget->pixels || depthTarget->pitch <= 0 ||
		depthTarget->width < target->width || depthTarget->height < target->height || (UINT64)depthTarget->width * 2U > (UINT32)depthTarget->pitch)) return false;
	if (!npdisp_d3d_sw_validCoord(v0->x) || !npdisp_d3d_sw_validCoord(v0->y) ||
		!npdisp_d3d_sw_validCoord(v1->x) || !npdisp_d3d_sw_validCoord(v1->y) ||
		!npdisp_d3d_sw_validCoord(v2->x) || !npdisp_d3d_sw_validCoord(v2->y)) return false;

	if (state->coordWrap[0] & 0x00000001UL) {
		tu1 = npdisp_d3d_sw_adjustWrappedCoord(tu0, tu1);
		tu2 = npdisp_d3d_sw_adjustWrappedCoord(tu0, tu2);
	}
	if (state->coordWrap[0] & 0x00000002UL) {
		tv1 = npdisp_d3d_sw_adjustWrappedCoord(tv0, tv1);
		tv2 = npdisp_d3d_sw_adjustWrappedCoord(tv0, tv2);
	}
	if (state->coordWrap[1] & 0x00000001UL) {
		su1 = npdisp_d3d_sw_adjustWrappedCoord(su0, su1);
		su2 = npdisp_d3d_sw_adjustWrappedCoord(su0, su2);
	}
	if (state->coordWrap[1] & 0x00000002UL) {
		sv1 = npdisp_d3d_sw_adjustWrappedCoord(sv0, sv1);
		sv2 = npdisp_d3d_sw_adjustWrappedCoord(sv0, sv2);
	}

	area = npdisp_d3d_sw_edge(v0->x, v0->y, v1->x, v1->y, v2->x, v2->y);
	if (area == 0.0f) return true;
	invArea = 1.0f / area;
	if (area > 0.0f) {
		topLeft0 = npdisp_d3d_sw_topLeftEdge(v1->x, v1->y, v2->x, v2->y);
		topLeft1 = npdisp_d3d_sw_topLeftEdge(v2->x, v2->y, v0->x, v0->y);
		topLeft2 = npdisp_d3d_sw_topLeftEdge(v0->x, v0->y, v1->x, v1->y);
	}
	else {
		topLeft0 = npdisp_d3d_sw_topLeftEdge(v2->x, v2->y, v1->x, v1->y);
		topLeft1 = npdisp_d3d_sw_topLeftEdge(v0->x, v0->y, v2->x, v2->y);
		topLeft2 = npdisp_d3d_sw_topLeftEdge(v1->x, v1->y, v0->x, v0->y);
	}
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		const NPDISP_D3D_TEXTURE* texture = state->textures[stage];
		if (!texture || texture->colorOp == NPDISP_D3DTOP_DISABLE) break;
		if (texture->mipCount > 1U && texture->mipFilter != NPDISP_D3DTFP_NONE) {
			if (texture->texCoordIndex) needGrad1 = true;
			else needGrad0 = true;
		}
	}
	{
		dw0dx = (v2->y - v1->y) * invArea;
		dw0dy = (v1->x - v2->x) * invArea;
		dw1dx = (v0->y - v2->y) * invArea;
		dw1dy = (v2->x - v0->x) * invArea;
		dw2dx = (v1->y - v0->y) * invArea;
		dw2dy = (v0->x - v1->x) * invArea;
		dQdx = dw0dx * v0->rhw + dw1dx * v1->rhw + dw2dx * v2->rhw;
		dQdy = dw0dy * v0->rhw + dw1dy * v1->rhw + dw2dy * v2->rhw;
		dU0dx = dw0dx * tu0 * v0->rhw + dw1dx * tu1 * v1->rhw + dw2dx * tu2 * v2->rhw;
		dU0dy = dw0dy * tu0 * v0->rhw + dw1dy * tu1 * v1->rhw + dw2dy * tu2 * v2->rhw;
		dV0dx = dw0dx * tv0 * v0->rhw + dw1dx * tv1 * v1->rhw + dw2dx * tv2 * v2->rhw;
		dV0dy = dw0dy * tv0 * v0->rhw + dw1dy * tv1 * v1->rhw + dw2dy * tv2 * v2->rhw;
		dU1dx = dw0dx * su0 * v0->rhw + dw1dx * su1 * v1->rhw + dw2dx * su2 * v2->rhw;
		dU1dy = dw0dy * su0 * v0->rhw + dw1dy * su1 * v1->rhw + dw2dy * su2 * v2->rhw;
		dV1dx = dw0dx * sv0 * v0->rhw + dw1dx * sv1 * v1->rhw + dw2dx * sv2 * v2->rhw;
		dV1dy = dw0dy * sv0 * v0->rhw + dw1dy * sv1 * v1->rhw + dw2dy * sv2 * v2->rhw;
	}
	if (state->cullMode == NPDISP_D3DCULL_CW && area < 0.0f) return true;
	if (state->cullMode == NPDISP_D3DCULL_CCW && area > 0.0f) return true;
	if (state->cullMode != NPDISP_D3DCULL_NONE && state->cullMode != NPDISP_D3DCULL_CW && state->cullMode != NPDISP_D3DCULL_CCW) return false;
	if (state->fillMode == NPDISP_D3DFILL_WIREFRAME) {
		if (!npdisp_d3d_sw_line(target, depthTarget, v0, v1, state)) return false;
		if (!npdisp_d3d_sw_line(target, depthTarget, v1, v2, state)) return false;
		return npdisp_d3d_sw_line(target, depthTarget, v2, v0, state);
	}

	minxf = v0->x; if (v1->x < minxf) minxf = v1->x; if (v2->x < minxf) minxf = v2->x;
	maxxf = v0->x; if (v1->x > maxxf) maxxf = v1->x; if (v2->x > maxxf) maxxf = v2->x;
	minyf = v0->y; if (v1->y < minyf) minyf = v1->y; if (v2->y < minyf) minyf = v2->y;
	maxyf = v0->y; if (v1->y > maxyf) maxyf = v1->y; if (v2->y > maxyf) maxyf = v2->y;
	if (maxxf < 0.0f || maxyf < 0.0f || minxf >= (float)target->width || minyf >= (float)target->height) return true;
	left = (minxf <= 0.0f) ? 0 : (SINT32)minxf;
	top = (minyf <= 0.0f) ? 0 : (SINT32)minyf;
	right = (maxxf >= (float)target->width) ? (SINT32)target->width : (SINT32)maxxf + 1;
	bottom = (maxyf >= (float)target->height) ? (SINT32)target->height : (SINT32)maxyf + 1;

	for (SINT32 y = top; y < bottom; ++y) {
		float e0 = npdisp_d3d_sw_edge(v1->x, v1->y, v2->x, v2->y, (float)left, (float)y);
		float e1 = npdisp_d3d_sw_edge(v2->x, v2->y, v0->x, v0->y, (float)left, (float)y);
		float e2 = npdisp_d3d_sw_edge(v0->x, v0->y, v1->x, v1->y, (float)left, (float)y);
		const float e0dx = v2->y - v1->y;
		const float e1dx = v0->y - v2->y;
		const float e2dx = v1->y - v0->y;
		for (SINT32 x = left; x < right; ++x, e0 += e0dx, e1 += e1dx, e2 += e2dx) {
			if ((area > 0.0f && (e0 < 0.0f || e1 < 0.0f || e2 < 0.0f ||
				(e0 == 0.0f && !topLeft0) || (e1 == 0.0f && !topLeft1) || (e2 == 0.0f && !topLeft2))) ||
				(area < 0.0f && (e0 > 0.0f || e1 > 0.0f || e2 > 0.0f ||
				(e0 == 0.0f && !topLeft0) || (e1 == 0.0f && !topLeft1) || (e2 == 0.0f && !topLeft2)))) continue;
			UINT32 color;
			UINT32 specular;
			UINT16 zValue = 0;
			UINT8* zp = NULL;
			float fogFactor;
			const float w0 = e0 * invArea;
			const float w1 = e1 * invArea;
			const float w2 = e2 * invArea;
			if (state->zEnable || state->stencilEnable) {
				if (state->zEnable) zValue = npdisp_d3d_sw_depthValue(v0->z * w0 + v1->z * w1 + v2->z * w2, state->depthMask);
				zp = depthTarget->pixels + (size_t)y * (size_t)depthTarget->pitch + (size_t)x * 2U;
				if (!npdisp_d3d_sw_depthStencilPass(zp, zValue, state)) continue;
			}
			if (state->shadeMode == NPDISP_D3DSHADE_FLAT) {
				color = v0->diffuse;
				specular = v0->specular;
			}
			else {
				color = npdisp_d3d_sw_interpolateColor(v0->diffuse, v1->diffuse, v2->diffuse, w0, w1, w2);
				specular = npdisp_d3d_sw_interpolateColor(v0->specular, v1->specular, v2->specular, w0, w1, w2);
			}
			{
				float q0 = w0 * v0->rhw;
				float q1 = w1 * v1->rhw;
				float q2 = w2 * v2->rhw;
				float q = q0 + q1 + q2;
				float numeratorU0 = tu0 * q0 + tu1 * q1 + tu2 * q2;
				float numeratorV0 = tv0 * q0 + tv1 * q1 + tv2 * q2;
				float numeratorU1 = su0 * q0 + su1 * q1 + su2 * q2;
				float numeratorV1 = sv0 * q0 + sv1 * q1 + sv2 * q2;
				float tu, tv, tw, tuB, tvB, twB;
				NPDISP_D3D_SW_TEXGRAD grad0;
				NPDISP_D3D_SW_TEXGRAD grad1;
				const NPDISP_D3D_SW_TEXGRAD* pGrad0 = NULL;
				const NPDISP_D3D_SW_TEXGRAD* pGrad1 = NULL;
				if (q > 0.0000001f || q < -0.0000001f) {
					const float invQ = 1.0f / q;
					const float invQ2 = invQ * invQ;
					tu = numeratorU0 * invQ;
					tv = numeratorV0 * invQ;
					tw = (tw0 * q0 + tw1 * q1 + tw2 * q2) * invQ;
					tuB = numeratorU1 * invQ;
					tvB = numeratorV1 * invQ;
					twB = (sw0 * q0 + sw1 * q1 + sw2 * q2) * invQ;
					if (needGrad0) {
						grad0.dudx = (dU0dx * q - numeratorU0 * dQdx) * invQ2;
						grad0.dvdx = (dV0dx * q - numeratorV0 * dQdx) * invQ2;
						grad0.dudy = (dU0dy * q - numeratorU0 * dQdy) * invQ2;
						grad0.dvdy = (dV0dy * q - numeratorV0 * dQdy) * invQ2;
						pGrad0 = &grad0;
					}
					if (needGrad1) {
						grad1.dudx = (dU1dx * q - numeratorU1 * dQdx) * invQ2;
						grad1.dvdx = (dV1dx * q - numeratorV1 * dQdx) * invQ2;
						grad1.dudy = (dU1dy * q - numeratorU1 * dQdy) * invQ2;
						grad1.dvdy = (dV1dy * q - numeratorV1 * dQdy) * invQ2;
						pGrad1 = &grad1;
					}
				}
				else {
					tu = tu0 * w0 + tu1 * w1 + tu2 * w2;
					tv = tv0 * w0 + tv1 * w1 + tv2 * w2;
					tw = tw0 * w0 + tw1 * w1 + tw2 * w2;
					tuB = su0 * w0 + su1 * w1 + su2 * w2;
					tvB = sv0 * w0 + sv1 * w1 + sv2 * w2;
					twB = sw0 * w0 + sw1 * w1 + sw2 * w2;
					if (needGrad0) {
						grad0.dudx = tu0 * dw0dx + tu1 * dw1dx + tu2 * dw2dx;
						grad0.dvdx = tv0 * dw0dx + tv1 * dw1dx + tv2 * dw2dx;
						grad0.dudy = tu0 * dw0dy + tu1 * dw1dy + tu2 * dw2dy;
						grad0.dvdy = tv0 * dw0dy + tv1 * dw1dy + tv2 * dw2dy;
						pGrad0 = &grad0;
					}
					if (needGrad1) {
						grad1.dudx = su0 * dw0dx + su1 * dw1dx + su2 * dw2dx;
						grad1.dvdx = sv0 * dw0dx + sv1 * dw1dx + sv2 * dw2dx;
						grad1.dudy = su0 * dw0dy + su1 * dw1dy + su2 * dw2dy;
						grad1.dvdy = sv0 * dw0dy + sv1 * dw1dy + sv2 * dw2dy;
						pGrad1 = &grad1;
					}
				}
				bool discard = false;
				if (!npdisp_d3d_sw_applyTextureStages(state, color, tu, tv, tw, tuB, tvB, twB, pGrad0, pGrad1, &color, &discard)) return false;
				if (discard) continue;
			}
			if (state->specularEnable) color = npdisp_d3d_sw_addSpecular(color, specular);
			if (state->alphaTestEnable && !npdisp_d3d_sw_alphaPass((color >> 24) & 0xffU, state->alphaRef, state->alphaFunc)) continue;
			if (state->fogEnable) {
				fogFactor = (((float)((v0->specular >> 24) & 0xffU) * w0) + ((float)((v1->specular >> 24) & 0xffU) * w1) + ((float)((v2->specular >> 24) & 0xffU) * w2)) / 255.0f;
				color = npdisp_d3d_sw_applyFog(color, state->fogColor, fogFactor);
			}
			if (state->zEnable && state->zWriteEnable) npdisp_d3d_sw_writeDepth(zp, zValue, state);
			UINT8* p = target->pixels + (size_t)y * (size_t)target->pitch + (size_t)x * bytesPerPixel;
			if (!npdisp_d3d_sw_outputPixel(target, p, state, color)) return false;
		}
	}
	return true;
}

#endif
