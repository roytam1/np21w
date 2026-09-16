/**
 * @file	npdisp_d3d.h
 * @brief	NPDISP Direct3D HAL interface
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_D3D)

typedef struct {
	float x;
	float y;
	float z;
	float rhw;
	UINT32 diffuse;
	UINT32 specular;
	float tu;
	float tv;
	float tu2;
	float tv2;
	float tw;
	float tw2;
} NPDISP_D3D_VERTEX;

#define NPDISP_D3D_TEXTURE_FORMAT_RGB16		0U
#define NPDISP_D3D_TEXTURE_FORMAT_U8V8		1U
#define NPDISP_D3D_TEXTURE_FORMAT_U5V5L6	2U
#define NPDISP_D3D_TEXTURE_FORMAT_RGB32		3U
#define NPDISP_D3D_TEXTURE_FORMAT_P8		4U
#define NPDISP_D3D_RASTER_TEXTURE_STAGES	3U
#define NPDISP_D3D_CUBE_FACES			6U
#define NPDISP_D3D_MAX_MIP_LEVELS		16U

typedef struct {
	const UINT8* pixels;
	const UINT8* cubePixels[NPDISP_D3D_CUBE_FACES];
	const UINT8* mipPixels[NPDISP_D3D_MAX_MIP_LEVELS];
	UINT32 mipWidth[NPDISP_D3D_MAX_MIP_LEVELS];
	UINT32 mipHeight[NPDISP_D3D_MAX_MIP_LEVELS];
	SINT32 mipPitch[NPDISP_D3D_MAX_MIP_LEVELS];
	UINT32 mipCount;
	UINT32 mipFilter;
	UINT32 width;
	UINT32 height;
	SINT32 pitch;
	UINT32 bpp;
	UINT32 rMask;
	UINT32 gMask;
	UINT32 bMask;
	UINT32 aMask;
	UINT32 duMask;
	UINT32 dvMask;
	UINT32 luminanceMask;
	UINT32 format;
	UINT32 texCoordIndex;
	UINT32 colorOp;
	UINT32 colorArg1;
	UINT32 colorArg2;
	UINT32 alphaOp;
	UINT32 alphaArg1;
	UINT32 alphaArg2;
	UINT32 addressU;
	UINT32 addressV;
	UINT32 magFilter;
	UINT32 minFilter;
	UINT32 colorKeyEnable;
	UINT32 colorKeyLow;
	UINT32 colorKeyHigh;
	UINT32 colorKeyMask;
	UINT32 palette[256];
	UINT32 paletteAlpha;
	UINT32 wrap;
	UINT32 cubeMap;
	float bumpMat00;
	float bumpMat01;
	float bumpMat10;
	float bumpMat11;
	float bumpLScale;
	float bumpLOffset;
} NPDISP_D3D_TEXTURE;

typedef struct {
	UINT32 fillMode;
	UINT32 shadeMode;
	UINT32 cullMode;
	UINT32 specularEnable;
	UINT32 alphaTestEnable;
	UINT32 alphaRef;
	UINT32 alphaFunc;
	UINT32 alphaBlendEnable;
	UINT32 srcBlend;
	UINT32 destBlend;
	UINT32 fogEnable;
	UINT32 fogColor;
	UINT32 zEnable;
	UINT32 zWriteEnable;
	UINT32 zFunc;
	UINT32 depthMask;
	UINT32 stencilBits;
	UINT32 stencilEnable;
	UINT32 stencilFunc;
	UINT32 stencilRef;
	UINT32 stencilReadMask;
	UINT32 stencilWriteMask;
	UINT32 stencilFail;
	UINT32 stencilZFail;
	UINT32 stencilPass;
	UINT32 coordWrap[2];
	const NPDISP_D3D_TEXTURE* textures[NPDISP_D3D_RASTER_TEXTURE_STAGES];
} NPDISP_D3D_RASTERSTATE;

bool npdisp_d3d_initializeHalMetadata(UINT32 globalAddr, UINT32 callbacksAddr, UINT32 bridgeInfoAddr);
bool npdisp_d3d_validateHalPointers(UINT32 globalAddr, UINT32 callbacksAddr);
UINT32 npdisp_d3d_getDriverInfo(UINT32 lpDataAddr);
void npdisp_d3d_reset(void);
void npdisp_d3d_flush(void);
void npdisp_d3d_poll(void);
UINT32 npdisp_d3d_stateSize(void);
bool npdisp_d3d_saveState(UINT8* dst, UINT32 size);
bool npdisp_d3d_loadState(const UINT8* src, UINT32 size);
void npdisp_d3d_destroySurface(UINT32 lpSurfaceAddr);
UINT32 npdisp_d3d_dispatch(UINT32 callbackId, UINT32 lpDataAddr);

#endif
