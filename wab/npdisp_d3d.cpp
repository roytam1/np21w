/**
 * @file	npdisp_d3d.cpp
 * @brief	NPDISP Direct3D HAL frontend
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_D3D)

#include	<deque>
#include	<math.h>
#include	<map>
#include	<vector>
#include	<process.h>

#include	"pccore.h"
#include	"cpucore.h"

#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_mem.h"
#include	"npdisp_d3d.h"
#include	"npdisp_d3d_sw.h"

extern NPDISP_WINDOWS npdispwin;

#if 0
static void npdisp_11_trace(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define TRACEOUT11(s) npdisp_11_trace s
#else
#define TRACEOUT11(s) (void)s
#endif

#if 0
static void npdisp_d3d_trace(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define TRACEOUTD3D(s) npdisp_d3d_trace s
#else
#define TRACEOUTD3D(s) (void)s
#endif

#if 0
static void npdisp_d3d_profTrace(const char* fmt, ...)
{
	char stmp[512];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define TRACEOUTD3DPROF(s) npdisp_d3d_profTrace s
#else
#define TRACEOUTD3DPROF(s) (void)s
#endif

#define NPDISP_D3D_MAX_RENDERSTATES	256U
#define NPDISP_D3DSTATE_OVERRIDE_BIAS	256U
#define NPDISP_D3DRENDERSTATE_ZVISIBLE	30U
#define NPDISP_D3DSTATUS_ZNOTVISIBLE	0x01000000UL
#define NPDISP_D3D_MAX_TEXTURE_STAGES	8U
#define NPDISP_D3D_MAX_TEXTURE_STAGE_STATES	(NPDISP_D3DTSS_MAX_STATE + 1U)
#define NPDISP_D3D_MAX_TEXTURESTATE_CHANGES	2048U
#define NPDISP_D3D_MAX_VERTICES		65536U
#define NPDISP_D3D_MAX_RECTS		65536U
#define NPDISP_D3D_MAX_DRAW_BLOCKS	4096U
#define NPDISP_D3D_MAX_DP2_COMMAND_BYTES	(16U * 1024U * 1024U)
#define NPDISP_D3D_MAX_VERTEX_SIZE	256U
#define NPDISP_D3D_MAX_TEXTURE_BYTES	(64U * 1024U * 1024U)
#define NPDISP_D3D_LEGACY_TLVERTEX_SIZE	32U
#define NPDISP_D3DRENDERSTATE_LASTPIXEL	16UL
#define NPDISP_D3DRENDERSTATE_DITHERENABLE	26UL
#define NPDISP_D3DRENDERSTATE_CLIPPING	136UL
#define NPDISP_D3DRENDERSTATE_LOCALVIEWER	142UL
#define NPDISP_D3DRENDERSTATE_DIFFUSEMATERIALSOURCE	145UL
#define NPDISP_D3DRENDERSTATE_SPECULARMATERIALSOURCE	146UL
#define NPDISP_D3DRENDERSTATE_AMBIENTMATERIALSOURCE	147UL
#define NPDISP_D3DRENDERSTATE_EMISSIVEMATERIALSOURCE	148UL
#define NPDISP_D3DRENDERSTATE_CLIPPLANEENABLE	152UL
#define NPDISP_D3DDP2OP_SETCLIPPLANE	44U
#define NPDISP_D3D_MAX_USER_CLIP_PLANES	4U
#define NPDISP_D3D_FRUSTUM_PLANES	6U
#define NPDISP_D3DMCS_MATERIAL	0UL
#define NPDISP_D3DMCS_COLOR1	1UL
#define NPDISP_D3DMCS_COLOR2	2UL

#define NPDISP_D3D_HAL_GLOBAL_SIZE				192U
#define NPDISP_D3D_HAL_CALLBACKS_SIZE			140U
#define NPDISP_D3D_TEXTURE_FORMAT_COUNT			6U

#define NPDISP_D3DDD_COLORMODEL					0x00000001UL
#define NPDISP_D3DDD_DEVCAPS					0x00000002UL
#define NPDISP_D3DDD_TRANSFORMCAPS				0x00000004UL
#define NPDISP_D3DDD_LIGHTINGCAPS				0x00000008UL
#define NPDISP_D3DDD_BCLIPPING					0x00000010UL
#define NPDISP_D3DDD_LINECAPS					0x00000020UL
#define NPDISP_D3DDD_TRICAPS					0x00000040UL
#define NPDISP_D3DDD_DEVICERENDERBITDEPTH		0x00000080UL
#define NPDISP_D3DDD_DEVICEZBUFFERBITDEPTH		0x00000100UL
#define NPDISP_D3DDD_MAXVERTEXCOUNT				0x00000400UL
#define NPDISP_D3DDEVCAPS_FLOATTLVERTEX			0x00000001UL
#define NPDISP_D3DDEVCAPS_EXECUTESYSTEMMEMORY	0x00000010UL
#define NPDISP_D3DDEVCAPS_TLVERTEXSYSTEMMEMORY	0x00000040UL
#define NPDISP_D3DDEVCAPS_TEXTURESYSTEMMEMORY	0x00000100UL
#define NPDISP_D3DDEVCAPS_TEXTUREVIDEOMEMORY	0x00000200UL
#define NPDISP_D3DDEVCAPS_DRAWPRIMTLVERTEX		0x00000400UL
#define NPDISP_D3DDEVCAPS_DRAWPRIMITIVES2		0x00002000UL
#define NPDISP_D3DDEVCAPS_DRAWPRIMITIVES2EX	0x00008000UL
#define NPDISP_D3DDEVCAPS_HWTRANSFORMANDLIGHT	0x00010000UL
#define NPDISP_D3DDEVCAPS_HWRASTERIZATION		0x00080000UL
#define NPDISP_D3DCOLOR_RGB						0x00000002UL
#define NPDISP_D3DPMISCCAPS_CULLNONE			0x00000010UL
#define NPDISP_D3DPMISCCAPS_CULLCW				0x00000020UL
#define NPDISP_D3DPMISCCAPS_CULLCCW				0x00000040UL
#define NPDISP_D3DPRASTERCAPS_ZTEST				0x00000010UL
#define NPDISP_D3DPRASTERCAPS_FOGVERTEX			0x00000080UL
#define NPDISP_D3DPRASTERCAPS_WBUFFER			0x00040000UL
#define NPDISP_D3DPCMPCAPS_ALL					0x000000ffUL
#define NPDISP_D3DPBLENDCAPS_ZERO				0x00000001UL
#define NPDISP_D3DPBLENDCAPS_ONE				0x00000002UL
#define NPDISP_D3DPBLENDCAPS_SRCCOLOR			0x00000004UL
#define NPDISP_D3DPBLENDCAPS_INVSRCCOLOR		0x00000008UL
#define NPDISP_D3DPBLENDCAPS_SRCALPHA			0x00000010UL
#define NPDISP_D3DPBLENDCAPS_INVSRCALPHA		0x00000020UL
#define NPDISP_D3DPBLENDCAPS_DESTCOLOR			0x00000100UL
#define NPDISP_D3DPBLENDCAPS_INVDESTCOLOR		0x00000200UL
#define NPDISP_D3DPSHADECAPS_COLORFLATRGB		0x00000002UL
#define NPDISP_D3DPSHADECAPS_COLORGOURAUDRGB	0x00000008UL
#define NPDISP_D3DPSHADECAPS_SPECULARFLATRGB	0x00000080UL
#define NPDISP_D3DPSHADECAPS_SPECULARGOURAUDRGB 0x00000200UL
#define NPDISP_D3DPSHADECAPS_ALPHAFLATBLEND		0x00001000UL
#define NPDISP_D3DPSHADECAPS_ALPHAGOURAUDBLEND	0x00004000UL
#define NPDISP_D3DPSHADECAPS_FOGGOURAUD			0x00080000UL
#define NPDISP_D3DPTEXTURECAPS_PERSPECTIVE		0x00000001UL
#define NPDISP_D3DPTEXTURECAPS_ALPHA			0x00000004UL
#define NPDISP_D3DPTEXTURECAPS_TRANSPARENCY		0x00000008UL
#define NPDISP_D3DPTEXTURECAPS_CUBEMAP			0x00000800UL
#define NPDISP_D3DPTEXTURECAPS_MIPMAP			0x00004000UL
#define NPDISP_D3DPTEXTURECAPS_NOPROJECTEDBUMPENV 0x00200000UL
#define NPDISP_D3DPTFILTERCAPS_NEAREST			0x00000001UL
#define NPDISP_D3DPTFILTERCAPS_LINEAR			0x00000002UL
#define NPDISP_D3DPTFILTERCAPS_MIPNEAREST		0x00000004UL
#define NPDISP_D3DPTFILTERCAPS_MIPLINEAR		0x00000008UL
#define NPDISP_D3DPTFILTERCAPS_LINEARMIPLINEAR	0x00000020UL
#define NPDISP_D3DPTFILTERCAPS_MINFPOINT		0x00000100UL
#define NPDISP_D3DPTFILTERCAPS_MINFLINEAR		0x00000200UL
#define NPDISP_D3DPTFILTERCAPS_MIPFPOINT		0x00010000UL
#define NPDISP_D3DPTFILTERCAPS_MIPFLINEAR		0x00020000UL
#define NPDISP_D3DPTFILTERCAPS_MAGFPOINT		0x01000000UL
#define NPDISP_D3DPTFILTERCAPS_MAGFLINEAR		0x02000000UL
#define NPDISP_D3DPTBLENDCAPS_MODULATE			0x00000002UL
#define NPDISP_D3DPTADDRESSCAPS_WRAP			0x00000001UL
#define NPDISP_D3DPTADDRESSCAPS_MIRROR			0x00000002UL
#define NPDISP_D3DPTADDRESSCAPS_CLAMP			0x00000004UL
#define NPDISP_D3DPTADDRESSCAPS_INDEPENDENTUV	0x00000010UL
#define NPDISP_D3DSTENCILCAPS_KEEP				0x00000001UL
#define NPDISP_D3DSTENCILCAPS_ZERO				0x00000002UL
#define NPDISP_D3DSTENCILCAPS_REPLACE			0x00000004UL
#define NPDISP_D3DSTENCILCAPS_INCRSAT			0x00000008UL
#define NPDISP_D3DSTENCILCAPS_DECRSAT			0x00000010UL
#define NPDISP_D3DSTENCILCAPS_INVERT			0x00000020UL
#define NPDISP_D3DSTENCILCAPS_INCR				0x00000040UL
#define NPDISP_D3DSTENCILCAPS_DECR				0x00000080UL
#define NPDISP_D3DVTXPCAPS_TEXGEN			0x00000001UL
#define NPDISP_D3DVTXPCAPS_DIRECTIONALLIGHTS	0x00000008UL
#define NPDISP_D3DVTXPCAPS_POSITIONALLIGHTS		0x00000010UL
#define NPDISP_D3DVTXPCAPS_MATERIALSOURCE7		0x00000002UL
#define NPDISP_D3DVTXPCAPS_LOCALVIEWER			0x00000020UL
#define NPDISP_D3D_MAX_ACTIVE_LIGHTS		8U
#define NPDISP_D3DTEXOPCAPS_DISABLE				0x00000001UL
#define NPDISP_D3DTEXOPCAPS_SELECTARG1			0x00000002UL
#define NPDISP_D3DTEXOPCAPS_SELECTARG2			0x00000004UL
#define NPDISP_D3DTEXOPCAPS_MODULATE			0x00000008UL
#define NPDISP_D3DTEXOPCAPS_ADD					0x00000040UL
#define NPDISP_D3DTEXOPCAPS_BLENDTEXTUREALPHA	0x00001000UL
#define NPDISP_D3DTEXOPCAPS_BUMPENVMAP			0x00200000UL
#define NPDISP_D3DTEXOPCAPS_BUMPENVMAPLUMINANCE 0x00400000UL
#define NPDISP_D3DTOP_BLENDTEXTUREALPHA		13U
#define NPDISP_D3DTSS_TCI_INDEXMASK			0x0000ffffUL
#define NPDISP_D3DTSS_TCI_GENMASK			0x000f0000UL
#define NPDISP_D3DTTFF_PROJECTED		0x00000100UL
#define NPDISP_DDSD_CAPS						0x00000001UL
#define NPDISP_D3DHAL2_CB32_SETRENDERTARGET		0x00000001UL
#define NPDISP_D3DHAL2_CB32_CLEAR				0x00000002UL
#define NPDISP_D3DHAL2_CB32_DRAWONEPRIMITIVE	0x00000004UL
#define NPDISP_D3DHAL2_CB32_DRAWONEINDEXEDPRIMITIVE 0x00000008UL
#define NPDISP_D3DHAL2_CB32_DRAWPRIMITIVES		0x00000010UL
#define NPDISP_D3DHAL3_CB32_CLEAR2				0x00000001UL
#define NPDISP_D3DHAL3_CB32_VALIDATETEXTURESTAGESTATE 0x00000004UL
#define NPDISP_D3DHAL3_CB32_DRAWPRIMITIVES2		0x00000008UL
#define NPDISP_DDHAL_MISC2CB32_ALPHABLT			0x00000001UL
#define NPDISP_DDHAL_MISC2CB32_CREATESURFACEEX	0x00000002UL
#define NPDISP_DDHAL_MISC2CB32_GETDRIVERSTATE	0x00000004UL
#define NPDISP_DDHAL_MISC2CB32_DESTROYDDLOCAL	0x00000008UL
#define NPDISP_D3DDEVINFOID_TEXTUREMANAGER		0x00000001UL
#define NPDISP_D3DDEVINFOID_D3DTEXTUREMANAGER	0x00000002UL
#define NPDISP_D3DDEVINFOID_TEXTURING			0x00000003UL
#define NPDISP_D3DHALDP2_W2K_DIRECTBUFFERS	0x80000000UL

typedef struct {
	UINT32 dwSize;
	UINT32 dwCaps;
} NPDISP_D3DTRANSFORMCAPS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwCaps;
	UINT32 dwLightingModel;
	UINT32 dwNumLights;
} NPDISP_D3DLIGHTINGCAPS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwMiscCaps;
	UINT32 dwRasterCaps;
	UINT32 dwZCmpCaps;
	UINT32 dwSrcBlendCaps;
	UINT32 dwDestBlendCaps;
	UINT32 dwAlphaCmpCaps;
	UINT32 dwShadeCaps;
	UINT32 dwTextureCaps;
	UINT32 dwTextureFilterCaps;
	UINT32 dwTextureBlendCaps;
	UINT32 dwTextureAddressCaps;
	UINT32 dwStippleWidth;
	UINT32 dwStippleHeight;
} NPDISP_D3DPRIMCAPS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 dcmColorModel;
	UINT32 dwDevCaps;
	NPDISP_D3DTRANSFORMCAPS32 dtcTransformCaps;
	UINT32 bClipping;
	NPDISP_D3DLIGHTINGCAPS32 dlcLightingCaps;
	NPDISP_D3DPRIMCAPS32 dpcLineCaps;
	NPDISP_D3DPRIMCAPS32 dpcTriCaps;
	UINT32 dwDeviceRenderBitDepth;
	UINT32 dwDeviceZBufferBitDepth;
	UINT32 dwMaxBufferSize;
	UINT32 dwMaxVertexCount;
} NPDISP_D3DDEVICEDESC_V1_32;

typedef struct {
	UINT32 dwSize;
	NPDISP_D3DDEVICEDESC_V1_32 hwCaps;
	UINT32 dwNumVertices;
	UINT32 dwNumClipVertices;
	UINT32 dwNumTextureFormats;
	UINT32 lpTextureFormats;
} NPDISP_D3DHAL_GLOBALDRIVERDATA32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 dwFourCC;
	UINT32 dwRGBBitCount;
	UINT32 dwRBitMask;
	UINT32 dwGBitMask;
	UINT32 dwBBitMask;
	UINT32 dwRGBAlphaBitMask;
} NPDISP_DDPIXELFORMAT_TEXTURE32;

typedef struct {
	UINT32 dwCaps;
} NPDISP_DDSCAPS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 dwHeight;
	UINT32 dwWidth;
	UINT32 lPitch;
	UINT32 dwBackBufferCount;
	UINT32 dwMipMapCount;
	UINT32 dwAlphaBitDepth;
	UINT32 dwReserved;
	UINT32 lpSurface;
	UINT32 ddckCKDestOverlay[2];
	UINT32 ddckCKDestBlt[2];
	UINT32 ddckCKSrcOverlay[2];
	UINT32 ddckCKSrcBlt[2];
	NPDISP_DDPIXELFORMAT_TEXTURE32 ddpfPixelFormat;
	NPDISP_DDSCAPS32 ddsCaps;
} NPDISP_DDSURFACEDESC32;

typedef struct {
	UINT32 dwSize;
	UINT32 ContextCreate;
	UINT32 ContextDestroy;
	UINT32 ContextDestroyAll;
	UINT32 SceneCapture;
	UINT32 lpReserved10;
	UINT32 lpReserved11;
	UINT32 RenderState;
	UINT32 RenderPrimitive;
	UINT32 dwReserved;
	UINT32 TextureCreate;
	UINT32 TextureDestroy;
	UINT32 TextureSwap;
	UINT32 TextureGetSurf;
	UINT32 lpReserved12;
	UINT32 lpReserved13;
	UINT32 lpReserved14;
	UINT32 lpReserved15;
	UINT32 lpReserved16;
	UINT32 lpReserved17;
	UINT32 lpReserved18;
	UINT32 lpReserved19;
	UINT32 lpReserved20;
	UINT32 lpReserved21;
	UINT32 GetState;
	UINT32 dwReserved0;
	UINT32 dwReserved1;
	UINT32 dwReserved2;
	UINT32 dwReserved3;
	UINT32 dwReserved4;
	UINT32 dwReserved5;
	UINT32 dwReserved6;
	UINT32 dwReserved7;
	UINT32 dwReserved8;
	UINT32 dwReserved9;
} NPDISP_D3DHAL_CALLBACKS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 SetRenderTarget;
	UINT32 Clear;
	UINT32 DrawOnePrimitive;
	UINT32 DrawOneIndexedPrimitive;
	UINT32 DrawPrimitives;
} NPDISP_D3DHAL_CALLBACKS2_32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 Clear2;
	UINT32 lpvReserved;
	UINT32 ValidateTextureStageState;
	UINT32 DrawPrimitives2;
} NPDISP_D3DHAL_CALLBACKS3_32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwMinTextureWidth;
	UINT32 dwMaxTextureWidth;
	UINT32 dwMinTextureHeight;
	UINT32 dwMaxTextureHeight;
	UINT32 dwMinStippleWidth;
	UINT32 dwMaxStippleWidth;
	UINT32 dwMinStippleHeight;
	UINT32 dwMaxStippleHeight;
	UINT32 dwMaxTextureRepeat;
	UINT32 dwMaxTextureAspectRatio;
	UINT32 dwMaxAnisotropy;
	float dvGuardBandLeft;
	float dvGuardBandTop;
	float dvGuardBandRight;
	float dvGuardBandBottom;
	float dvExtentsAdjust;
	UINT32 dwStencilCaps;
	UINT32 dwFVFCaps;
	UINT32 dwTextureOpCaps;
	UINT16 wMaxTextureBlendStages;
	UINT16 wMaxSimultaneousTextures;
	UINT32 dwMaxActiveLights;
	float dvMaxVertexW;
	UINT16 wMaxUserClipPlanes;
	UINT16 wMaxVertexBlendMatrices;
	UINT32 dwVertexProcessingCaps;
	UINT32 dwReserved1;
	UINT32 dwReserved2;
	UINT32 dwReserved3;
	UINT32 dwReserved4;
} NPDISP_D3DHAL_D3DEXTENDEDCAPS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 dwFourCC;
	UINT32 dwZBufferBitDepth;
	UINT32 dwStencilBitDepth;
	UINT32 dwZBitMask;
	UINT32 dwStencilBitMask;
	UINT32 dwRGBZBitMask;
} NPDISP_DDPIXELFORMAT32;

typedef struct {
	UINT32 dwCount;
	NPDISP_DDPIXELFORMAT32 formats[2];
} NPDISP_DDZPIXELFORMATS32;

typedef struct {
	UINT32 dwCaps2;
	UINT32 dwCaps3;
	UINT32 dwCaps4;
} NPDISP_DDSCAPSEX32;

typedef struct {
	NPDISP_DDSCAPSEX32 ddsCapsEx;
	NPDISP_DDSCAPSEX32 ddsCapsExAlt;
} NPDISP_DD_EXTENDEDHEAPRESTRICTIONS32;

typedef struct {
	UINT32 dwSize;
	NPDISP_DDSCAPSEX32 ddsCapsMore;
	NPDISP_DD_EXTENDEDHEAPRESTRICTIONS32 ddsExtendedHeapRestrictions[1];
} NPDISP_DD_MORESURFACECAPS32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	UINT32 AlphaBlt;
	UINT32 CreateSurfaceEx;
	UINT32 GetDriverState;
	UINT32 DestroyDDLocal;
} NPDISP_DD_MISCELLANEOUS2CALLBACKS32;

typedef struct {
	UINT32 dwFlags;
	UINT32 dwhContext;
	UINT32 lpdwStates;
	UINT32 dwLength;
	UINT32 ddRVal;
} NPDISP_DD_GETDRIVERSTATEDATA32;

typedef struct {
	UINT32 Data1;
	UINT16 Data2;
	UINT16 Data3;
	UINT8 Data4[8];
} NPDISP_GUID32;

typedef struct {
	UINT32 dwSize;
	UINT32 dwFlags;
	NPDISP_GUID32 guidInfo;
	UINT32 dwExpectedSize;
	UINT32 lpvData;
	UINT32 dwActualSize;
	UINT32 ddRVal;
	UINT32 dwContext;
} NPDISP_DD_GETDRIVERINFODATA32;

typedef char NPDISP_D3DDEVICEDESC_V1_32_SIZE_CHECK[(sizeof(NPDISP_D3DDEVICEDESC_V1_32) == 172) ? 1 : -1];
typedef char NPDISP_D3DHAL_GLOBALDRIVERDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_GLOBALDRIVERDATA32) == NPDISP_D3D_HAL_GLOBAL_SIZE) ? 1 : -1];
typedef char NPDISP_DDSURFACEDESC32_SIZE_CHECK[(sizeof(NPDISP_DDSURFACEDESC32) == NPDISP_D3D_TEXTURE_FORMAT_SIZE) ? 1 : -1];
typedef char NPDISP_D3DHAL_CALLBACKS32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_CALLBACKS32) == NPDISP_D3D_HAL_CALLBACKS_SIZE) ? 1 : -1];
typedef char NPDISP_D3DHAL_CALLBACKS2_32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_CALLBACKS2_32) == 28) ? 1 : -1];
typedef char NPDISP_D3DHAL_CALLBACKS3_32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_CALLBACKS3_32) == 24) ? 1 : -1];
typedef char NPDISP_D3DHAL_D3DEXTENDEDCAPS32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_D3DEXTENDEDCAPS32) == 116) ? 1 : -1];
typedef char NPDISP_DDZPIXELFORMATS32_SIZE_CHECK[(sizeof(NPDISP_DDZPIXELFORMATS32) == 68) ? 1 : -1];
typedef char NPDISP_DD_MORESURFACECAPS32_SIZE_CHECK[(sizeof(NPDISP_DD_MORESURFACECAPS32) == 40) ? 1 : -1];
typedef char NPDISP_DD_MISCELLANEOUS2CALLBACKS32_SIZE_CHECK[(sizeof(NPDISP_DD_MISCELLANEOUS2CALLBACKS32) == 24) ? 1 : -1];
typedef char NPDISP_DD_GETDRIVERINFODATA32_SIZE_CHECK[(sizeof(NPDISP_DD_GETDRIVERINFODATA32) == 44) ? 1 : -1];

typedef struct {
	UINT32 linearBase;
	UINT32 apertureOffset;
	UINT32 width;
	UINT32 height;
	SINT32 pitch;
	UINT32 bpp;
	bool systemMemory;
	bool valid;
} NPDISP_D3D_TARGET_SNAPSHOT;

typedef struct {
	UINT32 linearBase;
	UINT32 apertureOffset;
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
	bool systemMemory;
	bool valid;
} NPDISP_D3D_TEXTURE_SNAPSHOT;

typedef struct {
	UINT32 lpTarget;
	UINT32 lpDepth;
	NPDISP_D3D_TARGET_SNAPSHOT targetSnapshot;
	UINT32 ddLocalNamespace;
	UINT32 dwPID;
	UINT32 renderStates[NPDISP_D3D_MAX_RENDERSTATES];
	UINT8 renderStateOverrides[NPDISP_D3D_MAX_RENDERSTATES];
	UINT32 textureStageStates[NPDISP_D3D_MAX_TEXTURE_STAGES][NPDISP_D3D_MAX_TEXTURE_STAGE_STATES];
	UINT32 viewportX;
	UINT32 viewportY;
	UINT32 viewportWidth;
	UINT32 viewportHeight;
	float wNear;
	float wFar;
	float zMin;
	float zMax;
	bool sceneActive;
} NPDISP_D3D_CONTEXT;

typedef struct {
	NPDISP_D3DLIGHT7_32 light;
	bool valid;
	bool enabled;
} NPDISP_D3D_TNL_LIGHT;

typedef struct {
	NPDISP_D3DMATRIX32 world;
	NPDISP_D3DMATRIX32 worldBlend[3];
	NPDISP_D3DMATRIX32 view;
	NPDISP_D3DMATRIX32 projection;
	NPDISP_D3DMATRIX32 texture[8];
	NPDISP_D3DMATERIAL7_32 material;
	NPDISP_D3D_TNL_LIGHT lights[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
} NPDISP_D3D_TNL_CONTEXT;

typedef struct {
	float plane[4];
	bool valid;
} NPDISP_D3D_CLIP_PLANE;

typedef struct {
	NPDISP_D3D_CLIP_PLANE planes[NPDISP_D3D_MAX_USER_CLIP_PLANES];
} NPDISP_D3D_CLIP_CONTEXT;

typedef struct {
	NPDISP_D3DTLVERTEX32 vertex;
	float clip[4];
} NPDISP_D3D_CLIP_VERTEX;

typedef struct {
	UINT32 ddLocalNamespace;
	UINT32 handle;
	UINT32 lpSurface;
	UINT32 caps2;
	NPDISP_D3D_TARGET_SNAPSHOT targetSnapshot;
	NPDISP_D3D_TEXTURE_SNAPSHOT textureSnapshot;
} NPDISP_D3D_SURFACE;

#if defined(SUPPORT_NPDISP_W2K)
typedef struct {
	UINT32 ddLocalNamespace;
	UINT32 lpSurface;
	NPDISP_D3D_TARGET_SNAPSHOT snapshot;
	UINT32 depthMask;
	UINT32 stencilBits;
} NPDISP_D3D_W2K_DEPTH_SURFACE;
#endif

typedef struct {
	UINT32 dwhContext;
	UINT32 lpDDS;
	UINT32 lpSurface;
	NPDISP_D3D_TEXTURE_SNAPSHOT textureSnapshot;
} NPDISP_D3D_LEGACY_TEXTURE;

typedef struct {
	UINT32 flags;
	UINT32 entries[256];
} NPDISP_D3D_PALETTE;

typedef struct {
	NPDISP_D3D_SW_TARGET sw;
	UINT32 apertureOffset;
	bool visible;
} NPDISP_D3D_TARGET;

#define NPDISP_D3D_ASYNC_DRAW  1U
#define NPDISP_D3D_ASYNC_CLEAR 2U
#define NPDISP_D3D_ASYNC_MAX_COMMANDS 2048U
#define NPDISP_D3D_ASYNC_MAX_BYTES (64U * 1024U * 1024U)
#define NPDISP_D3D_ASYNC_WORKCLOCK_MAX_MS 50U

typedef struct {
	UINT32 type;
	UINT32 targetSurface;
	NPDISP_D3D_TARGET target;
	NPDISP_D3D_SW_DEPTH_TARGET depth;
	bool hasDepth;
	UINT32 primitiveType;
	NPDISP_D3D_RASTERSTATE state;
	NPDISP_D3D_TEXTURE textures[NPDISP_D3D_RASTER_TEXTURE_STAGES];
	UINT32 textureSurfaces[NPDISP_D3D_RASTER_TEXTURE_STAGES];
	std::vector<UINT8> textureStorage[NPDISP_D3D_RASTER_TEXTURE_STAGES];
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	std::vector<UINT16> indices;
	UINT32 clearFlags;
	UINT32 clearColor;
	UINT16 clearDepth;
	UINT32 clearStencil;
	std::vector<NPDISP_DDRECTL> rects;
	UINT32 bytes;
} NPDISP_D3D_ASYNC_COMMAND;

#define NPDISP_DDHAL_CREATESURFACEEX_SWAPHANDLES 0x00000001UL

static std::map<UINT32, NPDISP_D3D_CONTEXT> npdisp_d3d_contexts;
static std::map<UINT32, NPDISP_D3D_TNL_CONTEXT> npdisp_d3d_tnlContexts;
static std::map<UINT32, NPDISP_D3D_CLIP_CONTEXT> npdisp_d3d_clipContexts;
static std::map<UINT32, UINT32> npdisp_d3d_ddLocalNamespaces;
static std::map<UINT64, NPDISP_D3D_SURFACE> npdisp_d3d_surfaces;
#if defined(SUPPORT_NPDISP_W2K)
static std::map<UINT32, NPDISP_D3D_W2K_DEPTH_SURFACE> npdisp_d3d_w2kDepthSurfaces;
#endif
static std::map<UINT32, UINT32> npdisp_d3d_surfaceSwapPending;
static std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE> npdisp_d3d_legacyTextures;
static std::map<UINT64, NPDISP_D3D_PALETTE> npdisp_d3d_palettes;
static std::map<UINT64, UINT32> npdisp_d3d_surfacePalettes;
static UINT32 npdisp_d3d_nextContext = 1;
static UINT32 npdisp_d3d_nextDDLocalNamespace = 1;
static UINT32 npdisp_d3d_nextLegacyTexture = 1;
static bool npdisp_d3d_tnlEnabled = true;

#define NPDISP_D3D_STATE_MAGIC 0x31533344UL
#define NPDISP_D3D_STATE_VERSION 2U

typedef struct {
	UINT32 magic;
	UINT32 version;
	UINT32 nextContext;
	UINT32 nextDDLocalNamespace;
	UINT32 nextLegacyTexture;
	UINT32 contextCount;
	UINT32 namespaceCount;
	UINT32 surfaceCount;
	UINT32 swapCount;
	UINT32 legacyTextureCount;
	UINT32 paletteCount;
	UINT32 surfacePaletteCount;
} NPDISP_D3D_STATE_HEADER;

template<class K, class V> static bool npdisp_d3d_stateWriteMap(UINT8** dst, UINT32* remain, const std::map<K, V>& values)
{
	for (typename std::map<K, V>::const_iterator it = values.begin(); it != values.end(); ++it) {
		if (*remain < sizeof(K) + sizeof(V)) return false;
		memcpy(*dst, &it->first, sizeof(K));
		*dst += sizeof(K);
		memcpy(*dst, &it->second, sizeof(V));
		*dst += sizeof(V);
		*remain -= sizeof(K) + sizeof(V);
	}
	return true;
}

template<class K, class V> static bool npdisp_d3d_stateReadMap(const UINT8** src, UINT32* remain, UINT32 count, std::map<K, V>* values)
{
	if (!values || count > *remain / (sizeof(K) + sizeof(V))) return false;
	for (UINT32 i = 0; i < count; ++i) {
		K key;
		V value;
		memcpy(&key, *src, sizeof(K));
		*src += sizeof(K);
		memcpy(&value, *src, sizeof(V));
		*src += sizeof(V);
		*remain -= sizeof(K) + sizeof(V);
		(*values)[key] = value;
	}
	return true;
}

static CRITICAL_SECTION npdisp_d3d_asyncLock;
static HANDLE npdisp_d3d_asyncThread = NULL;
static HANDLE npdisp_d3d_asyncWake = NULL;
static HANDLE npdisp_d3d_asyncIdle = NULL;
static std::deque<NPDISP_D3D_ASYNC_COMMAND*> npdisp_d3d_asyncQueue;
static UINT32 npdisp_d3d_asyncBytes = 0;
static UINT32 npdisp_d3d_asyncActiveTarget = 0;
static UINT32 npdisp_d3d_asyncActiveTextures[NPDISP_D3D_RASTER_TEXTURE_STAGES] = { 0 };
static bool npdisp_d3d_asyncInitialized = false;
static bool npdisp_d3d_asyncStop = false;
static LONG npdisp_d3d_asyncVisibleDirty = 0;
static LONG npdisp_d3d_asyncError = 0;

static DWORD npdisp_d3d_profileTick = 0;
static UINT32 npdisp_d3d_profileFlushCalls = 0;
static UINT32 npdisp_d3d_profileBusyFlushCalls = 0;
static UINT64 npdisp_d3d_profileWaitUs = 0;
static UINT32 npdisp_d3d_profileWaitMaxUs = 0;
static UINT64 npdisp_d3d_profileWorkClock = 0;
static LONG npdisp_d3d_profileEnqueue = 0;
static LONG npdisp_d3d_profileEnqueueBytes = 0;
static LONG npdisp_d3d_profileDraw = 0;
static LONG npdisp_d3d_profileClear = 0;
static LONG npdisp_d3d_profileWorkerCommands = 0;
static LONG npdisp_d3d_profileWorkerMs = 0;
static LONG npdisp_d3d_profileRasterCalls = 0;
static LONG npdisp_d3d_profileTriangles = 0;
static LONG npdisp_d3d_profileDP2 = 0;
static LONG npdisp_d3d_profileBoxKPixels = 0;
static LONG npdisp_d3d_profileBoxMaxPixels = 0;
static LONG npdisp_d3d_profileBox4K = 0;
static LONG npdisp_d3d_profileBox16K = 0;
static LONG npdisp_d3d_profileBox64K = 0;
static LONG npdisp_d3d_profileTextureTriangles = 0;
static LONG npdisp_d3d_profileZTriangles = 0;
static LONG npdisp_d3d_profileBlendTriangles = 0;
static LONG npdisp_d3d_profileAlphaTestTriangles = 0;
static LONG npdisp_d3d_profileFogTriangles = 0;
static LONG npdisp_d3d_profileMipTriangles = 0;
static LONG npdisp_d3d_profileFilterPP = 0;
static LONG npdisp_d3d_profileFilterPL = 0;
static LONG npdisp_d3d_profileFilterLP = 0;
static LONG npdisp_d3d_profileFilterLL = 0;
static LONG npdisp_d3d_profileTex16Triangles = 0;
static LONG npdisp_d3d_profileTex32Triangles = 0;

static void npdisp_d3d_profileReport(DWORD now)
{
	UINT32 elapsed;
	LONG enqueue;
	LONG enqueueBytes;
	LONG draw;
	LONG clear;
	LONG workerCommands;
	LONG workerMs;
	LONG rasterCalls;
	LONG triangles;
	LONG dp2;
	LONG boxKPixels;
	LONG boxMaxPixels;
	LONG box4K;
	LONG box16K;
	LONG box64K;
	LONG textureTriangles;
	LONG zTriangles;
	LONG blendTriangles;
	LONG alphaTestTriangles;
	LONG fogTriangles;
	LONG mipTriangles;
	LONG filterPP;
	LONG filterPL;
	LONG filterLP;
	LONG filterLL;
	LONG tex16Triangles;
	LONG tex32Triangles;

	if (!npdisp_d3d_profileTick) {
		npdisp_d3d_profileTick = now;
		return;
	}
	elapsed = now - npdisp_d3d_profileTick;
	if (elapsed < 1000U) return;

	enqueue = InterlockedExchange(&npdisp_d3d_profileEnqueue, 0);
	enqueueBytes = InterlockedExchange(&npdisp_d3d_profileEnqueueBytes, 0);
	draw = InterlockedExchange(&npdisp_d3d_profileDraw, 0);
	clear = InterlockedExchange(&npdisp_d3d_profileClear, 0);
	workerCommands = InterlockedExchange(&npdisp_d3d_profileWorkerCommands, 0);
	workerMs = InterlockedExchange(&npdisp_d3d_profileWorkerMs, 0);
	rasterCalls = InterlockedExchange(&npdisp_d3d_profileRasterCalls, 0);
	triangles = InterlockedExchange(&npdisp_d3d_profileTriangles, 0);
	dp2 = InterlockedExchange(&npdisp_d3d_profileDP2, 0);
	boxKPixels = InterlockedExchange(&npdisp_d3d_profileBoxKPixels, 0);
	boxMaxPixels = InterlockedExchange(&npdisp_d3d_profileBoxMaxPixels, 0);
	box4K = InterlockedExchange(&npdisp_d3d_profileBox4K, 0);
	box16K = InterlockedExchange(&npdisp_d3d_profileBox16K, 0);
	box64K = InterlockedExchange(&npdisp_d3d_profileBox64K, 0);
	textureTriangles = InterlockedExchange(&npdisp_d3d_profileTextureTriangles, 0);
	zTriangles = InterlockedExchange(&npdisp_d3d_profileZTriangles, 0);
	blendTriangles = InterlockedExchange(&npdisp_d3d_profileBlendTriangles, 0);
	alphaTestTriangles = InterlockedExchange(&npdisp_d3d_profileAlphaTestTriangles, 0);
	fogTriangles = InterlockedExchange(&npdisp_d3d_profileFogTriangles, 0);
	mipTriangles = InterlockedExchange(&npdisp_d3d_profileMipTriangles, 0);
	filterPP = InterlockedExchange(&npdisp_d3d_profileFilterPP, 0);
	filterPL = InterlockedExchange(&npdisp_d3d_profileFilterPL, 0);
	filterLP = InterlockedExchange(&npdisp_d3d_profileFilterLP, 0);
	filterLL = InterlockedExchange(&npdisp_d3d_profileFilterLL, 0);
	tex16Triangles = InterlockedExchange(&npdisp_d3d_profileTex16Triangles, 0);
	tex32Triangles = InterlockedExchange(&npdisp_d3d_profileTex32Triangles, 0);

	TRACEOUTD3DPROF(("NPDISP11 D3D_PROF ms=%u flush=%u busy=%u wait_us=%llu wait_max=%u work=%llu enq=%ld bytes=%ld draw=%ld clear=%ld worker_cmd=%ld worker_ms=%ld dp2=%ld raster=%ld tri=%ld box_kpix=%ld box_max=%ld box4k=%ld box16k=%ld box64k=%ld tex=%ld z=%ld blend=%ld atest=%ld fog=%ld mip=%ld fpp=%ld fpl=%ld flp=%ld fll=%ld tex16=%ld tex32=%ld",
		elapsed, npdisp_d3d_profileFlushCalls, npdisp_d3d_profileBusyFlushCalls,
		(unsigned long long)npdisp_d3d_profileWaitUs, npdisp_d3d_profileWaitMaxUs,
		(unsigned long long)npdisp_d3d_profileWorkClock, enqueue, enqueueBytes, draw, clear,
		workerCommands, workerMs, dp2, rasterCalls, triangles, boxKPixels, boxMaxPixels,
		box4K, box16K, box64K, textureTriangles, zTriangles, blendTriangles,
		alphaTestTriangles, fogTriangles, mipTriangles, filterPP, filterPL, filterLP, filterLL,
		tex16Triangles, tex32Triangles));

	npdisp_d3d_profileTick = now;
	npdisp_d3d_profileFlushCalls = 0;
	npdisp_d3d_profileBusyFlushCalls = 0;
	npdisp_d3d_profileWaitUs = 0;
	npdisp_d3d_profileWaitMaxUs = 0;
	npdisp_d3d_profileWorkClock = 0;
}

static bool npdisp_d3d_asyncExecute(NPDISP_D3D_ASYNC_COMMAND* command);
static unsigned int __stdcall npdisp_d3d_asyncThreadProc(void*);
static bool npdisp_d3d_asyncStart(void);
static bool npdisp_d3d_asyncEnqueue(NPDISP_D3D_ASYNC_COMMAND* command);
static bool npdisp_d3d_asyncBusy(void);
static bool npdisp_d3d_asyncPendingTarget(UINT32 lpSurface);
static bool npdisp_d3d_asyncPendingSurface(UINT32 lpSurface);
static void npdisp_d3d_asyncStopWorker(void);
static void npdisp_d3d_tnlInitialize(NPDISP_D3D_TNL_CONTEXT* tnl);
static bool npdisp_d3d_attachedSurfaceByCaps(UINT32 lpSurface, UINT32 caps, UINT32* lpAttachedSurface);

static UINT64 npdisp_d3d_surfaceKey(UINT32 ddLocalNamespace, UINT32 handle)
{
	return ((UINT64)ddLocalNamespace << 32) | handle;
}

static bool npdisp_d3d_read(void *dst, UINT32 addr, UINT32 size)
{
	return addr && size <= 0x7fffffffUL && npdisp_readLinearMemory(dst, addr, (int)size);
}

static bool npdisp_d3d_write(void *src, UINT32 addr, UINT32 size)
{
	return addr && size <= 0x7fffffffUL && npdisp_writeLinearMemory(src, addr, (int)size);
}

static bool npdisp_d3d_add(UINT32 base, UINT32 offset, UINT32* result)
{
	if (!result || offset > 0xffffffffUL - base) return false;
	*result = base + offset;
	return true;
}


static bool npdisp_d3d_guidEqual(const NPDISP_GUID32* a, const NPDISP_GUID32* b)
{
	if (!a || !b || a->Data1 != b->Data1 || a->Data2 != b->Data2 || a->Data3 != b->Data3) return false;
	return memcmp(a->Data4, b->Data4, sizeof(a->Data4)) == 0;
}

static bool npdisp_d3d_readThunkTable(UINT32 bridgeInfoAddr, NPDISP_D3D_THUNK_TABLE32* thunks)
{
	NPDISP_DDBRIDGEINFO32 bridge = { 0 };
	if (!bridgeInfoAddr || !thunks || !npdisp_readMemory(&bridge, bridgeInfoAddr, sizeof(bridge))) return false;
	if (bridge.dwSize < sizeof(bridge) ||
		bridge.dwMagic != NPDISP_DDBRIDGE_V2_MAGIC ||
		bridge.dwStatus != NPDISP_DDBRIDGE_STATUS_DRIVER_READY ||
		!(bridge.dwNegotiatedFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_HAL) ||
		!(bridge.dwNegotiatedFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA) ||
		!(bridge.dwNegotiatedFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA) ||
		!bridge.lpD3DThunkTable ||
		bridge.dwD3DThunkTableSize < sizeof(*thunks) ||
		bridge.dwD3DThunkTableVersion != NPDISP_D3D_THUNK_TABLE_VERSION ||
		!npdisp_d3d_read(thunks, bridge.lpD3DThunkTable, sizeof(*thunks)) ||
		thunks->dwSize < sizeof(*thunks) ||
		thunks->dwVersion != NPDISP_D3D_THUNK_TABLE_VERSION) return false;
	return true;
}

#if defined(SUPPORT_NPDISP_W2K)
bool npdisp_d3d_getW2kCaps(NPDISP_W2K_D3D_CAPS* caps)
{
	if (!caps || npdisp.acceleration < NPDISP_ACCEL_DIRECT3D || npdisp.bpp < 15) return false;
	memset(caps, 0, sizeof(*caps));
	caps->Version = NPDISP_W2K_D3D_CAPS_VERSION;
	caps->Size = sizeof(*caps);
	caps->SetupInfo = NPDISP_W2K_D3D_SETUPINFO_HAL | NPDISP_W2K_D3D_SETUPINFO_GETDRIVERINFO | NPDISP_W2K_D3D_SETUPINFO_CALLBACKS_ACTIVE;
	caps->CallbackMask = NPDISP_W2K_D3D_CB_CONTEXTCREATE | NPDISP_W2K_D3D_CB_CONTEXTDESTROY |
		NPDISP_W2K_D3D_CB_VALIDATETEXTURESTAGE | NPDISP_W2K_D3D_CB_DRAWPRIMITIVES2 |
		NPDISP_W2K_D3D_CB_CREATESURFACEEX | NPDISP_W2K_D3D_CB_GETDRIVERSTATE | NPDISP_W2K_D3D_CB_DESTROYDDLOCAL;
	caps->GetDriverInfoMask = NPDISP_W2K_D3D_GETINFO_CALLBACKS | NPDISP_W2K_D3D_GETINFO_CALLBACKS3 |
		NPDISP_W2K_D3D_GETINFO_EXTENDEDCAPS | NPDISP_W2K_D3D_GETINFO_MISC2 | NPDISP_W2K_D3D_GETINFO_ZPIXELFORMATS;

	caps->HwCaps.dwSize = sizeof(caps->HwCaps);
	caps->HwCaps.dwFlags = NPDISP_W2K_D3D_D3DDD_COLORMODEL | NPDISP_W2K_D3D_D3DDD_DEVCAPS |
		NPDISP_W2K_D3D_D3DDD_TRANSFORMCAPS | NPDISP_W2K_D3D_D3DDD_LIGHTINGCAPS |
		NPDISP_W2K_D3D_D3DDD_BCLIPPING | NPDISP_W2K_D3D_D3DDD_LINECAPS |
		NPDISP_W2K_D3D_D3DDD_TRICAPS | NPDISP_W2K_D3D_D3DDD_DEVICERENDERBITDEPTH |
		NPDISP_W2K_D3D_D3DDD_DEVICEZBUFFERBITDEPTH | NPDISP_W2K_D3D_D3DDD_MAXVERTEXCOUNT;
	caps->HwCaps.dcmColorModel = NPDISP_W2K_D3D_D3DCOLOR_RGB;
	caps->HwCaps.dwDevCaps = NPDISP_W2K_D3D_D3DDEVCAPS_FLOATTLVERTEX |
		NPDISP_W2K_D3D_D3DDEVCAPS_EXECUTESYSTEMMEMORY |
		NPDISP_W2K_D3D_D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
		NPDISP_D3DDEVCAPS_TEXTURESYSTEMMEMORY | NPDISP_D3DDEVCAPS_TEXTUREVIDEOMEMORY |
		NPDISP_W2K_D3D_D3DDEVCAPS_DRAWPRIMTLVERTEX | NPDISP_W2K_D3D_D3DDEVCAPS_CANRENDERAFTERFLIP |
		NPDISP_W2K_D3D_D3DDEVCAPS_DRAWPRIMITIVES2 | NPDISP_D3DDEVCAPS_DRAWPRIMITIVES2EX |
		NPDISP_W2K_D3D_D3DDEVCAPS_HWRASTERIZATION;
	caps->HwCaps.dtcTransformCaps.dwSize = sizeof(caps->HwCaps.dtcTransformCaps);
	caps->HwCaps.bClipping = 1;
	caps->HwCaps.dlcLightingCaps.dwSize = sizeof(caps->HwCaps.dlcLightingCaps);
	caps->HwCaps.dpcLineCaps.dwSize = sizeof(caps->HwCaps.dpcLineCaps);
	caps->HwCaps.dpcLineCaps.dwMiscCaps = NPDISP_W2K_D3D_D3DPMISCCAPS_CULLNONE;
	caps->HwCaps.dpcLineCaps.dwRasterCaps = NPDISP_W2K_D3D_D3DPRASTERCAPS_ZTEST | NPDISP_D3DPRASTERCAPS_FOGVERTEX | NPDISP_D3DPRASTERCAPS_WBUFFER;
	caps->HwCaps.dpcLineCaps.dwZCmpCaps = NPDISP_W2K_D3D_D3DPCMPCAPS_ALL;
	caps->HwCaps.dpcLineCaps.dwSrcBlendCaps = NPDISP_D3DPBLENDCAPS_ZERO | NPDISP_D3DPBLENDCAPS_ONE |
		NPDISP_D3DPBLENDCAPS_SRCCOLOR | NPDISP_D3DPBLENDCAPS_INVSRCCOLOR |
		NPDISP_D3DPBLENDCAPS_SRCALPHA | NPDISP_D3DPBLENDCAPS_INVSRCALPHA |
		NPDISP_D3DPBLENDCAPS_DESTCOLOR | NPDISP_D3DPBLENDCAPS_INVDESTCOLOR;
	caps->HwCaps.dpcLineCaps.dwDestBlendCaps = caps->HwCaps.dpcLineCaps.dwSrcBlendCaps;
	caps->HwCaps.dpcLineCaps.dwAlphaCmpCaps = NPDISP_D3DPCMPCAPS_ALL;
	caps->HwCaps.dpcLineCaps.dwShadeCaps = NPDISP_W2K_D3D_D3DPSHADECAPS_COLORFLATRGB |
		NPDISP_W2K_D3D_D3DPSHADECAPS_COLORGOURAUDRGB | NPDISP_D3DPSHADECAPS_SPECULARFLATRGB |
		NPDISP_D3DPSHADECAPS_SPECULARGOURAUDRGB | NPDISP_D3DPSHADECAPS_ALPHAFLATBLEND |
		NPDISP_D3DPSHADECAPS_ALPHAGOURAUDBLEND | NPDISP_D3DPSHADECAPS_FOGGOURAUD;
	caps->HwCaps.dpcLineCaps.dwTextureCaps = NPDISP_D3DPTEXTURECAPS_PERSPECTIVE |
		NPDISP_D3DPTEXTURECAPS_ALPHA | NPDISP_D3DPTEXTURECAPS_TRANSPARENCY;
	caps->HwCaps.dpcLineCaps.dwTextureFilterCaps = NPDISP_D3DPTFILTERCAPS_NEAREST |
		NPDISP_D3DPTFILTERCAPS_LINEAR | NPDISP_D3DPTFILTERCAPS_MINFPOINT |
		NPDISP_D3DPTFILTERCAPS_MINFLINEAR | NPDISP_D3DPTFILTERCAPS_MAGFPOINT |
		NPDISP_D3DPTFILTERCAPS_MAGFLINEAR;
	caps->HwCaps.dpcLineCaps.dwTextureBlendCaps = NPDISP_D3DPTBLENDCAPS_MODULATE;
	caps->HwCaps.dpcLineCaps.dwTextureAddressCaps = NPDISP_D3DPTADDRESSCAPS_WRAP |
		NPDISP_D3DPTADDRESSCAPS_MIRROR | NPDISP_D3DPTADDRESSCAPS_CLAMP |
		NPDISP_D3DPTADDRESSCAPS_INDEPENDENTUV;
	caps->HwCaps.dpcTriCaps = caps->HwCaps.dpcLineCaps;
	caps->HwCaps.dpcTriCaps.dwMiscCaps = NPDISP_W2K_D3D_D3DPMISCCAPS_CULLNONE |
		NPDISP_W2K_D3D_D3DPMISCCAPS_CULLCW | NPDISP_W2K_D3D_D3DPMISCCAPS_CULLCCW;
	caps->HwCaps.dpcTriCaps.dwTextureCaps |= NPDISP_D3DPTEXTURECAPS_MIPMAP;
	caps->HwCaps.dpcTriCaps.dwTextureFilterCaps |= NPDISP_D3DPTFILTERCAPS_MIPNEAREST |
		NPDISP_D3DPTFILTERCAPS_MIPLINEAR | NPDISP_D3DPTFILTERCAPS_LINEARMIPLINEAR |
		NPDISP_D3DPTFILTERCAPS_MIPFPOINT | NPDISP_D3DPTFILTERCAPS_MIPFLINEAR;
	caps->HwCaps.dwDeviceRenderBitDepth = NPDISP_W2K_D3D_DDBD_16 | NPDISP_W2K_D3D_DDBD_24 | NPDISP_W2K_D3D_DDBD_32;
	caps->HwCaps.dwDeviceZBufferBitDepth = NPDISP_W2K_D3D_DDBD_16;
	caps->HwCaps.dwMaxVertexCount = 63488UL;

	caps->ExtendedCaps.dwSize = sizeof(caps->ExtendedCaps);
	caps->ExtendedCaps.dwMinTextureWidth = 1;
	caps->ExtendedCaps.dwMaxTextureWidth = 1024;
	caps->ExtendedCaps.dwMinTextureHeight = 1;
	caps->ExtendedCaps.dwMaxTextureHeight = 1024;
	caps->ExtendedCaps.dwMaxTextureRepeat = 1024;
	caps->ExtendedCaps.dwMaxTextureAspectRatio = 1024;
	caps->ExtendedCaps.dwMaxAnisotropy = 1;
	caps->ExtendedCaps.dwFVFCaps = NPDISP_D3D_TEXTURE_COORD_SETS;
	caps->ExtendedCaps.dwTextureOpCaps = NPDISP_D3DTEXOPCAPS_DISABLE | NPDISP_D3DTEXOPCAPS_SELECTARG1 |
		NPDISP_D3DTEXOPCAPS_SELECTARG2 | NPDISP_D3DTEXOPCAPS_MODULATE | NPDISP_D3DTEXOPCAPS_ADD |
		NPDISP_D3DTEXOPCAPS_BLENDTEXTUREALPHA | NPDISP_D3DTEXOPCAPS_BUMPENVMAP |
		NPDISP_D3DTEXOPCAPS_BUMPENVMAPLUMINANCE;
	caps->ExtendedCaps.wMaxTextureBlendStages = 3;
	caps->ExtendedCaps.wMaxSimultaneousTextures = 3;
	caps->ExtendedCaps.dvMaxVertexW = 1.0f;

	caps->TextureFormatCount = 5;
	caps->TextureFormats[0].dwFlags = NPDISP_W2K_D3D_DDSD_CAPS | NPDISP_W2K_D3D_DDSD_PIXELFORMAT;
	caps->TextureFormats[0].dwPixelFormatFlags = NPDISP_W2K_D3D_DDPF_RGB;
	caps->TextureFormats[0].dwRGBBitCount = 16;
	caps->TextureFormats[0].dwRBitMask = 0x0000f800UL;
	caps->TextureFormats[0].dwGBitMask = 0x000007e0UL;
	caps->TextureFormats[0].dwBBitMask = 0x0000001fUL;
	caps->TextureFormats[0].dwSurfaceCaps = NPDISP_W2K_D3D_DDSCAPS_TEXTURE;
	caps->TextureFormats[1] = caps->TextureFormats[0];
	caps->TextureFormats[1].dwPixelFormatFlags |= NPDISP_W2K_D3D_DDPF_ALPHAPIXELS;
	caps->TextureFormats[1].dwRBitMask = 0x00007c00UL;
	caps->TextureFormats[1].dwGBitMask = 0x000003e0UL;
	caps->TextureFormats[1].dwRGBAlphaBitMask = 0x00008000UL;
	caps->TextureFormats[2] = caps->TextureFormats[1];
	caps->TextureFormats[2].dwRBitMask = 0x00000f00UL;
	caps->TextureFormats[2].dwGBitMask = 0x000000f0UL;
	caps->TextureFormats[2].dwBBitMask = 0x0000000fUL;
	caps->TextureFormats[2].dwRGBAlphaBitMask = 0x0000f000UL;
	caps->TextureFormats[3] = caps->TextureFormats[0];
	caps->TextureFormats[3].dwPixelFormatFlags = NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE;
	caps->TextureFormats[3].dwRBitMask = 0x0000001fUL;
	caps->TextureFormats[3].dwGBitMask = 0x000003e0UL;
	caps->TextureFormats[3].dwBBitMask = 0x0000fc00UL;
	caps->TextureFormats[4] = caps->TextureFormats[1];
	caps->TextureFormats[4].dwRGBBitCount = 32;
	caps->TextureFormats[4].dwRBitMask = 0x00ff0000UL;
	caps->TextureFormats[4].dwGBitMask = 0x0000ff00UL;
	caps->TextureFormats[4].dwBBitMask = 0x000000ffUL;
	caps->TextureFormats[4].dwRGBAlphaBitMask = 0xff000000UL;
	return true;
}
#endif

bool npdisp_d3d_initializeHalMetadata(UINT32 globalAddr, UINT32 callbacksAddr, UINT32 bridgeInfoAddr)
{
	NPDISP_D3D_THUNK_TABLE32 thunks = { 0 };
	NPDISP_D3DHAL_GLOBALDRIVERDATA32 global = { 0 };
	NPDISP_D3DHAL_CALLBACKS32 callbacks = { 0 };
	NPDISP_DDSURFACEDESC32 formats[NPDISP_D3D_TEXTURE_FORMAT_COUNT] = { 0 };
	UINT32 formatAddr;

	if (npdisp.version < 16 || !globalAddr || !callbacksAddr ||
		!npdisp_d3d_readThunkTable(bridgeInfoAddr, &thunks) ||
		globalAddr > 0xffffffffUL - NPDISP_D3D_GLOBALDRIVERDATA_SIZE) return false;
	formatAddr = globalAddr + NPDISP_D3D_GLOBALDRIVERDATA_SIZE;

	global.dwSize = sizeof(global);
	global.hwCaps.dwSize = sizeof(global.hwCaps);
	global.hwCaps.dwFlags = NPDISP_D3DDD_COLORMODEL | NPDISP_D3DDD_DEVCAPS |
		NPDISP_D3DDD_TRANSFORMCAPS | NPDISP_D3DDD_LIGHTINGCAPS | NPDISP_D3DDD_BCLIPPING |
		NPDISP_D3DDD_LINECAPS | NPDISP_D3DDD_TRICAPS | NPDISP_D3DDD_DEVICERENDERBITDEPTH |
		NPDISP_D3DDD_DEVICEZBUFFERBITDEPTH | NPDISP_D3DDD_MAXVERTEXCOUNT;
	global.hwCaps.dcmColorModel = NPDISP_D3DCOLOR_RGB;
	global.hwCaps.dwDevCaps = NPDISP_D3DDEVCAPS_FLOATTLVERTEX |
		NPDISP_D3DDEVCAPS_EXECUTESYSTEMMEMORY | NPDISP_D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
		NPDISP_D3DDEVCAPS_TEXTURESYSTEMMEMORY | NPDISP_D3DDEVCAPS_TEXTUREVIDEOMEMORY |
		NPDISP_D3DDEVCAPS_DRAWPRIMTLVERTEX | NPDISP_D3DDEVCAPS_DRAWPRIMITIVES2 |
		NPDISP_D3DDEVCAPS_DRAWPRIMITIVES2EX | NPDISP_D3DDEVCAPS_HWRASTERIZATION;
	if (npdisp_d3d_tnlEnabled) global.hwCaps.dwDevCaps |= NPDISP_D3DDEVCAPS_HWTRANSFORMANDLIGHT;
	global.hwCaps.dtcTransformCaps.dwSize = sizeof(global.hwCaps.dtcTransformCaps);
	global.hwCaps.bClipping = 1;
	global.hwCaps.dlcLightingCaps.dwSize = sizeof(global.hwCaps.dlcLightingCaps);
	global.hwCaps.dpcLineCaps.dwSize = sizeof(global.hwCaps.dpcLineCaps);
	global.hwCaps.dpcLineCaps.dwRasterCaps = NPDISP_D3DPRASTERCAPS_ZTEST | NPDISP_D3DPRASTERCAPS_FOGVERTEX | NPDISP_D3DPRASTERCAPS_WBUFFER;
	global.hwCaps.dpcLineCaps.dwZCmpCaps = NPDISP_D3DPCMPCAPS_ALL;
	global.hwCaps.dpcLineCaps.dwSrcBlendCaps = NPDISP_D3DPBLENDCAPS_ZERO | NPDISP_D3DPBLENDCAPS_ONE |
		NPDISP_D3DPBLENDCAPS_SRCCOLOR | NPDISP_D3DPBLENDCAPS_INVSRCCOLOR |
		NPDISP_D3DPBLENDCAPS_SRCALPHA | NPDISP_D3DPBLENDCAPS_INVSRCALPHA |
		NPDISP_D3DPBLENDCAPS_DESTCOLOR | NPDISP_D3DPBLENDCAPS_INVDESTCOLOR;
	global.hwCaps.dpcLineCaps.dwDestBlendCaps = global.hwCaps.dpcLineCaps.dwSrcBlendCaps;
	global.hwCaps.dpcLineCaps.dwAlphaCmpCaps = NPDISP_D3DPCMPCAPS_ALL;
	global.hwCaps.dpcLineCaps.dwShadeCaps = NPDISP_D3DPSHADECAPS_COLORFLATRGB |
		NPDISP_D3DPSHADECAPS_COLORGOURAUDRGB | NPDISP_D3DPSHADECAPS_SPECULARFLATRGB |
		NPDISP_D3DPSHADECAPS_SPECULARGOURAUDRGB | NPDISP_D3DPSHADECAPS_ALPHAFLATBLEND |
		NPDISP_D3DPSHADECAPS_ALPHAGOURAUDBLEND | NPDISP_D3DPSHADECAPS_FOGGOURAUD;
	global.hwCaps.dpcLineCaps.dwTextureCaps = NPDISP_D3DPTEXTURECAPS_PERSPECTIVE |
		NPDISP_D3DPTEXTURECAPS_ALPHA | NPDISP_D3DPTEXTURECAPS_TRANSPARENCY |
		NPDISP_D3DPTEXTURECAPS_CUBEMAP | NPDISP_D3DPTEXTURECAPS_NOPROJECTEDBUMPENV;
	global.hwCaps.dpcLineCaps.dwTextureFilterCaps = NPDISP_D3DPTFILTERCAPS_NEAREST |
		NPDISP_D3DPTFILTERCAPS_LINEAR | NPDISP_D3DPTFILTERCAPS_MINFPOINT |
		NPDISP_D3DPTFILTERCAPS_MINFLINEAR | NPDISP_D3DPTFILTERCAPS_MAGFPOINT |
		NPDISP_D3DPTFILTERCAPS_MAGFLINEAR;
	global.hwCaps.dpcLineCaps.dwTextureBlendCaps = NPDISP_D3DPTBLENDCAPS_MODULATE;
	global.hwCaps.dpcLineCaps.dwTextureAddressCaps = NPDISP_D3DPTADDRESSCAPS_WRAP |
		NPDISP_D3DPTADDRESSCAPS_MIRROR | NPDISP_D3DPTADDRESSCAPS_CLAMP |
		NPDISP_D3DPTADDRESSCAPS_INDEPENDENTUV;

	global.hwCaps.dpcTriCaps.dwSize = sizeof(global.hwCaps.dpcTriCaps);
	global.hwCaps.dpcTriCaps.dwMiscCaps = NPDISP_D3DPMISCCAPS_CULLNONE |
		NPDISP_D3DPMISCCAPS_CULLCW | NPDISP_D3DPMISCCAPS_CULLCCW;
	global.hwCaps.dpcTriCaps.dwRasterCaps = NPDISP_D3DPRASTERCAPS_ZTEST | NPDISP_D3DPRASTERCAPS_FOGVERTEX | NPDISP_D3DPRASTERCAPS_WBUFFER;
	global.hwCaps.dpcTriCaps.dwZCmpCaps = NPDISP_D3DPCMPCAPS_ALL;
	global.hwCaps.dpcTriCaps.dwSrcBlendCaps = global.hwCaps.dpcLineCaps.dwSrcBlendCaps;
	global.hwCaps.dpcTriCaps.dwDestBlendCaps = global.hwCaps.dpcTriCaps.dwSrcBlendCaps;
	global.hwCaps.dpcTriCaps.dwAlphaCmpCaps = NPDISP_D3DPCMPCAPS_ALL;
	global.hwCaps.dpcTriCaps.dwShadeCaps = global.hwCaps.dpcLineCaps.dwShadeCaps;
	global.hwCaps.dpcTriCaps.dwTextureCaps = NPDISP_D3DPTEXTURECAPS_PERSPECTIVE |
		NPDISP_D3DPTEXTURECAPS_ALPHA | NPDISP_D3DPTEXTURECAPS_TRANSPARENCY |
		NPDISP_D3DPTEXTURECAPS_CUBEMAP | NPDISP_D3DPTEXTURECAPS_MIPMAP |
		NPDISP_D3DPTEXTURECAPS_NOPROJECTEDBUMPENV;
	global.hwCaps.dpcTriCaps.dwTextureFilterCaps = NPDISP_D3DPTFILTERCAPS_NEAREST |
		NPDISP_D3DPTFILTERCAPS_LINEAR | NPDISP_D3DPTFILTERCAPS_MIPNEAREST |
		NPDISP_D3DPTFILTERCAPS_MIPLINEAR | NPDISP_D3DPTFILTERCAPS_LINEARMIPLINEAR |
		NPDISP_D3DPTFILTERCAPS_MINFPOINT | NPDISP_D3DPTFILTERCAPS_MINFLINEAR |
		NPDISP_D3DPTFILTERCAPS_MIPFPOINT | NPDISP_D3DPTFILTERCAPS_MIPFLINEAR |
		NPDISP_D3DPTFILTERCAPS_MAGFPOINT | NPDISP_D3DPTFILTERCAPS_MAGFLINEAR;
	global.hwCaps.dpcTriCaps.dwTextureBlendCaps = NPDISP_D3DPTBLENDCAPS_MODULATE;
	global.hwCaps.dpcTriCaps.dwTextureAddressCaps = NPDISP_D3DPTADDRESSCAPS_WRAP |
		NPDISP_D3DPTADDRESSCAPS_MIRROR | NPDISP_D3DPTADDRESSCAPS_CLAMP |
		NPDISP_D3DPTADDRESSCAPS_INDEPENDENTUV;
	global.hwCaps.dwDeviceRenderBitDepth = NPDISP_DDBD_16 | NPDISP_DDBD_24 | NPDISP_DDBD_32;
	global.hwCaps.dwDeviceZBufferBitDepth = NPDISP_DDBD_16;
	global.hwCaps.dwMaxVertexCount = 63488UL;

	formats[0].dwSize = sizeof(formats[0]);
	formats[0].dwFlags = NPDISP_DDSD_CAPS | NPDISP_DDSD_PIXELFORMAT;
	formats[0].ddpfPixelFormat.dwSize = sizeof(formats[0].ddpfPixelFormat);
	formats[0].ddpfPixelFormat.dwFlags = NPDISP_DDPF_RGB;
	formats[0].ddpfPixelFormat.dwRGBBitCount = 16;
	formats[0].ddpfPixelFormat.dwRBitMask = 0x0000f800UL;
	formats[0].ddpfPixelFormat.dwGBitMask = 0x000007e0UL;
	formats[0].ddpfPixelFormat.dwBBitMask = 0x0000001fUL;
	formats[0].ddsCaps.dwCaps = NPDISP_DDSCAPS_TEXTURE;

	formats[1].dwSize = sizeof(formats[1]);
	formats[1].dwFlags = NPDISP_DDSD_CAPS | NPDISP_DDSD_PIXELFORMAT;
	formats[1].ddpfPixelFormat.dwSize = sizeof(formats[1].ddpfPixelFormat);
	formats[1].ddpfPixelFormat.dwFlags = NPDISP_DDPF_RGB | NPDISP_DDPF_ALPHAPIXELS;
	formats[1].ddpfPixelFormat.dwRGBBitCount = 16;
	formats[1].ddpfPixelFormat.dwRBitMask = 0x00007c00UL;
	formats[1].ddpfPixelFormat.dwGBitMask = 0x000003e0UL;
	formats[1].ddpfPixelFormat.dwBBitMask = 0x0000001fUL;
	formats[1].ddpfPixelFormat.dwRGBAlphaBitMask = 0x00008000UL;
	formats[1].ddsCaps.dwCaps = NPDISP_DDSCAPS_TEXTURE;

	formats[2].dwSize = sizeof(formats[2]);
	formats[2].dwFlags = NPDISP_DDSD_CAPS | NPDISP_DDSD_PIXELFORMAT;
	formats[2].ddpfPixelFormat.dwSize = sizeof(formats[2].ddpfPixelFormat);
	formats[2].ddpfPixelFormat.dwFlags = NPDISP_DDPF_RGB | NPDISP_DDPF_ALPHAPIXELS;
	formats[2].ddpfPixelFormat.dwRGBBitCount = 16;
	formats[2].ddpfPixelFormat.dwRBitMask = 0x00000f00UL;
	formats[2].ddpfPixelFormat.dwGBitMask = 0x000000f0UL;
	formats[2].ddpfPixelFormat.dwBBitMask = 0x0000000fUL;
	formats[2].ddpfPixelFormat.dwRGBAlphaBitMask = 0x0000f000UL;
	formats[2].ddsCaps.dwCaps = NPDISP_DDSCAPS_TEXTURE;

	formats[3].dwSize = sizeof(formats[3]);
	formats[3].dwFlags = NPDISP_DDSD_CAPS | NPDISP_DDSD_PIXELFORMAT;
	formats[3].ddpfPixelFormat.dwSize = sizeof(formats[3].ddpfPixelFormat);
	formats[3].ddpfPixelFormat.dwFlags = NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE;
	formats[3].ddpfPixelFormat.dwRGBBitCount = 16;
	formats[3].ddpfPixelFormat.dwRBitMask = 0x0000001fUL;
	formats[3].ddpfPixelFormat.dwGBitMask = 0x000003e0UL;
	formats[3].ddpfPixelFormat.dwBBitMask = 0x0000fc00UL;
	formats[3].ddsCaps.dwCaps = NPDISP_DDSCAPS_TEXTURE;

	formats[4].dwSize = sizeof(formats[4]);
	formats[4].dwFlags = NPDISP_DDSD_CAPS | NPDISP_DDSD_PIXELFORMAT;
	formats[4].ddpfPixelFormat.dwSize = sizeof(formats[4].ddpfPixelFormat);
	formats[4].ddpfPixelFormat.dwFlags = NPDISP_DDPF_RGB | NPDISP_DDPF_ALPHAPIXELS;
	formats[4].ddpfPixelFormat.dwRGBBitCount = 32;
	formats[4].ddpfPixelFormat.dwRBitMask = 0x00ff0000UL;
	formats[4].ddpfPixelFormat.dwGBitMask = 0x0000ff00UL;
	formats[4].ddpfPixelFormat.dwBBitMask = 0x000000ffUL;
	formats[4].ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000UL;
	formats[4].ddsCaps.dwCaps = NPDISP_DDSCAPS_TEXTURE;

	formats[5] = formats[0];
	formats[5].ddpfPixelFormat.dwFlags = NPDISP_DDPF_BUMPDUDV;
	formats[5].ddpfPixelFormat.dwRGBBitCount = 16;
	formats[5].ddpfPixelFormat.dwRBitMask = 0x000000ffUL;
	formats[5].ddpfPixelFormat.dwGBitMask = 0x0000ff00UL;
	formats[5].ddpfPixelFormat.dwBBitMask = 0x00000000UL;
	formats[5].ddpfPixelFormat.dwRGBAlphaBitMask = 0x00000000UL;

	global.dwNumTextureFormats = NPDISP_D3D_TEXTURE_FORMAT_COUNT;
	global.lpTextureFormats = formatAddr;

	callbacks.dwSize = sizeof(callbacks);
	callbacks.ContextCreate = thunks.ContextCreate;
	callbacks.ContextDestroy = thunks.ContextDestroy;
	callbacks.ContextDestroyAll = thunks.ContextDestroyAll;
	callbacks.SceneCapture = thunks.SceneCapture;
	callbacks.RenderState = thunks.RenderState;
	callbacks.RenderPrimitive = thunks.RenderPrimitive;
	callbacks.TextureCreate = thunks.TextureCreate;
	callbacks.TextureDestroy = thunks.TextureDestroy;
	callbacks.TextureSwap = thunks.TextureSwap;
	callbacks.TextureGetSurf = thunks.TextureGetSurf;
	callbacks.GetState = thunks.GetState;

	if (!callbacks.ContextCreate || !callbacks.ContextDestroy || !callbacks.ContextDestroyAll ||
		!callbacks.RenderState || !callbacks.RenderPrimitive || !callbacks.GetState ||
		!thunks.SetRenderTarget || !thunks.Clear || !thunks.DrawPrimitives ||
		!thunks.ValidateTextureStageState || !thunks.DrawPrimitives2 ||
		!thunks.CreateSurfaceEx || !thunks.GetDriverState || !thunks.DestroyDDLocal) return false;

	return npdisp_d3d_write(&global, globalAddr, sizeof(global)) &&
		npdisp_d3d_write(formats, formatAddr, sizeof(formats)) &&
		npdisp_d3d_write(&callbacks, callbacksAddr, sizeof(callbacks));
}

static UINT32 npdisp_d3d_getDriverState(UINT32 lpDataAddr)
{
	NPDISP_DD_GETDRIVERSTATEDATA32 data = { 0 };
	UINT8 zeroState[44] = { 0 };
	UINT32 expectedSize = 0;

	if (!lpDataAddr) return NPDISP_DDHAL_DRIVER_HANDLED;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (data.dwFlags == NPDISP_D3DDEVINFOID_TEXTUREMANAGER || data.dwFlags == NPDISP_D3DDEVINFOID_D3DTEXTUREMANAGER) expectedSize = 44;
	else if (data.dwFlags == NPDISP_D3DDEVINFOID_TEXTURING) expectedSize = 40;

	if (!expectedSize || !data.lpdwStates || data.dwLength < expectedSize ||
		!npdisp_d3d_write(zeroState, data.lpdwStates, expectedSize)) {
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
	}
	else {
		data.ddRVal = 0;
	}
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

UINT32 npdisp_d3d_getDriverInfo(UINT32 lpDataAddr)
{
	static const NPDISP_GUID32 callbacksGuid = { 0x7bf06990UL, 0x8794, 0x11d0, { 0x91, 0x39, 0x08, 0x00, 0x36, 0xd2, 0xef, 0x02 } };
	static const NPDISP_GUID32 callbacks2Guid = { 0x0ba584e1UL, 0x70b6, 0x11d0, { 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a } };
	static const NPDISP_GUID32 callbacks3Guid = { 0xddf41230UL, 0xec0a, 0x11d0, { 0xa9, 0xb6, 0x00, 0xaa, 0x00, 0xc0, 0x99, 0x3e } };
	static const NPDISP_GUID32 parseUnknownCommandGuid = { 0x2e04ffa0UL, 0x98e4, 0x11d1, { 0x8c, 0xe1, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 } };
	static const NPDISP_GUID32 extendedCapsGuid = { 0x7de41f80UL, 0x9d93, 0x11d0, { 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29 } };
	static const NPDISP_GUID32 misc2Guid = { 0x406b2f00UL, 0x3e5a, 0x11d1, { 0xb6, 0x40, 0x00, 0xaa, 0x00, 0xa1, 0xf9, 0x6a } };
	static const NPDISP_GUID32 zPixelFormatsGuid = { 0x93869880UL, 0x36cf, 0x11d1, { 0x9b, 0x1b, 0x00, 0xaa, 0x00, 0xbb, 0xb8, 0xae } };
	static const NPDISP_GUID32 moreSurfaceCapsGuid = { 0x3b8a0466UL, 0xf269, 0x11d1, { 0x88, 0x0b, 0x00, 0xc0, 0x4f, 0xd9, 0x30, 0xc5 } };
	NPDISP_DD_GETDRIVERINFODATA32 data = { 0 };
	NPDISP_D3D_THUNK_TABLE32 thunks = { 0 };
	NPDISP_D3DHAL_CALLBACKS32 callbacks = { 0 };
	NPDISP_D3DHAL_CALLBACKS2_32 callbacks2 = { 0 };
	NPDISP_D3DHAL_CALLBACKS3_32 callbacks3 = { 0 };
	NPDISP_D3DHAL_D3DEXTENDEDCAPS32 extendedCaps = { 0 };
	NPDISP_DDZPIXELFORMATS32 zFormats = { 0 };
	NPDISP_DD_MORESURFACECAPS32 moreCaps = { 0 };
	NPDISP_DD_MISCELLANEOUS2CALLBACKS32 misc2 = { 0 };
	const void* source = NULL;
	UINT32 sourceSize = 0;
	UINT32 copySize;

	if (!lpDataAddr) return NPDISP_DDHAL_DRIVER_HANDLED;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.dwActualSize = 0;
	TRACEOUTD3D(("NPDISP11 D3D_GETINFO_QUERY guid=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x expected=%u data=%08x",
		data.guidInfo.Data1, data.guidInfo.Data2, data.guidInfo.Data3, data.guidInfo.Data4[0], data.guidInfo.Data4[1],
		data.guidInfo.Data4[2], data.guidInfo.Data4[3], data.guidInfo.Data4[4], data.guidInfo.Data4[5],
		data.guidInfo.Data4[6], data.guidInfo.Data4[7], data.dwExpectedSize, data.lpvData));

	if (npdisp_d3d_guidEqual(&data.guidInfo, &parseUnknownCommandGuid)) {
		data.ddRVal = 0;
		TRACEOUTD3D(("NPDISP11 D3D_GETINFO_PARSEUNKNOWN ok"));
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	if (npdisp.acceleration < NPDISP_ACCEL_DIRECT3D ||
		!npdisp_d3d_readThunkTable(npdisp.mm_ddBridgeInfoAddr, &thunks) || !data.lpvData) {
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	if (npdisp_d3d_guidEqual(&data.guidInfo, &callbacksGuid)) {
		callbacks.dwSize = sizeof(callbacks);
		callbacks.ContextCreate = thunks.ContextCreate;
		callbacks.ContextDestroy = thunks.ContextDestroy;
		callbacks.ContextDestroyAll = thunks.ContextDestroyAll;
		callbacks.SceneCapture = thunks.SceneCapture;
		callbacks.RenderState = thunks.RenderState;
		callbacks.RenderPrimitive = thunks.RenderPrimitive;
		callbacks.TextureCreate = thunks.TextureCreate;
		callbacks.TextureDestroy = thunks.TextureDestroy;
		callbacks.TextureSwap = thunks.TextureSwap;
		callbacks.TextureGetSurf = thunks.TextureGetSurf;
		callbacks.GetState = thunks.GetState;
		source = &callbacks;
		sourceSize = sizeof(callbacks);
	}
	else if (npdisp_d3d_guidEqual(&data.guidInfo, &callbacks2Guid)) {
		callbacks2.dwSize = sizeof(callbacks2);
		callbacks2.dwFlags = NPDISP_D3DHAL2_CB32_SETRENDERTARGET | NPDISP_D3DHAL2_CB32_CLEAR |
			NPDISP_D3DHAL2_CB32_DRAWONEPRIMITIVE | NPDISP_D3DHAL2_CB32_DRAWONEINDEXEDPRIMITIVE |
			NPDISP_D3DHAL2_CB32_DRAWPRIMITIVES;
		callbacks2.SetRenderTarget = thunks.SetRenderTarget;
		callbacks2.Clear = thunks.Clear;
		callbacks2.DrawOnePrimitive = thunks.DrawOnePrimitive;
		callbacks2.DrawOneIndexedPrimitive = thunks.DrawOneIndexedPrimitive;
		callbacks2.DrawPrimitives = thunks.DrawPrimitives;
		source = &callbacks2;
		sourceSize = sizeof(callbacks2);
	}
	else if (npdisp_d3d_guidEqual(&data.guidInfo, &callbacks3Guid)) {
		callbacks3.dwSize = sizeof(callbacks3);
		callbacks3.dwFlags = NPDISP_D3DHAL3_CB32_CLEAR2 | NPDISP_D3DHAL3_CB32_VALIDATETEXTURESTAGESTATE | NPDISP_D3DHAL3_CB32_DRAWPRIMITIVES2;
		callbacks3.Clear2 = thunks.Clear2;
		callbacks3.ValidateTextureStageState = thunks.ValidateTextureStageState;
		callbacks3.DrawPrimitives2 = thunks.DrawPrimitives2;
		source = &callbacks3;
		sourceSize = sizeof(callbacks3);
	}
	else if (npdisp_d3d_guidEqual(&data.guidInfo, &extendedCapsGuid)) {
		extendedCaps.dwSize = sizeof(extendedCaps);
		extendedCaps.dwMinTextureWidth = 1;
		extendedCaps.dwMaxTextureWidth = 1024;
		extendedCaps.dwMinTextureHeight = 1;
		extendedCaps.dwMaxTextureHeight = 1024;
		extendedCaps.dwMaxTextureRepeat = 1024;
		extendedCaps.dwMaxTextureAspectRatio = 1024;
		extendedCaps.dwMaxAnisotropy = 1;
		extendedCaps.dwStencilCaps = NPDISP_D3DSTENCILCAPS_KEEP | NPDISP_D3DSTENCILCAPS_ZERO |
			NPDISP_D3DSTENCILCAPS_REPLACE | NPDISP_D3DSTENCILCAPS_INCRSAT |
			NPDISP_D3DSTENCILCAPS_DECRSAT | NPDISP_D3DSTENCILCAPS_INVERT |
			NPDISP_D3DSTENCILCAPS_INCR | NPDISP_D3DSTENCILCAPS_DECR;
		extendedCaps.dwFVFCaps = NPDISP_D3D_TEXTURE_COORD_SETS;
		extendedCaps.dwTextureOpCaps = NPDISP_D3DTEXOPCAPS_DISABLE | NPDISP_D3DTEXOPCAPS_SELECTARG1 |
			NPDISP_D3DTEXOPCAPS_SELECTARG2 | NPDISP_D3DTEXOPCAPS_MODULATE |
			NPDISP_D3DTEXOPCAPS_ADD | NPDISP_D3DTEXOPCAPS_BLENDTEXTUREALPHA |
			NPDISP_D3DTEXOPCAPS_BUMPENVMAP | NPDISP_D3DTEXOPCAPS_BUMPENVMAPLUMINANCE;
		extendedCaps.wMaxTextureBlendStages = 3;
		extendedCaps.wMaxSimultaneousTextures = 3;
		extendedCaps.wMaxUserClipPlanes = NPDISP_D3D_MAX_USER_CLIP_PLANES;
		if (npdisp_d3d_tnlEnabled) {
			extendedCaps.dwMaxActiveLights = NPDISP_D3D_MAX_ACTIVE_LIGHTS;
			extendedCaps.wMaxVertexBlendMatrices = 4;
			extendedCaps.dwVertexProcessingCaps = NPDISP_D3DVTXPCAPS_TEXGEN | NPDISP_D3DVTXPCAPS_DIRECTIONALLIGHTS |
				NPDISP_D3DVTXPCAPS_POSITIONALLIGHTS | NPDISP_D3DVTXPCAPS_MATERIALSOURCE7 |
				NPDISP_D3DVTXPCAPS_LOCALVIEWER;
		}
		source = &extendedCaps;
		sourceSize = sizeof(extendedCaps);
	}
	else if (npdisp_d3d_guidEqual(&data.guidInfo, &zPixelFormatsGuid)) {
		zFormats.dwCount = 2;
		zFormats.formats[0].dwSize = sizeof(zFormats.formats[0]);
		zFormats.formats[0].dwFlags = 0x00000400UL;
		zFormats.formats[0].dwZBufferBitDepth = 16;
		zFormats.formats[0].dwZBitMask = 0x0000ffffUL;
		zFormats.formats[1].dwSize = sizeof(zFormats.formats[1]);
		zFormats.formats[1].dwFlags = 0x00004400UL;
		zFormats.formats[1].dwZBufferBitDepth = 16;
		zFormats.formats[1].dwStencilBitDepth = 4;
		zFormats.formats[1].dwZBitMask = 0x00000fffUL;
		zFormats.formats[1].dwStencilBitMask = 0x0000f000UL;
		source = &zFormats;
		sourceSize = sizeof(zFormats);
		TRACEOUTD3D(("NPDISP11 D3D_GETINFO_ZFORMATS expected=%u count=%u bytes=%u pf0=%u/%08x/%u pf1=%u/%08x/%u/%u",
			data.dwExpectedSize, zFormats.dwCount, sourceSize,
			zFormats.formats[0].dwSize, zFormats.formats[0].dwFlags, zFormats.formats[0].dwZBufferBitDepth,
			zFormats.formats[1].dwSize, zFormats.formats[1].dwFlags, zFormats.formats[1].dwZBufferBitDepth, zFormats.formats[1].dwStencilBitDepth));
	}
	else if (npdisp_d3d_guidEqual(&data.guidInfo, &moreSurfaceCapsGuid)) {
		moreCaps.dwSize = sizeof(moreCaps);
		moreCaps.ddsCapsMore.dwCaps2 = NPDISP_DDSCAPS2_CUBEMAP | NPDISP_DDSCAPS2_CUBEMAP_ALLFACES;
		source = &moreCaps;
		sourceSize = sizeof(moreCaps);
	}
	else if (npdisp_d3d_guidEqual(&data.guidInfo, &misc2Guid)) {
		misc2.dwSize = sizeof(misc2);
		misc2.dwFlags = NPDISP_DDHAL_MISC2CB32_CREATESURFACEEX |
			NPDISP_DDHAL_MISC2CB32_GETDRIVERSTATE | NPDISP_DDHAL_MISC2CB32_DESTROYDDLOCAL;
		misc2.CreateSurfaceEx = thunks.CreateSurfaceEx;
		misc2.GetDriverState = thunks.GetDriverState;
		misc2.DestroyDDLocal = thunks.DestroyDDLocal;
		source = &misc2;
		sourceSize = sizeof(misc2);
	}
	else {
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		TRACEOUT11(("NPDISP11 D3D_GETINFO_UNKNOWN guid=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x expected=%u",
			data.guidInfo.Data1, data.guidInfo.Data2, data.guidInfo.Data3, data.guidInfo.Data4[0], data.guidInfo.Data4[1],
			data.guidInfo.Data4[2], data.guidInfo.Data4[3], data.guidInfo.Data4[4], data.guidInfo.Data4[5],
			data.guidInfo.Data4[6], data.guidInfo.Data4[7], data.dwExpectedSize));
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	copySize = data.dwExpectedSize < sourceSize ? data.dwExpectedSize : sourceSize;
	if (!copySize || !npdisp_d3d_write((void*)source, data.lpvData, copySize)) {
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
	}
	else {
		data.dwActualSize = sourceSize;
		data.ddRVal = 0;
	}
	TRACEOUTD3D(("NPDISP11 D3D_GETINFO_RESULT expected=%u source=%u copy=%u actual=%u rval=%08x",
		data.dwExpectedSize, sourceSize, copySize, data.dwActualSize, data.ddRVal));
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_asyncCommandBytes(const NPDISP_D3D_ASYNC_COMMAND* command)
{
	UINT64 bytes = sizeof(*command);
	if (!command) return 0;
	bytes += (UINT64)command->vertices.size() * sizeof(command->vertices[0]);
	bytes += (UINT64)command->indices.size() * sizeof(command->indices[0]);
	bytes += (UINT64)command->rects.size() * sizeof(command->rects[0]);
	for (UINT32 i = 0; i < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++i) bytes += command->textureStorage[i].size();
	return bytes > 0xffffffffULL ? 0xffffffffUL : (UINT32)bytes;
}

static bool npdisp_d3d_asyncStart(void)
{
	unsigned int threadId = 0;
	if (npdisp_d3d_asyncInitialized) return npdisp_d3d_asyncThread != NULL;
	InitializeCriticalSection(&npdisp_d3d_asyncLock);
	npdisp_d3d_asyncWake = CreateEvent(NULL, TRUE, FALSE, NULL);
	npdisp_d3d_asyncIdle = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (!npdisp_d3d_asyncWake || !npdisp_d3d_asyncIdle) {
		if (npdisp_d3d_asyncWake) CloseHandle(npdisp_d3d_asyncWake);
		if (npdisp_d3d_asyncIdle) CloseHandle(npdisp_d3d_asyncIdle);
		npdisp_d3d_asyncWake = NULL;
		npdisp_d3d_asyncIdle = NULL;
		DeleteCriticalSection(&npdisp_d3d_asyncLock);
		return false;
	}
	npdisp_d3d_asyncStop = false;
	npdisp_d3d_asyncBytes = 0;
	npdisp_d3d_asyncActiveTarget = 0;
	memset(npdisp_d3d_asyncActiveTextures, 0, sizeof(npdisp_d3d_asyncActiveTextures));
	InterlockedExchange(&npdisp_d3d_asyncVisibleDirty, 0);
	InterlockedExchange(&npdisp_d3d_asyncError, 0);
	npdisp_d3d_asyncThread = (HANDLE)_beginthreadex(NULL, 0, npdisp_d3d_asyncThreadProc, NULL, 0, &threadId);
	if (!npdisp_d3d_asyncThread) {
		CloseHandle(npdisp_d3d_asyncWake);
		CloseHandle(npdisp_d3d_asyncIdle);
		npdisp_d3d_asyncWake = NULL;
		npdisp_d3d_asyncIdle = NULL;
		DeleteCriticalSection(&npdisp_d3d_asyncLock);
		return false;
	}
	npdisp_d3d_asyncInitialized = true;
	TRACEOUTD3D(("NPDISP11 D3D_ASYNC_START thread=%u", threadId));
	return true;
}

void npdisp_d3d_flush(void)
{
	LARGE_INTEGER qpcFrequency = { 0 };
	LARGE_INTEGER qpcStart = { 0 };
	LARGE_INTEGER qpcEnd = { 0 };
	DWORD tickStart;
	UINT32 queued = 0;
	UINT32 bytes = 0;
	UINT32 active = 0;
	bool useQpc;
	if (!npdisp_d3d_asyncInitialized || !npdisp_d3d_asyncIdle) return;
	tickStart = GetTickCount();
	useQpc = QueryPerformanceFrequency(&qpcFrequency) && QueryPerformanceCounter(&qpcStart);
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	queued = (UINT32)npdisp_d3d_asyncQueue.size();
	bytes = npdisp_d3d_asyncBytes;
	active = npdisp_d3d_asyncActiveTarget;
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	WaitForSingleObject(npdisp_d3d_asyncIdle, INFINITE);
	{
		UINT64 elapsedUs;
		UINT32 workClock = 0;
		if (useQpc && QueryPerformanceCounter(&qpcEnd) && qpcEnd.QuadPart >= qpcStart.QuadPart && qpcFrequency.QuadPart > 0) {
			UINT64 ticks = (UINT64)(qpcEnd.QuadPart - qpcStart.QuadPart);
			elapsedUs = (ticks * 1000000ULL + (UINT64)qpcFrequency.QuadPart - 1ULL) / (UINT64)qpcFrequency.QuadPart;
		}
		else {
			elapsedUs = (UINT64)(GetTickCount() - tickStart) * 1000ULL;
		}
		if (elapsedUs && (queued || active) && pccore.realclock) {
			UINT64 workUs = elapsedUs > (UINT64)NPDISP_D3D_ASYNC_WORKCLOCK_MAX_MS * 1000ULL ? (UINT64)NPDISP_D3D_ASYNC_WORKCLOCK_MAX_MS * 1000ULL : elapsedUs;
			UINT64 work64 = ((UINT64)pccore.realclock * workUs + 999999ULL) / 1000000ULL;
			if (work64 > 0x3fffffffULL) work64 = 0x3fffffffULL;
			workClock = (UINT32)work64;
			CPU_REMCLOCK -= (SINT32)workClock;
		}
		++npdisp_d3d_profileFlushCalls;
		if (queued || active) ++npdisp_d3d_profileBusyFlushCalls;
		npdisp_d3d_profileWaitUs += elapsedUs;
		if (elapsedUs > npdisp_d3d_profileWaitMaxUs) npdisp_d3d_profileWaitMaxUs = elapsedUs > 0xffffffffULL ? 0xffffffffUL : (UINT32)elapsedUs;
		npdisp_d3d_profileWorkClock += workClock;
		if (elapsedUs) TRACEOUTD3D(("NPDISP11 D3D_ASYNC_WAIT us=%llu work=%u queued=%u bytes=%u active=%08x", (unsigned long long)elapsedUs, workClock, queued, bytes, active));
	}
	npdisp_d3d_profileReport(GetTickCount());
	npdisp_d3d_poll();
	if (InterlockedExchange(&npdisp_d3d_asyncError, 0)) TRACEOUTD3D(("NPDISP11 D3D_ASYNC_ERROR worker command failed"));
}

void npdisp_d3d_poll(void)
{
	if (InterlockedExchange(&npdisp_d3d_asyncVisibleDirty, 0)) {
		npdisp_setDirtyAll();
		npdisp.updated = 1;
	}
}

static bool npdisp_d3d_asyncBusy(void)
{
	bool busy;
	if (!npdisp_d3d_asyncInitialized) return false;
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	busy = npdisp_d3d_asyncActiveTarget != 0 || !npdisp_d3d_asyncQueue.empty();
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	return busy;
}

bool npdisp_d3d_busy(void)
{
	return npdisp_d3d_asyncBusy();
}

static bool npdisp_d3d_asyncPendingTarget(UINT32 lpSurface)
{
	bool pending = false;
	if (!lpSurface || !npdisp_d3d_asyncInitialized) return false;
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	if (npdisp_d3d_asyncActiveTarget == lpSurface) pending = true;
	else {
		for (std::deque<NPDISP_D3D_ASYNC_COMMAND*>::const_iterator it = npdisp_d3d_asyncQueue.begin(); it != npdisp_d3d_asyncQueue.end(); ++it) {
			if ((*it)->targetSurface == lpSurface) { pending = true; break; }
		}
	}
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	return pending;
}

static bool npdisp_d3d_asyncPendingSurface(UINT32 lpSurface)
{
	bool pending = false;
	if (!lpSurface || !npdisp_d3d_asyncInitialized) return false;
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	if (npdisp_d3d_asyncActiveTarget == lpSurface) pending = true;
	for (UINT32 stage = 0; !pending && stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		if (npdisp_d3d_asyncActiveTextures[stage] == lpSurface) pending = true;
	}
	if (!pending) {
		for (std::deque<NPDISP_D3D_ASYNC_COMMAND*>::const_iterator it = npdisp_d3d_asyncQueue.begin(); it != npdisp_d3d_asyncQueue.end() && !pending; ++it) {
			if ((*it)->targetSurface == lpSurface) { pending = true; break; }
			for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
				if ((*it)->textureSurfaces[stage] == lpSurface) { pending = true; break; }
			}
		}
	}
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	return pending;
}

static bool npdisp_d3d_asyncEnqueue(NPDISP_D3D_ASYNC_COMMAND* command)
{
	UINT32 bytes;
	bool drain = false;
	if (!command || !npdisp_d3d_asyncStart()) return false;
	bytes = npdisp_d3d_asyncCommandBytes(command);
	command->bytes = bytes;
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	if (npdisp_d3d_asyncQueue.size() >= NPDISP_D3D_ASYNC_MAX_COMMANDS || npdisp_d3d_asyncBytes >= NPDISP_D3D_ASYNC_MAX_BYTES ||
		(npdisp_d3d_asyncBytes && bytes > NPDISP_D3D_ASYNC_MAX_BYTES - npdisp_d3d_asyncBytes)) drain = true;
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	if (drain) npdisp_d3d_flush();
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	npdisp_d3d_asyncQueue.push_back(command);
	npdisp_d3d_asyncBytes += bytes;
	InterlockedIncrement(&npdisp_d3d_profileEnqueue);
	InterlockedExchangeAdd(&npdisp_d3d_profileEnqueueBytes, (LONG)bytes);
	if (command->type == NPDISP_D3D_ASYNC_DRAW) InterlockedIncrement(&npdisp_d3d_profileDraw);
	else if (command->type == NPDISP_D3D_ASYNC_CLEAR) InterlockedIncrement(&npdisp_d3d_profileClear);
	ResetEvent(npdisp_d3d_asyncIdle);
	SetEvent(npdisp_d3d_asyncWake);
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	return true;
}

static void npdisp_d3d_asyncStopWorker(void)
{
	if (!npdisp_d3d_asyncInitialized) return;
	npdisp_d3d_flush();
	EnterCriticalSection(&npdisp_d3d_asyncLock);
	npdisp_d3d_asyncStop = true;
	SetEvent(npdisp_d3d_asyncWake);
	LeaveCriticalSection(&npdisp_d3d_asyncLock);
	WaitForSingleObject(npdisp_d3d_asyncThread, INFINITE);
	CloseHandle(npdisp_d3d_asyncThread);
	CloseHandle(npdisp_d3d_asyncWake);
	CloseHandle(npdisp_d3d_asyncIdle);
	npdisp_d3d_asyncThread = NULL;
	npdisp_d3d_asyncWake = NULL;
	npdisp_d3d_asyncIdle = NULL;
	npdisp_d3d_asyncInitialized = false;
	npdisp_d3d_asyncStop = false;
	npdisp_d3d_asyncBytes = 0;
	npdisp_d3d_asyncActiveTarget = 0;
	memset(npdisp_d3d_asyncActiveTextures, 0, sizeof(npdisp_d3d_asyncActiveTextures));
	DeleteCriticalSection(&npdisp_d3d_asyncLock);
	TRACEOUTD3D(("NPDISP11 D3D_ASYNC_STOP"));
}

static unsigned int __stdcall npdisp_d3d_asyncThreadProc(void*)
{
	for (;;) {
		DWORD batchStart;
		UINT32 batchCommands = 0;
		WaitForSingleObject(npdisp_d3d_asyncWake, INFINITE);
		batchStart = GetTickCount();
		for (;;) {
			NPDISP_D3D_ASYNC_COMMAND* command = NULL;
			bool stop = false;
			EnterCriticalSection(&npdisp_d3d_asyncLock);
			if (!npdisp_d3d_asyncQueue.empty()) {
				command = npdisp_d3d_asyncQueue.front();
				npdisp_d3d_asyncQueue.pop_front();
				npdisp_d3d_asyncBytes -= command->bytes;
				npdisp_d3d_asyncActiveTarget = command->targetSurface;
				memcpy(npdisp_d3d_asyncActiveTextures, command->textureSurfaces, sizeof(npdisp_d3d_asyncActiveTextures));
			}
			else {
				ResetEvent(npdisp_d3d_asyncWake);
				npdisp_d3d_asyncActiveTarget = 0;
				memset(npdisp_d3d_asyncActiveTextures, 0, sizeof(npdisp_d3d_asyncActiveTextures));
				SetEvent(npdisp_d3d_asyncIdle);
				stop = npdisp_d3d_asyncStop;
			}
			LeaveCriticalSection(&npdisp_d3d_asyncLock);
			if (!command) {
				if (batchCommands) InterlockedExchangeAdd(&npdisp_d3d_profileWorkerMs, (LONG)(GetTickCount() - batchStart));
				if (stop) { _endthreadex(0); return 0; }
				break;
			}
			if (!npdisp_d3d_asyncExecute(command)) InterlockedExchange(&npdisp_d3d_asyncError, 1);
			++batchCommands;
			InterlockedIncrement(&npdisp_d3d_profileWorkerCommands);
			if (command->target.visible) InterlockedExchange(&npdisp_d3d_asyncVisibleDirty, 1);
			delete command;
			EnterCriticalSection(&npdisp_d3d_asyncLock);
			npdisp_d3d_asyncActiveTarget = 0;
			memset(npdisp_d3d_asyncActiveTextures, 0, sizeof(npdisp_d3d_asyncActiveTextures));
			if (npdisp_d3d_asyncQueue.empty()) SetEvent(npdisp_d3d_asyncIdle);
			LeaveCriticalSection(&npdisp_d3d_asyncLock);
		}
	}
}

static bool npdisp_d3d_surfaceHandleFromLocal(UINT32 lpSurfaceLocal, UINT32* handle)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_MORE_HEAD32 more = { 0 };
	UINT32 handleAddr;
	if (!handle) return false;
	*handle = 0;
	if (!lpSurfaceLocal || !npdisp_d3d_read(&lcl, lpSurfaceLocal, sizeof(lcl)) || !lcl.lpSurfMore ||
		!npdisp_d3d_read(&more, lcl.lpSurfMore, sizeof(more)) || more.dwSize < NPDISP_DDRAWSURFACE_MORE_SIZE_DX7 ||
		!npdisp_d3d_add(lcl.lpSurfMore, NPDISP_DDRAWSURFACE_HANDLE_OFS_DX7, &handleAddr) ||
		!npdisp_d3d_read(handle, handleAddr, sizeof(*handle)) || !*handle) return false;
	return true;
}

bool npdisp_d3d_validateHalPointers(UINT32 globalAddr, UINT32 callbacksAddr)
{
	UINT32 value;
	UINT32 capsAddr;
	static const UINT32 callbackOffsets[] = { 4, 8, 12, 28, 32, 96 };
	if (!npdisp_d3d_read(&value, globalAddr, sizeof(value)) || value != 192U ||
		!npdisp_d3d_add(globalAddr, 4U, &capsAddr) || !npdisp_d3d_read(&value, capsAddr, sizeof(value)) || value != 172U ||
		!npdisp_d3d_read(&value, callbacksAddr, sizeof(value)) || value != 140U) return false;
	for (UINT32 i = 0; i < sizeof(callbackOffsets) / sizeof(callbackOffsets[0]); ++i) {
		UINT32 addr;
		if (!npdisp_d3d_add(callbacksAddr, callbackOffsets[i], &addr) ||
			!npdisp_d3d_read(&value, addr, sizeof(value)) || !value) return false;
	}
	return true;
}

static NPDISP_D3D_CONTEXT* npdisp_d3d_getContext(UINT32 handle)
{
	std::map<UINT32, NPDISP_D3D_CONTEXT>::iterator it;
	if (!handle) return NULL;
	it = npdisp_d3d_contexts.find(handle);
	return (it != npdisp_d3d_contexts.end()) ? &it->second : NULL;
}

static NPDISP_D3D_TNL_CONTEXT* npdisp_d3d_getTnlContext(UINT32 handle)
{
	std::map<UINT32, NPDISP_D3D_TNL_CONTEXT>::iterator it;
	if (!handle) return NULL;
	it = npdisp_d3d_tnlContexts.find(handle);
	return (it != npdisp_d3d_tnlContexts.end()) ? &it->second : NULL;
}


static UINT32 npdisp_d3d_getDDLocalNamespace(UINT32 lpDDLcl, bool create)
{
	std::map<UINT32, UINT32>::iterator it;
	UINT32 start;
	if (!lpDDLcl) return 0;
	it = npdisp_d3d_ddLocalNamespaces.find(lpDDLcl);
	if (it != npdisp_d3d_ddLocalNamespaces.end()) return it->second;
	if (!create) return 0;
	start = npdisp_d3d_nextDDLocalNamespace;
	do {
		UINT32 id = npdisp_d3d_nextDDLocalNamespace++;
		bool used = false;
		if (!npdisp_d3d_nextDDLocalNamespace) npdisp_d3d_nextDDLocalNamespace = 1;
		if (id) {
			for (it = npdisp_d3d_ddLocalNamespaces.begin(); it != npdisp_d3d_ddLocalNamespaces.end(); ++it) {
				if (it->second == id) {
					used = true;
					break;
				}
			}
			if (!used) {
				npdisp_d3d_ddLocalNamespaces[lpDDLcl] = id;
				return id;
			}
		}
	} while (npdisp_d3d_nextDDLocalNamespace != start);
	return 0;
}

static bool npdisp_d3d_bindDDLocalNamespaceByHandle(NPDISP_D3D_CONTEXT* context, UINT32 handle)
{
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it;
	UINT32 ddLocalNamespace = 0;
	if (!context || !handle) return false;
	if (context->ddLocalNamespace) return true;
	for (it = npdisp_d3d_surfaces.begin(); it != npdisp_d3d_surfaces.end(); ++it) {
		if (it->second.handle != handle) continue;
		if (ddLocalNamespace && ddLocalNamespace != it->second.ddLocalNamespace) return false;
		ddLocalNamespace = it->second.ddLocalNamespace;
	}
	if (!ddLocalNamespace) return false;
	context->ddLocalNamespace = ddLocalNamespace;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_NAMESPACE_BIND ns=%08x handle=%08x", ddLocalNamespace, handle));
	return true;
}

static UINT32 npdisp_d3d_allocContext(void)
{
	UINT32 start = npdisp_d3d_nextContext;
	do {
		UINT32 handle = npdisp_d3d_nextContext++;
		if (!npdisp_d3d_nextContext) npdisp_d3d_nextContext = 1;
		if (handle && npdisp_d3d_contexts.find(handle) == npdisp_d3d_contexts.end()) return handle;
	} while (npdisp_d3d_nextContext != start);
	return 0;
}

static UINT32 npdisp_d3d_allocLegacyTexture(void)
{
	UINT32 start = npdisp_d3d_nextLegacyTexture;
	do {
		UINT32 handle = npdisp_d3d_nextLegacyTexture++;
		if (!npdisp_d3d_nextLegacyTexture) npdisp_d3d_nextLegacyTexture = 1;
		if (handle && npdisp_d3d_legacyTextures.find(handle) == npdisp_d3d_legacyTextures.end()) return handle;
	} while (npdisp_d3d_nextLegacyTexture != start);
	return 0;
}

static void npdisp_d3d_destroyLegacyTextures(UINT32 dwhContext)
{
	std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator it = npdisp_d3d_legacyTextures.begin();
	while (it != npdisp_d3d_legacyTextures.end()) {
		if (it->second.dwhContext == dwhContext) npdisp_d3d_legacyTextures.erase(it++);
		else ++it;
	}
}

static UINT32 npdisp_d3d_indirectSurfaceLocal(UINT32 surface)
{
	NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
	if (!surface || !npdisp_d3d_read(&surfaceInt, surface, sizeof(surfaceInt))) return 0;
	return surfaceInt.lpLcl;
}

static bool npdisp_d3d_resolveApertureOffset(UINT32 offset, SINT32 pitch, UINT32 height, UINT8** hostBase)
{
	UINT64 bytes;
	if (!hostBase || pitch <= 0 || !height) return false;
	bytes = (UINT64)(UINT32)pitch * height;
	if (!offset) {
		if (!npdisp.mm_screenPtr || bytes > npdisp.mm_screenSize || bytes > NPDISP_DD_PRIMARY_REGION_SIZE) return false;
		*hostBase = npdisp.mm_screenPtr;
		return true;
	}
	if (offset < NPDISP_DD_OFFSCREEN_OFFSET || offset >= NPDISP_DD_APERTURE_SIZE ||
		!npdisp.mm_ddOffscreenPtr || npdisp.mm_ddOffscreenSize != NPDISP_DD_OFFSCREEN_SIZE) return false;
	if (bytes > NPDISP_DD_OFFSCREEN_SIZE ||
		(UINT64)(offset - NPDISP_DD_OFFSCREEN_OFFSET) + bytes > NPDISP_DD_OFFSCREEN_SIZE) return false;
	*hostBase = npdisp.mm_ddOffscreenPtr + (offset - NPDISP_DD_OFFSCREEN_OFFSET);
	return true;
}

static bool npdisp_d3d_resolveAperture(UINT32 fpVidMem, SINT32 pitch, UINT32 height, UINT8** hostBase, UINT32* apertureOffset)
{
	UINT32 offset;
	if (!hostBase || !apertureOffset || !npdisp.mm_vramLinearAddr) return false;
	if (fpVidMem == 0 || fpVidMem == npdisp.mm_vramLinearAddr) offset = 0;
	else {
		if (fpVidMem < npdisp.mm_vramLinearAddr) return false;
		offset = fpVidMem - npdisp.mm_vramLinearAddr;
	}
	if (!npdisp_d3d_resolveApertureOffset(offset, pitch, height, hostBase)) return false;
	*apertureOffset = offset;
	return true;
}

static bool npdisp_d3d_target(UINT32 lpSurface, NPDISP_D3D_TARGET* target)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	UINT32 bytesPerPixel;
	UINT32 width;
	UINT32 height;
	SINT32 pitch;
	if (!lpSurface || !target || (npdisp.bpp != 15 && npdisp.bpp != 16 && npdisp.bpp != 24 && npdisp.bpp != 32)) {
		TRACEOUTD3D(("NPDISP11 D3D_TARGET_REJECT stage=args surf=%08x bpp=%u", lpSurface, npdisp.bpp));
		return false;
	}
	if (!npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpGbl) {
		TRACEOUTD3D(("NPDISP11 D3D_TARGET_REJECT stage=lcl surf=%08x", lpSurface));
		return false;
	}
	if (!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl))) {
		TRACEOUTD3D(("NPDISP11 D3D_TARGET_REJECT stage=gbl surf=%08x gbl=%08x", lpSurface, lcl.lpGbl));
		return false;
	}
	if (lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		TRACEOUTD3D(("NPDISP11 D3D_TARGET_REJECT stage=sysmem surf=%08x caps=%08x fp=%08x", lpSurface, lcl.ddsCaps.dwCaps, gbl.fpVidMem));
		return false;
	}
	width = gbl.wWidth ? gbl.wWidth : npdisp.width;
	height = gbl.wHeight ? gbl.wHeight : npdisp.height;
	pitch = gbl.lPitch ? gbl.lPitch : (SINT32)npdispwin.stride;
	bytesPerPixel = (npdisp.bpp == 15 || npdisp.bpp == 16) ? 2U : (npdisp.bpp == 24 ? 3U : (npdisp.bpp == 32 ? 4U : 0U));
	if (!bytesPerPixel || !width || !height || pitch <= 0 || (UINT64)width * bytesPerPixel > (UINT32)pitch) {
		TRACEOUTD3D(("NPDISP11 D3D_TARGET_REJECT stage=geometry surf=%08x size=%ux%u pitch=%d fp=%08x", lpSurface, width, height, pitch, gbl.fpVidMem));
		return false;
	}
	if (!npdisp_d3d_resolveAperture(gbl.fpVidMem, pitch, height, &hostBase, &apertureOffset)) {
		TRACEOUTD3D(("NPDISP11 D3D_TARGET_REJECT stage=aperture surf=%08x size=%ux%u pitch=%d fp=%08x", lpSurface, width, height, pitch, gbl.fpVidMem));
		return false;
	}
	target->sw.pixels = hostBase;
	target->sw.width = width;
	target->sw.height = height;
	target->sw.pitch = pitch;
	target->sw.bpp = npdisp.bpp;
	target->apertureOffset = apertureOffset;
	target->visible = (apertureOffset == npdisp.mm_ddScanoutOffset);
	return true;
}

static bool npdisp_d3d_captureTargetSnapshot(UINT32 lpSurface, NPDISP_D3D_TARGET_SNAPSHOT* snapshot)
{
	NPDISP_D3D_TARGET target;
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	UINT32 bytesPerPixel;
	UINT64 bytes;
	if (!snapshot) return false;
	memset(snapshot, 0, sizeof(*snapshot));
	if (npdisp_d3d_target(lpSurface, &target)) {
		snapshot->linearBase = npdisp.mm_vramLinearAddr + target.apertureOffset;
		snapshot->apertureOffset = target.apertureOffset;
		snapshot->width = target.sw.width;
		snapshot->height = target.sw.height;
		snapshot->pitch = target.sw.pitch;
		snapshot->bpp = target.sw.bpp;
		snapshot->valid = true;
		return true;
	}
	if (!lpSurface || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpGbl ||
		(lcl.ddsCaps.dwCaps & (NPDISP_DDSCAPS_SYSTEMMEMORY | NPDISP_DDSCAPS_TEXTURE | NPDISP_DDSCAPS_3DDEVICE)) !=
		(NPDISP_DDSCAPS_SYSTEMMEMORY | NPDISP_DDSCAPS_TEXTURE | NPDISP_DDSCAPS_3DDEVICE) ||
		!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl)) || !gbl.fpVidMem || !gbl.wWidth || !gbl.wHeight || gbl.lPitch <= 0) return false;
	bytesPerPixel = (npdisp.bpp == 15 || npdisp.bpp == 16) ? 2U : (npdisp.bpp == 24 ? 3U : (npdisp.bpp == 32 ? 4U : 0U));
	if (!bytesPerPixel || (UINT64)gbl.wWidth * bytesPerPixel > (UINT32)gbl.lPitch) return false;
	if (lcl.dwFlags & NPDISP_DDRAWISURF_HASPIXELFORMAT) {
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT fullGbl = { 0 };
		const NPDISP_DDPIXELFORMAT* pf;
		if (!npdisp_d3d_read(&fullGbl, lcl.lpGbl, sizeof(fullGbl))) return false;
		pf = &fullGbl.ddpfSurface;
		if (pf->dwSize < sizeof(*pf) || !(pf->dwFlags & NPDISP_DDPF_RGB) || (pf->dwFlags & NPDISP_DDPF_FOURCC)) return false;
		if ((npdisp.bpp == 15 && (pf->dwRGBBitCount != 16U || pf->dwRBitMask != 0x00007c00UL || pf->dwGBitMask != 0x000003e0UL || pf->dwBBitMask != 0x0000001fUL)) ||
			(npdisp.bpp == 16 && (pf->dwRGBBitCount != 16U || pf->dwRBitMask != 0x0000f800UL || pf->dwGBitMask != 0x000007e0UL || pf->dwBBitMask != 0x0000001fUL)) ||
			(npdisp.bpp == 24 && (pf->dwRGBBitCount != 24U || pf->dwRBitMask != 0x00ff0000UL || pf->dwGBitMask != 0x0000ff00UL || pf->dwBBitMask != 0x000000ffUL)) ||
			(npdisp.bpp == 32 && (pf->dwRGBBitCount != 32U || pf->dwRBitMask != 0x00ff0000UL || pf->dwGBitMask != 0x0000ff00UL || pf->dwBBitMask != 0x000000ffUL))) return false;
	}
	bytes = (UINT64)(UINT32)gbl.lPitch * gbl.wHeight;
	if (!bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES || bytes > 0x7fffffffUL || (UINT64)gbl.fpVidMem + bytes > ((UINT64)1 << 32)) return false;
	snapshot->linearBase = gbl.fpVidMem;
	snapshot->width = gbl.wWidth;
	snapshot->height = gbl.wHeight;
	snapshot->pitch = gbl.lPitch;
	snapshot->bpp = npdisp.bpp;
	snapshot->systemMemory = true;
	snapshot->valid = true;
	return true;
}

static bool npdisp_d3d_captureTextureSnapshot(UINT32 lpSurface, NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	NPDISP_DDPIXELFORMAT pf = { 0 };
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	UINT32 bytesPerPixel = 2U;
	UINT64 bytes;
	if (!snapshot) return false;
	memset(snapshot, 0, sizeof(*snapshot));
	if (!lpSurface || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpGbl ||
		!(lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_TEXTURE) ||
		!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl)) || !gbl.fpVidMem || !gbl.wWidth || !gbl.wHeight || gbl.lPitch <= 0) return false;
	if (lcl.dwFlags & NPDISP_DDRAWISURF_HASPIXELFORMAT) {
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT fullGbl = { 0 };
		if (!npdisp_d3d_read(&fullGbl, lcl.lpGbl, sizeof(fullGbl))) return false;
		pf = fullGbl.ddpfSurface;
		if (pf.dwSize < sizeof(pf) || (pf.dwFlags & NPDISP_DDPF_FOURCC)) return false;
		if (pf.dwFlags & NPDISP_DDPF_PALETTEINDEXED8) {
			if (pf.dwRGBBitCount != 8U) return false;
			snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_P8;
			bytesPerPixel = 1U;
		}
		else if (pf.dwFlags & NPDISP_DDPF_RGB) {
			if (pf.dwRGBBitCount == 16U &&
				((pf.dwRBitMask == 0x0000f800UL && pf.dwGBitMask == 0x000007e0UL && pf.dwBBitMask == 0x0000001fUL && !pf.dwRGBAlphaBitMask) ||
				(pf.dwRBitMask == 0x00007c00UL && pf.dwGBitMask == 0x000003e0UL && pf.dwBBitMask == 0x0000001fUL && pf.dwRGBAlphaBitMask == 0x00008000UL) ||
				(pf.dwRBitMask == 0x00000f00UL && pf.dwGBitMask == 0x000000f0UL && pf.dwBBitMask == 0x0000000fUL && pf.dwRGBAlphaBitMask == 0x0000f000UL))) {
				snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_RGB16;
			}
			else if (pf.dwRGBBitCount == 32U && pf.dwRBitMask == 0x00ff0000UL && pf.dwGBitMask == 0x0000ff00UL &&
				pf.dwBBitMask == 0x000000ffUL && (!pf.dwRGBAlphaBitMask || pf.dwRGBAlphaBitMask == 0xff000000UL)) {
				snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_RGB32;
				bytesPerPixel = 4U;
			}
			else return false;
		}
		else if ((pf.dwFlags & (NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE)) ==
			(NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE) &&
			pf.dwBumpBitCount == 16U && pf.dwBumpDuBitMask == 0x0000001fUL &&
			pf.dwBumpDvBitMask == 0x000003e0UL && pf.dwBumpLuminanceBitMask == 0x0000fc00UL) {
			snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_U5V5L6;
		}
		else if ((pf.dwFlags & NPDISP_DDPF_BUMPDUDV) && !(pf.dwFlags & NPDISP_DDPF_BUMPLUMINANCE) &&
			pf.dwBumpBitCount == 16U && pf.dwBumpDuBitMask == 0x000000ffUL &&
			pf.dwBumpDvBitMask == 0x0000ff00UL && !pf.dwBumpLuminanceBitMask) {
			snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_U8V8;
		}
		else return false;
	}
	else {
		pf.dwRGBBitCount = 16U;
		pf.dwRBitMask = 0x0000f800UL;
		pf.dwGBitMask = 0x000007e0UL;
		pf.dwBBitMask = 0x0000001fUL;
		snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_RGB16;
	}
	bytes = (UINT64)(UINT32)gbl.lPitch * gbl.wHeight;
	if ((UINT64)gbl.wWidth * bytesPerPixel > (UINT32)gbl.lPitch || !bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES) return false;
	if (lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		if (bytes > 0x7fffffffUL || (UINT64)gbl.fpVidMem + bytes > ((UINT64)1 << 32)) return false;
		snapshot->systemMemory = true;
	}
	else {
		if (!npdisp_d3d_resolveAperture(gbl.fpVidMem, gbl.lPitch, gbl.wHeight, &hostBase, &apertureOffset) || !apertureOffset) return false;
		snapshot->apertureOffset = apertureOffset;
	}
	snapshot->linearBase = gbl.fpVidMem;
	snapshot->width = gbl.wWidth;
	snapshot->height = gbl.wHeight;
	snapshot->pitch = gbl.lPitch;
	snapshot->bpp = bytesPerPixel * 8U;
	snapshot->rMask = pf.dwRBitMask;
	snapshot->gMask = pf.dwGBitMask;
	snapshot->bMask = pf.dwBBitMask;
	snapshot->aMask = pf.dwRGBAlphaBitMask;
	snapshot->duMask = pf.dwBumpDuBitMask;
	snapshot->dvMask = pf.dwBumpDvBitMask;
	snapshot->luminanceMask = pf.dwBumpLuminanceBitMask;
	snapshot->valid = true;
	return true;
}

static bool npdisp_d3d_targetFromSnapshot(const NPDISP_D3D_TARGET_SNAPSHOT* snapshot, NPDISP_D3D_TARGET* target)
{
	UINT8* hostBase = NULL;
	if (!snapshot || !snapshot->valid || !target) return false;
	if (!npdisp_d3d_resolveApertureOffset(snapshot->apertureOffset, snapshot->pitch, snapshot->height, &hostBase)) return false;
	target->sw.pixels = hostBase;
	target->sw.width = snapshot->width;
	target->sw.height = snapshot->height;
	target->sw.pitch = snapshot->pitch;
	target->sw.bpp = snapshot->bpp;
	target->apertureOffset = snapshot->apertureOffset;
	target->visible = (snapshot->apertureOffset == npdisp.mm_ddScanoutOffset);
	return true;
}

static bool npdisp_d3d_targetFromSnapshotBuffered(const NPDISP_D3D_TARGET_SNAPSHOT* snapshot, std::vector<UINT8>* storage, NPDISP_D3D_TARGET* target)
{
	UINT64 bytes;
	if (!snapshot || !snapshot->valid || !storage || !target) return false;
	if (!snapshot->systemMemory) return npdisp_d3d_targetFromSnapshot(snapshot, target);
	bytes = (UINT64)(UINT32)snapshot->pitch * snapshot->height;
	if (!snapshot->linearBase || !bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES || bytes > 0x7fffffffUL) return false;
	storage->resize((size_t)bytes);
	if (!npdisp_d3d_read(&(*storage)[0], snapshot->linearBase, (UINT32)bytes)) return false;
	target->sw.pixels = &(*storage)[0];
	target->sw.width = snapshot->width;
	target->sw.height = snapshot->height;
	target->sw.pitch = snapshot->pitch;
	target->sw.bpp = snapshot->bpp;
	target->apertureOffset = 0;
	target->visible = false;
	return true;
}

#if defined(SUPPORT_NPDISP_W2K)
static bool npdisp_d3d_w2kDepthSurfaceInfo(UINT32 lpSurface, NPDISP_D3D_SW_DEPTH_TARGET* target, UINT32* depthMask, UINT32* stencilBits)
{
	std::map<UINT32, NPDISP_D3D_W2K_DEPTH_SURFACE>::iterator it;
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	if (!lpSurface || !target) return false;
	it = npdisp_d3d_w2kDepthSurfaces.find(lpSurface);
	if (it == npdisp_d3d_w2kDepthSurfaces.end() || !it->second.snapshot.valid || it->second.snapshot.systemMemory) return false;
	apertureOffset = it->second.snapshot.apertureOffset;
	if (!npdisp_d3d_resolveApertureOffset(apertureOffset, it->second.snapshot.pitch, it->second.snapshot.height, &hostBase)) return false;
	target->pixels = hostBase;
	target->width = it->second.snapshot.width;
	target->height = it->second.snapshot.height;
	target->pitch = it->second.snapshot.pitch;
	if (depthMask) *depthMask = it->second.depthMask;
	if (stencilBits) *stencilBits = it->second.stencilBits;
	return true;
}
#endif

static bool npdisp_d3d_depthSurfaceInfo(UINT32 lpSurface, NPDISP_D3D_SW_DEPTH_TARGET* target, UINT32* depthMask, UINT32* stencilBits)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT gbl = { 0 };
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	UINT32 width;
	UINT32 height;
	UINT32 mask;
	UINT32 stencil;
	SINT32 pitch;
#if defined(SUPPORT_NPDISP_W2K)
	if (npdisp_d3d_w2kDepthSurfaceInfo(lpSurface, target, depthMask, stencilBits)) return true;
#endif
	if (!lpSurface || !target || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpGbl ||
		!(lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_ZBUFFER) || (lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_SYSTEMMEMORY) ||
		!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl))) return false;
	if (lcl.dwFlags & NPDISP_DDRAWISURF_HASPIXELFORMAT) {
		if (gbl.ddpfSurface.dwSize < sizeof(NPDISP_DDPIXELFORMAT) || !(gbl.ddpfSurface.dwFlags & NPDISP_DDPF_ZBUFFER) ||
			gbl.ddpfSurface.dwZBufferBitDepth != 16U) return false;
		if (gbl.ddpfSurface.dwFlags & NPDISP_DDPF_STENCILBUFFER) {
			stencil = gbl.ddpfSurface.dwStencilBitDepth;
			if (stencil == 1U) {
				if (gbl.ddpfSurface.dwZBitMask != 0x00007fffUL || gbl.ddpfSurface.dwStencilBitMask != 0x00008000UL) return false;
				mask = 0x00007fffUL;
			}
			else if (stencil == 4U) {
				if (gbl.ddpfSurface.dwZBitMask != 0x00000fffUL || gbl.ddpfSurface.dwStencilBitMask != 0x0000f000UL) return false;
				mask = 0x00000fffUL;
			}
			else return false;
		}
		else {
			if (gbl.ddpfSurface.dwStencilBitDepth || gbl.ddpfSurface.dwStencilBitMask ||
				(gbl.ddpfSurface.dwZBitMask && gbl.ddpfSurface.dwZBitMask != 0x0000ffffUL)) return false;
			mask = 0x0000ffffUL;
			stencil = 0U;
		}
	}
	else {
		// Older Win9x runtimes may omit HASPIXELFORMAT even though this HAL only creates Z16 surfaces.
		mask = 0x0000ffffUL;
		stencil = 0U;
	}
	width = gbl.head.wWidth;
	height = gbl.head.wHeight;
	pitch = gbl.head.lPitch;
	if (!width || !height || pitch <= 0 || (UINT64)width * 2U > (UINT32)pitch ||
		!npdisp_d3d_resolveAperture(gbl.head.fpVidMem, pitch, height, &hostBase, &apertureOffset)) return false;
	target->pixels = hostBase;
	target->width = width;
	target->height = height;
	target->pitch = pitch;
	if (depthMask) *depthMask = mask;
	if (stencilBits) *stencilBits = stencil;
	return true;
}

static bool npdisp_d3d_depthTarget(UINT32 lpSurface, NPDISP_D3D_SW_DEPTH_TARGET* target)
{
	return npdisp_d3d_depthSurfaceInfo(lpSurface, target, NULL, NULL);
}

static bool npdisp_d3d_rawBufferRead(UINT32 base, UINT32 offset, void* dst, UINT32 size)
{
	UINT32 addr;
	UINT32 apertureOffset;
	UINT64 end;
	if (!base || !dst || !size || !npdisp_d3d_add(base, offset, &addr)) {
		TRACEOUTD3D(("NPDISP11 D3D_RAWBUF_REJECT stage=params base=%08x off=%u size=%u", base, offset, size));
		return false;
	}
	if (npdisp.mm_vramLinearAddr && addr >= npdisp.mm_vramLinearAddr) {
		apertureOffset = addr - npdisp.mm_vramLinearAddr;
		end = (UINT64)apertureOffset + size;
		if (apertureOffset < NPDISP_DD_PRIMARY_REGION_SIZE && end <= NPDISP_DD_PRIMARY_REGION_SIZE && npdisp.mm_screenPtr &&
			(UINT64)apertureOffset + size <= npdisp.mm_screenSize) {
			const UINT8* src = npdisp.mm_screenPtr + apertureOffset;
			UINT8* out = (UINT8*)dst;
			for (UINT32 i = 0; i < size; ++i) out[i] = src[i];
			return true;
		}
		if (apertureOffset >= NPDISP_DD_OFFSCREEN_OFFSET && end <= NPDISP_DD_APERTURE_SIZE && npdisp.mm_ddOffscreenPtr &&
			npdisp.mm_ddOffscreenSize == NPDISP_DD_OFFSCREEN_SIZE) {
			const UINT8* src = npdisp.mm_ddOffscreenPtr + (apertureOffset - NPDISP_DD_OFFSCREEN_OFFSET);
			UINT8* out = (UINT8*)dst;
			if ((UINT64)(apertureOffset - NPDISP_DD_OFFSCREEN_OFFSET) + size > NPDISP_DD_OFFSCREEN_SIZE) return false;
			for (UINT32 i = 0; i < size; ++i) out[i] = src[i];
			return true;
		}
	}
	if (!npdisp_d3d_read(dst, addr, size)) {
		TRACEOUTD3D(("NPDISP11 D3D_RAWBUF_REJECT stage=memory base=%08x addr=%08x off=%u size=%u", base, addr, offset, size));
		return false;
	}
	return true;
}

static bool npdisp_d3d_lclBufferRead(UINT32 lpSurfaceLcl, UINT32 offset, void* dst, UINT32 size)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	if (!lpSurfaceLcl || !dst || !size) {
		TRACEOUTD3D(("NPDISP11 D3D_LCLBUF_REJECT stage=params surf=%08x off=%u size=%u", lpSurfaceLcl, offset, size));
		return false;
	}
	if (!npdisp_d3d_read(&lcl, lpSurfaceLcl, sizeof(lcl))) {
		TRACEOUTD3D(("NPDISP11 D3D_LCLBUF_REJECT stage=lcl surf=%08x off=%u size=%u", lpSurfaceLcl, offset, size));
		return false;
	}
	if (!lcl.lpGbl) {
		TRACEOUTD3D(("NPDISP11 D3D_LCLBUF_REJECT stage=lcl-gbl surf=%08x off=%u size=%u", lpSurfaceLcl, offset, size));
		return false;
	}
	if (!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl))) {
		TRACEOUTD3D(("NPDISP11 D3D_LCLBUF_REJECT stage=gbl surf=%08x gbl=%08x off=%u size=%u", lpSurfaceLcl, lcl.lpGbl, offset, size));
		return false;
	}
	if (!gbl.fpVidMem) {
		TRACEOUTD3D(("NPDISP11 D3D_LCLBUF_REJECT stage=address surf=%08x gbl=%08x fp=%08x off=%u size=%u", lpSurfaceLcl, lcl.lpGbl, gbl.fpVidMem, offset, size));
		return false;
	}
	if (!npdisp_d3d_rawBufferRead(gbl.fpVidMem, offset, dst, size)) {
		TRACEOUTD3D(("NPDISP11 D3D_LCLBUF_REJECT stage=memory surf=%08x gbl=%08x fp=%08x off=%u size=%u", lpSurfaceLcl, lcl.lpGbl, gbl.fpVidMem, offset, size));
		return false;
	}
	return true;
}

static bool npdisp_d3d_bufferRead(UINT32 lpSurface, UINT32 offset, void* dst, UINT32 size)
{
	NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
	if (!lpSurface || !dst || !size) return false;
	if (npdisp_d3d_read(&surfaceInt, lpSurface, sizeof(surfaceInt)) && surfaceInt.lpLcl &&
		npdisp_d3d_lclBufferRead(surfaceInt.lpLcl, offset, dst, size)) return true;
	if (npdisp_d3d_lclBufferRead(lpSurface, offset, dst, size)) return true;
	TRACEOUTD3D(("NPDISP11 D3D_BUFFER_REJECT surf=%08x off=%u size=%u", lpSurface, offset, size));
	return false;
}

static bool npdisp_d3d_applyStates(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DSTATE32* states, UINT32 count)
{
	if (!context || (!states && count)) return false;
	for (UINT32 i = 0; i < count; ++i) {
		UINT32 type = states[i].type;
		if (type > NPDISP_D3DSTATE_OVERRIDE_BIAS && type - NPDISP_D3DSTATE_OVERRIDE_BIAS < NPDISP_D3D_MAX_RENDERSTATES) {
			UINT32 overrideType = type - NPDISP_D3DSTATE_OVERRIDE_BIAS;
			context->renderStateOverrides[overrideType] = states[i].value ? 1U : 0U;
			TRACEOUTD3D(("NPDISP11 D3D_RS_OVERRIDE state=%u enabled=%u", overrideType, context->renderStateOverrides[overrideType]));
			continue;
		}
		if (type >= NPDISP_D3D_MAX_RENDERSTATES) continue;
		if (context->renderStateOverrides[type]) {
			TRACEOUTD3D(("NPDISP11 D3D_RS_OVERRIDE_SKIP state=%u value=%08x", type, states[i].value));
			continue;
		}
		if (type == NPDISP_D3DRENDERSTATE_VERTEXBLEND && states[i].value > NPDISP_D3DVBLEND_3WEIGHTS) return false;
		if (type == NPDISP_D3DRENDERSTATE_SCENECAPTURE) context->sceneActive = states[i].value ? true : false;
		context->renderStates[type] = states[i].value;
	}
	return true;
}

static UINT32 npdisp_d3d_normalizeTextureFilterState(UINT32 state, UINT32 value)
{
	if (state == NPDISP_D3DTSS_MAGFILTER) {
		if (value >= 3U && value <= 6U) return NPDISP_D3DTFG_LINEAR;
	}
	else if (state == NPDISP_D3DTSS_MINFILTER) {
		if (value >= 3U && value <= 6U) return NPDISP_D3DTFN_LINEAR;
	}
	return value;
}

static bool npdisp_d3d_applyTextureStageStates(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DP2TEXTURESTAGESTATE32* states, UINT32 count)
{
	if (!context || (!states && count)) return false;
	for (UINT32 i = 0; i < count; ++i) {
		if (states[i].wStage >= NPDISP_D3D_MAX_TEXTURE_STAGES || states[i].TSState > NPDISP_D3DTSS_MAX_STATE) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TSS_REJECT index=%u stage=%u state=%u value=%08x", i, states[i].wStage, states[i].TSState, states[i].dwValue));
			return false;
		}
	}
	for (UINT32 i = 0; i < count; ++i) {
		UINT32 value = npdisp_d3d_normalizeTextureFilterState(states[i].TSState, states[i].dwValue);
		context->textureStageStates[states[i].wStage][states[i].TSState] = value;
		if (states[i].wStage < NPDISP_D3D_RASTER_TEXTURE_STAGES) {
			if (value != states[i].dwValue) {
				TRACEOUTD3D(("NPDISP11 D3D_DP2_TSS_FILTER_FALLBACK stage=%u state=%u value=%08x fallback=%08x", states[i].wStage, states[i].TSState, states[i].dwValue, value));
			}
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TSS_STATE stage=%u state=%u value=%08x", states[i].wStage, states[i].TSState, value));
		}
	}
	return true;
}

static bool npdisp_d3d_drawState(const NPDISP_D3D_CONTEXT* context, NPDISP_D3D_RASTERSTATE* state)
{
	if (!context || !state) return false;
	memset(state, 0, sizeof(*state));
	for (UINT32 stage = NPDISP_D3D_RASTER_TEXTURE_STAGES; stage < NPDISP_D3D_MAX_TEXTURE_STAGES; ++stage) {
		if (context->textureStageStates[stage][NPDISP_D3DTSS_TEXTUREMAP] ||
			(context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP] &&
			 context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP] != NPDISP_D3DTOP_DISABLE)) {
			TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=multitexture stage=%u handle=%08x op=%u", stage,
				context->textureStageStates[stage][NPDISP_D3DTSS_TEXTUREMAP],
				context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP]));
			return false;
		}
	}
	state->zEnable = context->renderStates[NPDISP_D3DRENDERSTATE_ZENABLE];
	state->zWriteEnable = context->renderStates[NPDISP_D3DRENDERSTATE_ZWRITEENABLE] ? 1U : 0U;
	state->zFunc = context->renderStates[NPDISP_D3DRENDERSTATE_ZFUNC];
	state->wNear = context->wNear;
	state->wFar = context->wFar;
	state->depthMask = 0x0000ffffUL;
	state->stencilBits = 0U;
	state->stencilEnable = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILENABLE] ? 1U : 0U;
	state->stencilFail = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILFAIL];
	state->stencilZFail = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILZFAIL];
	state->stencilPass = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILPASS];
	state->stencilFunc = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILFUNC];
	state->stencilRef = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILREF];
	state->stencilReadMask = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILMASK];
	state->stencilWriteMask = context->renderStates[NPDISP_D3DRENDERSTATE_STENCILWRITEMASK];
	if (state->zEnable > 2U) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=depth-mode value=%08x", state->zEnable));
		return false;
	}
	if (state->zEnable && (state->zFunc < NPDISP_D3DCMP_NEVER || state->zFunc > NPDISP_D3DCMP_ALWAYS)) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=zfunc value=%08x", state->zFunc));
		return false;
	}
	if (state->stencilEnable) {
		if (state->stencilFunc < NPDISP_D3DCMP_NEVER || state->stencilFunc > NPDISP_D3DCMP_ALWAYS ||
			state->stencilFail < NPDISP_D3DSTENCILOP_KEEP || state->stencilFail > NPDISP_D3DSTENCILOP_DECR ||
			state->stencilZFail < NPDISP_D3DSTENCILOP_KEEP || state->stencilZFail > NPDISP_D3DSTENCILOP_DECR ||
			state->stencilPass < NPDISP_D3DSTENCILOP_KEEP || state->stencilPass > NPDISP_D3DSTENCILOP_DECR) {
			TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=stencil func=%u fail=%u zfail=%u pass=%u",
				state->stencilFunc, state->stencilFail, state->stencilZFail, state->stencilPass));
			return false;
		}
	}
	state->alphaBlendEnable = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHABLENDENABLE] ? 1U : 0U;
	state->srcBlend = context->renderStates[NPDISP_D3DRENDERSTATE_SRCBLEND];
	state->destBlend = context->renderStates[NPDISP_D3DRENDERSTATE_DESTBLEND];
	if (state->alphaBlendEnable && state->srcBlend == NPDISP_D3DBLEND_BOTHSRCALPHA) {
		state->srcBlend = NPDISP_D3DBLEND_SRCALPHA;
		state->destBlend = NPDISP_D3DBLEND_INVSRCALPHA;
	} else if (state->alphaBlendEnable && state->srcBlend == NPDISP_D3DBLEND_BOTHINVSRCALPHA) {
		state->srcBlend = NPDISP_D3DBLEND_INVSRCALPHA;
		state->destBlend = NPDISP_D3DBLEND_SRCALPHA;
	}
	if (state->alphaBlendEnable &&
		(state->srcBlend < NPDISP_D3DBLEND_ZERO || state->srcBlend > NPDISP_D3DBLEND_SRCALPHASAT ||
		 state->destBlend < NPDISP_D3DBLEND_ZERO || state->destBlend > NPDISP_D3DBLEND_SRCALPHASAT)) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=blend src=%08x dst=%08x", state->srcBlend, state->destBlend));
		return false;
	}
	state->fillMode = context->renderStates[NPDISP_D3DRENDERSTATE_FILLMODE];
	if (state->fillMode != NPDISP_D3DFILL_SOLID && state->fillMode != NPDISP_D3DFILL_WIREFRAME) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=fill value=%08x", state->fillMode));
		return false;
	}
	state->shadeMode = context->renderStates[NPDISP_D3DRENDERSTATE_SHADEMODE];
	state->cullMode = context->renderStates[NPDISP_D3DRENDERSTATE_CULLMODE];
	state->ditherEnable = context->renderStates[NPDISP_D3DRENDERSTATE_DITHERENABLE];
	state->lastPixel = context->renderStates[NPDISP_D3DRENDERSTATE_LASTPIXEL];
	state->specularEnable = context->renderStates[NPDISP_D3DRENDERSTATE_SPECULARENABLE] ? 1U : 0U;
	state->alphaTestEnable = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHATESTENABLE] ? 1U : 0U;
	state->alphaRef = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHAREF] & 0xffU;
	state->alphaFunc = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHAFUNC];
	state->fogEnable = context->renderStates[NPDISP_D3DRENDERSTATE_FOGENABLE] ? 1U : 0U;
	state->fogColor = context->renderStates[NPDISP_D3DRENDERSTATE_FOGCOLOR];
	for (UINT32 coord = 0; coord < NPDISP_D3D_TEXTURE_COORD_SETS; ++coord)
		state->coordWrap[coord] = context->renderStates[NPDISP_D3DRENDERSTATE_WRAP0 + coord];
	if (state->shadeMode != NPDISP_D3DSHADE_FLAT && state->shadeMode != NPDISP_D3DSHADE_GOURAUD) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=shade value=%08x", state->shadeMode));
		return false;
	}
	if (state->cullMode != NPDISP_D3DCULL_NONE && state->cullMode != NPDISP_D3DCULL_CW && state->cullMode != NPDISP_D3DCULL_CCW) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=cull value=%08x", state->cullMode));
		return false;
	}
	if (state->ditherEnable > 1U || state->lastPixel > 1U) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=raster-bool dither=%u last=%u", state->ditherEnable, state->lastPixel));
		return false;
	}
	if (state->alphaTestEnable && (state->alphaFunc < NPDISP_D3DCMP_NEVER || state->alphaFunc > NPDISP_D3DCMP_ALWAYS)) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=alphafunc value=%08x", state->alphaFunc));
		return false;
	}
	return true;
}

static bool npdisp_d3d_readTextureSnapshot(const NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot, UINT8* dst, UINT32 size)
{
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	UINT64 bytes;
	if (!snapshot || !snapshot->valid || !dst) return false;
	bytes = (UINT64)(UINT32)snapshot->pitch * snapshot->height;
	if (!bytes || bytes != size || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES) return false;
	if (snapshot->systemMemory) return npdisp_d3d_read(dst, snapshot->linearBase, size);
	if (!snapshot->linearBase) {
		if (!npdisp_d3d_resolveApertureOffset(snapshot->apertureOffset, snapshot->pitch, snapshot->height, &hostBase)) return false;
	}
	else if (!npdisp_d3d_resolveAperture(snapshot->linearBase, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
		apertureOffset != snapshot->apertureOffset) return false;
	memcpy(dst, hostBase, size);
	return true;
}

static bool npdisp_d3d_textureSnapshotPointer(const NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot, const UINT8** pixels)
{
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	if (!snapshot || !snapshot->valid || !pixels || snapshot->systemMemory) return false;
	if (!snapshot->linearBase) {
		if (!npdisp_d3d_resolveApertureOffset(snapshot->apertureOffset, snapshot->pitch, snapshot->height, &hostBase)) return false;
	}
	else if (!npdisp_d3d_resolveAperture(snapshot->linearBase, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
		apertureOffset != snapshot->apertureOffset) return false;
	*pixels = hostBase;
	return true;
}

static bool npdisp_d3d_surfaceCaps2(UINT32 lpSurface, UINT32* caps2)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_MORE_HEAD32 more = { 0 };
	UINT32 addr;
	if (!caps2) return false;
	*caps2 = 0;
	if (!lpSurface || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpSurfMore ||
		!npdisp_d3d_read(&more, lcl.lpSurfMore, sizeof(more)) ||
		more.dwSize < NPDISP_DDRAWSURFACE_CAPS2_OFS_DX7 + sizeof(UINT32) ||
		!npdisp_d3d_add(lcl.lpSurfMore, NPDISP_DDRAWSURFACE_CAPS2_OFS_DX7, &addr) ||
		!npdisp_d3d_read(caps2, addr, sizeof(*caps2))) return false;
	return true;
}

static SINT32 npdisp_d3d_cubeFaceIndex(UINT32 caps2)
{
	switch (caps2 & NPDISP_DDSCAPS2_CUBEMAP_ALLFACES) {
	case NPDISP_DDSCAPS2_CUBEMAP_POSITIVEX: return 0;
	case NPDISP_DDSCAPS2_CUBEMAP_NEGATIVEX: return 1;
	case NPDISP_DDSCAPS2_CUBEMAP_POSITIVEY: return 2;
	case NPDISP_DDSCAPS2_CUBEMAP_NEGATIVEY: return 3;
	case NPDISP_DDSCAPS2_CUBEMAP_POSITIVEZ: return 4;
	case NPDISP_DDSCAPS2_CUBEMAP_NEGATIVEZ: return 5;
	case NPDISP_DDSCAPS2_CUBEMAP_ALLFACES: return 0;
	default: return -1;
	}
}

static bool npdisp_d3d_cubeSnapshotFromSurface(UINT32 lpSurface, NPDISP_D3D_TEXTURE_SNAPSHOT* snapshots, UINT32* found)
{
	UINT32 caps2 = 0;
	SINT32 face;
	if (!lpSurface || !snapshots || !found || !npdisp_d3d_surfaceCaps2(lpSurface, &caps2)) return false;
	if (!(caps2 & NPDISP_DDSCAPS2_CUBEMAP) || (caps2 & NPDISP_DDSCAPS2_MIPMAPSUBLEVEL)) return true;
	face = npdisp_d3d_cubeFaceIndex(caps2);
	if (face < 0 || face >= (SINT32)NPDISP_D3D_CUBE_FACES) return false;
	if (!npdisp_d3d_captureTextureSnapshot(lpSurface, &snapshots[face])) return false;
	*found |= 1U << face;
	return true;
}

static bool npdisp_d3d_cubeSnapshotsFromList(UINT32 listAddr, NPDISP_D3D_TEXTURE_SNAPSHOT* snapshots, UINT32* found)
{
	for (UINT32 i = 0; listAddr && i < 64U; ++i) {
		NPDISP_DDATTACHLIST list = { 0 };
		UINT32 attached = 0;
		if (!npdisp_d3d_read(&list, listAddr, sizeof(list))) return false;
		if (list.lpAttached) {
			attached = list.lpAttached;
			if (!npdisp_d3d_cubeSnapshotFromSurface(attached, snapshots, found)) return false;
		}
		if (list.lpIAttached) {
			NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
			if (!npdisp_d3d_read(&surfaceInt, list.lpIAttached, sizeof(surfaceInt))) return false;
			if (surfaceInt.lpLcl && surfaceInt.lpLcl != attached &&
				!npdisp_d3d_cubeSnapshotFromSurface(surfaceInt.lpLcl, snapshots, found)) return false;
		}
		listAddr = list.lpLink;
	}
	return true;
}

static bool npdisp_d3d_captureCubeSnapshots(UINT32 lpSurface, NPDISP_D3D_TEXTURE_SNAPSHOT* snapshots)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	UINT32 found = 0;
	if (!lpSurface || !snapshots || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl))) return false;
	memset(snapshots, 0, sizeof(NPDISP_D3D_TEXTURE_SNAPSHOT) * NPDISP_D3D_CUBE_FACES);
	if (!npdisp_d3d_cubeSnapshotFromSurface(lpSurface, snapshots, &found) ||
		!npdisp_d3d_cubeSnapshotsFromList(lcl.lpAttachList, snapshots, &found) ||
		!npdisp_d3d_cubeSnapshotsFromList(lcl.lpAttachListFrom, snapshots, &found)) return false;
	return found == ((1U << NPDISP_D3D_CUBE_FACES) - 1U);
}

static bool npdisp_d3d_mipSurfaceMatches(UINT32 lpSurface, UINT32 width, UINT32 height)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	if (!lpSurface || !width || !height || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpGbl ||
		(lcl.ddsCaps.dwCaps & (NPDISP_DDSCAPS_TEXTURE | NPDISP_DDSCAPS_MIPMAP)) !=
			(NPDISP_DDSCAPS_TEXTURE | NPDISP_DDSCAPS_MIPMAP) || !npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl))) return false;
	return gbl.wWidth == width && gbl.wHeight == height;
}

static bool npdisp_d3d_mipSurfaceFromList(UINT32 listAddr, UINT32 width, UINT32 height, UINT32* lpSurface)
{
	if (!lpSurface) return false;
	for (UINT32 i = 0; listAddr && i < 64U; ++i) {
		NPDISP_DDATTACHLIST list = { 0 };
		if (!npdisp_d3d_read(&list, listAddr, sizeof(list))) return false;
		if (list.lpAttached && npdisp_d3d_mipSurfaceMatches(list.lpAttached, width, height)) {
			*lpSurface = list.lpAttached;
			return true;
		}
		if (list.lpIAttached) {
			NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
			if (!npdisp_d3d_read(&surfaceInt, list.lpIAttached, sizeof(surfaceInt))) return false;
			if (surfaceInt.lpLcl && npdisp_d3d_mipSurfaceMatches(surfaceInt.lpLcl, width, height)) {
				*lpSurface = surfaceInt.lpLcl;
				return true;
			}
		}
		listAddr = list.lpLink;
	}
	return true;
}

static bool npdisp_d3d_nextMipSurface(UINT32 lpSurface, UINT32 width, UINT32 height, UINT32* lpNext)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	UINT32 nextWidth;
	UINT32 nextHeight;
	if (!lpSurface || !width || !height || !lpNext || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl))) return false;
	*lpNext = 0;
	nextWidth = width > 1U ? width >> 1 : 1U;
	nextHeight = height > 1U ? height >> 1 : 1U;
	if (nextWidth == width && nextHeight == height) return true;
	if (!npdisp_d3d_mipSurfaceFromList(lcl.lpAttachList, nextWidth, nextHeight, lpNext)) return false;
	if (*lpNext) return true;
	return npdisp_d3d_mipSurfaceFromList(lcl.lpAttachListFrom, nextWidth, nextHeight, lpNext);
}

static bool npdisp_d3d_captureMipSnapshots(UINT32 lpSurface, const NPDISP_D3D_TEXTURE_SNAPSHOT* top, NPDISP_D3D_TEXTURE_SNAPSHOT* snapshots, UINT32* count)
{
	UINT32 currentSurface = lpSurface;
	if (!lpSurface || !top || !top->valid || !snapshots || !count) return false;
	memset(snapshots, 0, sizeof(NPDISP_D3D_TEXTURE_SNAPSHOT) * NPDISP_D3D_MAX_MIP_LEVELS);
	snapshots[0] = *top;
	*count = 1;
	for (UINT32 level = 1; level < NPDISP_D3D_MAX_MIP_LEVELS; ++level) {
		UINT32 nextSurface = 0;
		NPDISP_D3D_TEXTURE_SNAPSHOT next;
		const NPDISP_D3D_TEXTURE_SNAPSHOT* prev = &snapshots[level - 1U];
		if (!npdisp_d3d_nextMipSurface(currentSurface, prev->width, prev->height, &nextSurface)) return false;
		if (!nextSurface) break;
		if (!npdisp_d3d_captureTextureSnapshot(nextSurface, &next) || next.bpp != top->bpp || next.format != top->format ||
			next.rMask != top->rMask || next.gMask != top->gMask || next.bMask != top->bMask || next.aMask != top->aMask ||
			next.duMask != top->duMask || next.dvMask != top->dvMask || next.luminanceMask != top->luminanceMask) return false;
		snapshots[level] = next;
		*count = level + 1U;
		currentSurface = nextSurface;
	}
	return true;
}

static bool npdisp_d3d_textureRow(const NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot, UINT32 x, UINT32 y, UINT32 width, UINT8* row, bool write)
{
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	UINT32 bytesPerPixel;
	UINT32 offset;
	UINT32 bytes;
	UINT32 addr;
	if (!snapshot || !snapshot->valid || !row || x > snapshot->width || y >= snapshot->height || width > snapshot->width - x ||
		(snapshot->bpp != 8U && snapshot->bpp != 16U && snapshot->bpp != 32U)) return false;
	bytesPerPixel = snapshot->bpp >> 3;
	if (width > 0x7fffffffUL / bytesPerPixel) return false;
	bytes = width * bytesPerPixel;
	if ((UINT64)y * (UINT32)snapshot->pitch + (UINT64)x * bytesPerPixel > 0xffffffffULL) return false;
	offset = y * (UINT32)snapshot->pitch + x * bytesPerPixel;
	if (snapshot->systemMemory) {
		if (!npdisp_d3d_add(snapshot->linearBase, offset, &addr)) return false;
		return write ? npdisp_d3d_write(row, addr, bytes) : npdisp_d3d_read(row, addr, bytes);
	}
	if (!snapshot->linearBase) {
		if (!npdisp_d3d_resolveApertureOffset(snapshot->apertureOffset, snapshot->pitch, snapshot->height, &hostBase)) return false;
	}
	else if (!npdisp_d3d_resolveAperture(snapshot->linearBase, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
		apertureOffset != snapshot->apertureOffset) return false;
	if (write) memcpy(hostBase + offset, row, bytes);
	else memcpy(row, hostBase + offset, bytes);
	return true;
}

static float npdisp_d3d_stateFloat(UINT32 value)
{
	float result;
	memcpy(&result, &value, sizeof(result));
	return result;
}

static bool npdisp_d3d_prepareTextureStage(NPDISP_D3D_CONTEXT* context, UINT32 stage, NPDISP_D3D_RASTERSTATE* state, std::vector<UINT8>* storage, NPDISP_D3D_TEXTURE* texture, bool directVideo, UINT32* boundSurface)
{
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it;
	const NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot;
	NPDISP_D3D_TEXTURE_SNAPSHOT cubeSnapshots[NPDISP_D3D_CUBE_FACES];
	NPDISP_D3D_TEXTURE_SNAPSHOT mipSnapshots[NPDISP_D3D_MAX_MIP_LEVELS];
	UINT32 handle, colorOp, colorArg1, colorArg2, alphaOp, alphaArg1, alphaArg2, addressU, addressV, magFilter, minFilter, mipFilter, texCoordIndex, texCoordState, textureTransformFlags;
	UINT32 textureSurface = 0;
	UINT32 textureNamespace = 0;
	NPDISP_D3D_TEXTURE_SNAPSHOT* textureSnapshot = NULL;
	const NPDISP_D3D_PALETTE* palette = NULL;
	UINT32 mipCount = 1U;
	UINT32 caps2 = 0;
	UINT64 bytes;
	NPDISP_DDCOLORKEY textureColorKey = { 0 };
	bool colorKeyEnable = false;
	bool legacyTextureState = false;
	bool cubeMap = false;
	if (!context || !state || !storage || !texture || stage >= NPDISP_D3D_RASTER_TEXTURE_STAGES) return false;
	if (boundSurface) *boundSurface = 0;
	state->textures[stage] = NULL;
	colorOp = context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP];
	if (!colorOp) colorOp = stage ? NPDISP_D3DTOP_DISABLE : NPDISP_D3DTOP_MODULATE;
	if (colorOp == NPDISP_D3DTOP_DISABLE) return true;
	handle = context->textureStageStates[stage][NPDISP_D3DTSS_TEXTUREMAP];
	if (!handle && stage == 0) {
		handle = context->renderStates[NPDISP_D3DRENDERSTATE_TEXTUREHANDLE];
		legacyTextureState = handle != 0;
		if (handle) {
			if (context->ddLocalNamespace) {
				it = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, handle));
				if (it != npdisp_d3d_surfaces.end()) {
					textureSurface = it->second.lpSurface;
					textureNamespace = it->second.ddLocalNamespace;
					textureSnapshot = &it->second.textureSnapshot;
				}
			}
			else {
				for (it = npdisp_d3d_surfaces.begin(); it != npdisp_d3d_surfaces.end(); ++it) {
					if ((UINT32)it->first != handle) continue;
					if (textureSurface) {
						textureSurface = 0;
						textureSnapshot = NULL;
						break;
					}
					textureSurface = it->second.lpSurface;
					textureNamespace = it->second.ddLocalNamespace;
					textureSnapshot = &it->second.textureSnapshot;
				}
			}
			if (!textureSurface) {
				std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator legacy = npdisp_d3d_legacyTextures.find(handle);
				if (legacy == npdisp_d3d_legacyTextures.end()) {
					TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=legacy-handle stage=%u handle=%08x", stage, handle));
					return false;
				}
				textureSurface = legacy->second.lpSurface;
				textureSnapshot = &legacy->second.textureSnapshot;
			}
		}
	}
	if (!handle) {
		if (stage == 0) return true;
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=no-handle stage=%u op=%u", stage, colorOp));
		return false;
	}
	if (!textureSurface) {
		std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator legacy;
		if (!context->ddLocalNamespace && !legacyTextureState && !npdisp_d3d_bindDDLocalNamespaceByHandle(context, handle)) {
			legacy = npdisp_d3d_legacyTextures.find(handle);
			if (legacy == npdisp_d3d_legacyTextures.end() || npdisp_d3d_getContext(legacy->second.dwhContext) != context) {
				TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=namespace stage=%u handle=%08x", stage, handle));
				return false;
			}
			textureSurface = legacy->second.lpSurface;
			textureSnapshot = &legacy->second.textureSnapshot;
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_LEGACY_BIND stage=%u handle=%08x surf=%08x", stage, handle, textureSurface));
		}
		if (!textureSurface) {
			if (!context->ddLocalNamespace) return false;
			it = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, handle));
			if (it == npdisp_d3d_surfaces.end()) {
				legacy = npdisp_d3d_legacyTextures.find(handle);
				if (legacy == npdisp_d3d_legacyTextures.end() || npdisp_d3d_getContext(legacy->second.dwhContext) != context) {
					TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=surface stage=%u ns=%08x handle=%08x", stage, context->ddLocalNamespace, handle));
					return false;
				}
				textureSurface = legacy->second.lpSurface;
				textureSnapshot = &legacy->second.textureSnapshot;
				TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_LEGACY_BIND stage=%u handle=%08x surf=%08x", stage, handle, textureSurface));
			}
			else {
				textureSurface = it->second.lpSurface;
				textureNamespace = it->second.ddLocalNamespace;
				textureSnapshot = &it->second.textureSnapshot;
				if (!it->second.caps2 && npdisp_d3d_surfaceCaps2(textureSurface, &caps2)) it->second.caps2 = caps2;
				else caps2 = it->second.caps2;
			}
		}
	}
	else npdisp_d3d_surfaceCaps2(textureSurface, &caps2);
	cubeMap = (caps2 & NPDISP_DDSCAPS2_CUBEMAP) != 0;
	if (boundSurface) *boundSurface = textureSurface;
	if (context->renderStates[NPDISP_D3DRENDERSTATE_COLORKEYENABLE]) {
		NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS lclKeys = { 0 };
		if (!npdisp_d3d_read(&lclKeys, textureSurface, sizeof(lclKeys))) return false;
		if (lclKeys.head.dwFlags & NPDISP_DDRAWISURF_HASCKEYSRCBLT) {
			textureColorKey = lclKeys.ddckCKSrcBlt;
			if (textureColorKey.dwColorSpaceLowValue > textureColorKey.dwColorSpaceHighValue) return false;
			colorKeyEnable = true;
		}
	}
	if (!directVideo && ((cubeMap && npdisp_d3d_asyncBusy()) || (!cubeMap && npdisp_d3d_asyncPendingTarget(textureSurface)))) npdisp_d3d_flush();
	if (cubeMap) {
		if (!npdisp_d3d_captureCubeSnapshots(textureSurface, cubeSnapshots)) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=cubemap-faces stage=%u ns=%08x handle=%08x surf=%08x caps2=%08x", stage, context->ddLocalNamespace, handle, textureSurface, caps2));
			return false;
		}
		snapshot = &cubeSnapshots[0];
	}
	else {
		if (!textureSnapshot || (!textureSnapshot->valid && !npdisp_d3d_captureTextureSnapshot(textureSurface, textureSnapshot))) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=snapshot stage=%u ns=%08x handle=%08x surf=%08x", stage, context->ddLocalNamespace, handle, textureSurface));
			return false;
		}
		snapshot = textureSnapshot;
	}
	if (snapshot->format == NPDISP_D3D_TEXTURE_FORMAT_P8) {
		std::map<UINT64, UINT32>::const_iterator association;
		std::map<UINT64, NPDISP_D3D_PALETTE>::const_iterator paletteIt;
		if (!textureNamespace) textureNamespace = context->ddLocalNamespace;
		association = npdisp_d3d_surfacePalettes.find(npdisp_d3d_surfaceKey(textureNamespace, handle));
		if (!textureNamespace || association == npdisp_d3d_surfacePalettes.end()) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=palette-association stage=%u ns=%08x handle=%08x", stage, textureNamespace, handle));
			return false;
		}
		paletteIt = npdisp_d3d_palettes.find(npdisp_d3d_surfaceKey(textureNamespace, association->second));
		if (paletteIt == npdisp_d3d_palettes.end()) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=palette-data stage=%u ns=%08x handle=%08x palette=%08x", stage, textureNamespace, handle, association->second));
			return false;
		}
		palette = &paletteIt->second;
	}
	colorArg1 = context->textureStageStates[stage][NPDISP_D3DTSS_COLORARG1];
	colorArg2 = context->textureStageStates[stage][NPDISP_D3DTSS_COLORARG2];
	alphaOp = context->textureStageStates[stage][NPDISP_D3DTSS_ALPHAOP];
	alphaArg1 = context->textureStageStates[stage][NPDISP_D3DTSS_ALPHAARG1];
	alphaArg2 = context->textureStageStates[stage][NPDISP_D3DTSS_ALPHAARG2];
	if (legacyTextureState) {
		UINT32 textureBlend = context->renderStates[NPDISP_D3DRENDERSTATE_TEXTUREMAPBLEND];
		if (!textureBlend) textureBlend = NPDISP_D3DTBLEND_MODULATE;
		if (textureBlend == NPDISP_D3DTBLEND_MODULATE) {
			colorOp = NPDISP_D3DTOP_MODULATE;
			colorArg1 = NPDISP_D3DTA_TEXTURE;
			colorArg2 = NPDISP_D3DTA_DIFFUSE;
			alphaOp = NPDISP_D3DTOP_SELECTARG1;
			alphaArg1 = (snapshot->aMask || (palette && (palette->flags & NPDISP_DDRAWIPAL_ALPHA))) ? NPDISP_D3DTA_TEXTURE : NPDISP_D3DTA_DIFFUSE;
		}
	}
	addressU = context->textureStageStates[stage][NPDISP_D3DTSS_ADDRESSU];
	addressV = context->textureStageStates[stage][NPDISP_D3DTSS_ADDRESSV];
	if (!addressU) addressU = context->textureStageStates[stage][NPDISP_D3DTSS_ADDRESS];
	if (!addressV) addressV = context->textureStageStates[stage][NPDISP_D3DTSS_ADDRESS];
	if (!addressU) addressU = NPDISP_D3DTADDRESS_WRAP;
	if (!addressV) addressV = NPDISP_D3DTADDRESS_WRAP;
	magFilter = context->textureStageStates[stage][NPDISP_D3DTSS_MAGFILTER];
	minFilter = context->textureStageStates[stage][NPDISP_D3DTSS_MINFILTER];
	if (!magFilter) magFilter = NPDISP_D3DTFG_POINT;
	if (!minFilter) minFilter = NPDISP_D3DTFN_POINT;
	mipFilter = context->textureStageStates[stage][NPDISP_D3DTSS_MIPFILTER];
	if (!mipFilter) mipFilter = NPDISP_D3DTFP_NONE;
	texCoordState = context->textureStageStates[stage][NPDISP_D3DTSS_TEXCOORDINDEX];
	texCoordIndex = texCoordState & NPDISP_D3DTSS_TCI_INDEXMASK;
	textureTransformFlags = context->textureStageStates[stage][NPDISP_D3DTSS_TEXTURETRANSFORMFLAGS];
	if (texCoordIndex >= NPDISP_D3D_TEXTURE_COORD_SETS ||
		(texCoordState & NPDISP_D3DTSS_TCI_GENMASK) > 0x00030000UL ||
		(textureTransformFlags & ~(NPDISP_D3DTTFF_PROJECTED | 7U)) ||
		(textureTransformFlags && !(textureTransformFlags & 7U)) ||
		(textureTransformFlags && (textureTransformFlags & 7U) > 4U) ||
		((textureTransformFlags & NPDISP_D3DTTFF_PROJECTED) && (textureTransformFlags & 7U) < 2U) ||
		(texCoordState & ~(NPDISP_D3DTSS_TCI_INDEXMASK | NPDISP_D3DTSS_TCI_GENMASK)) ||
		(mipFilter != NPDISP_D3DTFP_NONE && mipFilter != NPDISP_D3DTFP_POINT && mipFilter != NPDISP_D3DTFP_LINEAR) ||
		(addressU != NPDISP_D3DTADDRESS_WRAP && addressU != NPDISP_D3DTADDRESS_MIRROR && addressU != NPDISP_D3DTADDRESS_CLAMP) ||
		(addressV != NPDISP_D3DTADDRESS_WRAP && addressV != NPDISP_D3DTADDRESS_MIRROR && addressV != NPDISP_D3DTADDRESS_CLAMP) ||
		(magFilter != NPDISP_D3DTFG_POINT && magFilter != NPDISP_D3DTFG_LINEAR) ||
		(minFilter != NPDISP_D3DTFN_POINT && minFilter != NPDISP_D3DTFN_LINEAR)) {
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=state stage=%u handle=%08x op=%u tc=%08x addr=%u/%u filter=%u/%u mip=%u tf=%08x",
			stage, handle, colorOp, texCoordState, addressU, addressV, minFilter, magFilter, mipFilter, textureTransformFlags));
		return false;
	}
	
	if (cubeMap && mipFilter != NPDISP_D3DTFP_NONE) {
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=cubemap-mip stage=%u handle=%08x mip=%u", stage, handle, mipFilter));
		return false;
	}
	if (colorOp == NPDISP_D3DTOP_BUMPENVMAP || colorOp == NPDISP_D3DTOP_BUMPENVMAPLUMINANCE) {
		if (mipFilter != NPDISP_D3DTFP_NONE || cubeMap || (snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_U8V8 && snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_U5V5L6)) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=bump-format stage=%u handle=%08x format=%u cube=%u", stage, handle, snapshot->format, cubeMap ? 1U : 0U));
			return false;
		}
	}
	else {
		if (colorOp != NPDISP_D3DTOP_MODULATE && colorOp != NPDISP_D3DTOP_SELECTARG1 &&
			colorOp != NPDISP_D3DTOP_SELECTARG2 && colorOp != NPDISP_D3DTOP_ADD &&
			colorOp != NPDISP_D3DTOP_BLENDTEXTUREALPHA) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=color-op stage=%u handle=%08x op=%u", stage, handle, colorOp));
			return false;
		}
		if (snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_RGB16 && snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_RGB32 &&
			snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_P8) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=color-format stage=%u handle=%08x op=%u format=%u", stage, handle, colorOp, snapshot->format));
			return false;
		}
		if ((colorArg1 & ~NPDISP_D3DTA_SELECTMASK) || (colorArg2 & ~NPDISP_D3DTA_SELECTMASK) ||
			((colorArg1 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE) ||
			((colorArg2 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE)) {
			TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=color-arg stage=%u handle=%08x arg=%08x/%08x", stage, handle, colorArg1, colorArg2));
			return false;
		}
	}
	if (alphaOp && alphaOp != NPDISP_D3DTOP_DISABLE &&
		alphaOp != NPDISP_D3DTOP_MODULATE && alphaOp != NPDISP_D3DTOP_SELECTARG1 && alphaOp != NPDISP_D3DTOP_SELECTARG2 &&
		alphaOp != NPDISP_D3DTOP_BLENDTEXTUREALPHA) {
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=alpha-op stage=%u handle=%08x op=%u", stage, handle, alphaOp));
		return false;
	}
	if (alphaOp && alphaOp != NPDISP_D3DTOP_DISABLE &&
		((alphaArg1 & ~NPDISP_D3DTA_SELECTMASK) || (alphaArg2 & ~NPDISP_D3DTA_SELECTMASK) ||
		 ((alphaArg1 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE) ||
		 ((alphaArg2 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE))) {
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=alpha-arg stage=%u handle=%08x arg=%08x/%08x", stage, handle, alphaArg1, alphaArg2));
		return false;
	}
	memset(mipSnapshots, 0, sizeof(mipSnapshots));
	mipSnapshots[0] = *snapshot;
	if (!cubeMap && mipFilter != NPDISP_D3DTFP_NONE &&
		!npdisp_d3d_captureMipSnapshots(textureSurface, snapshot, mipSnapshots, &mipCount)) {
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=mip-chain stage=%u handle=%08x", stage, handle));
		return false;
	}
	bytes = (UINT64)(UINT32)snapshot->pitch * snapshot->height;
	if (!bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES || bytes > 0x7fffffffUL) return false;
	memset(texture, 0, sizeof(*texture));
	if (cubeMap) {
		UINT64 totalBytes = bytes * NPDISP_D3D_CUBE_FACES;
		bool direct = directVideo;
		if (totalBytes > 0x7fffffffULL) return false;
		for (UINT32 face = 0; face < NPDISP_D3D_CUBE_FACES; ++face) {
			const NPDISP_D3D_TEXTURE_SNAPSHOT* faceSnapshot = &cubeSnapshots[face];
			UINT64 faceBytes = (UINT64)(UINT32)faceSnapshot->pitch * faceSnapshot->height;
			if (!faceSnapshot->valid || faceBytes != bytes || faceSnapshot->width != snapshot->width || faceSnapshot->height != snapshot->height ||
				faceSnapshot->pitch != snapshot->pitch || faceSnapshot->bpp != snapshot->bpp || faceSnapshot->format != snapshot->format ||
				faceSnapshot->rMask != snapshot->rMask || faceSnapshot->gMask != snapshot->gMask || faceSnapshot->bMask != snapshot->bMask || faceSnapshot->aMask != snapshot->aMask) return false;
			if (faceSnapshot->systemMemory) direct = false;
		}
		if (direct) {
			for (UINT32 face = 0; face < NPDISP_D3D_CUBE_FACES; ++face) {
				if (!npdisp_d3d_textureSnapshotPointer(&cubeSnapshots[face], &texture->cubePixels[face])) return false;
			}
		}
		else {
			storage->resize((size_t)totalBytes);
			for (UINT32 face = 0; face < NPDISP_D3D_CUBE_FACES; ++face) {
				UINT8* faceData = &(*storage)[0] + (size_t)bytes * face;
				if (!npdisp_d3d_readTextureSnapshot(&cubeSnapshots[face], faceData, (UINT32)bytes)) return false;
				texture->cubePixels[face] = faceData;
			}
		}
		texture->pixels = texture->cubePixels[0];
		texture->cubeMap = 1;
	}
	else {
		UINT64 totalBytes = 0;
		bool direct = directVideo;
		for (UINT32 level = 0; level < mipCount; ++level) {
			UINT64 levelBytes = (UINT64)(UINT32)mipSnapshots[level].pitch * mipSnapshots[level].height;
			if (!mipSnapshots[level].valid || !levelBytes || levelBytes > NPDISP_D3D_MAX_TEXTURE_BYTES ||
				totalBytes + levelBytes > NPDISP_D3D_MAX_TEXTURE_BYTES) return false;
			totalBytes += levelBytes;
			if (mipSnapshots[level].systemMemory) direct = false;
		}
		if (direct) {
			for (UINT32 level = 0; level < mipCount; ++level) {
				if (!npdisp_d3d_textureSnapshotPointer(&mipSnapshots[level], &texture->mipPixels[level])) return false;
			}
		}
		else {
			UINT32 offset = 0;
			storage->resize((size_t)totalBytes);
			for (UINT32 level = 0; level < mipCount; ++level) {
				UINT32 levelBytes = (UINT32)mipSnapshots[level].pitch * mipSnapshots[level].height;
				UINT8* levelData = &(*storage)[0] + offset;
				if (!npdisp_d3d_readTextureSnapshot(&mipSnapshots[level], levelData, levelBytes)) return false;
				texture->mipPixels[level] = levelData;
				offset += levelBytes;
			}
		}
		texture->pixels = texture->mipPixels[0];
	}
	texture->mipCount = cubeMap ? 1U : mipCount;
	texture->mipFilter = mipFilter;
	for (UINT32 level = 0; level < texture->mipCount; ++level) {
		const NPDISP_D3D_TEXTURE_SNAPSHOT* levelSnapshot = cubeMap ? snapshot : &mipSnapshots[level];
		if (!texture->mipPixels[level]) texture->mipPixels[level] = (level == 0U) ? texture->pixels : NULL;
		texture->mipWidth[level] = levelSnapshot->width;
		texture->mipHeight[level] = levelSnapshot->height;
		texture->mipPitch[level] = levelSnapshot->pitch;
	}
	texture->width = snapshot->width;
	texture->height = snapshot->height;
	texture->pitch = snapshot->pitch;
	texture->bpp = snapshot->bpp;
	texture->rMask = snapshot->rMask;
	texture->gMask = snapshot->gMask;
	texture->bMask = snapshot->bMask;
	texture->aMask = snapshot->aMask;
	texture->duMask = snapshot->duMask;
	texture->dvMask = snapshot->dvMask;
	texture->luminanceMask = snapshot->luminanceMask;
	texture->format = snapshot->format;
	texture->texCoordIndex = texCoordIndex;
	texture->textureTransformFlags = textureTransformFlags;
	texture->colorOp = colorOp;
	texture->colorArg1 = colorArg1;
	texture->colorArg2 = colorArg2;
	texture->alphaOp = alphaOp;
	texture->alphaArg1 = alphaArg1;
	texture->alphaArg2 = alphaArg2;
	texture->addressU = addressU;
	texture->addressV = addressV;
	texture->magFilter = magFilter;
	texture->minFilter = minFilter;
	texture->colorKeyEnable = colorKeyEnable ? 1U : 0U;
	texture->colorKeyMask = snapshot->format == NPDISP_D3D_TEXTURE_FORMAT_P8 ? 0xffU : (snapshot->rMask | snapshot->gMask | snapshot->bMask);
	texture->colorKeyLow = textureColorKey.dwColorSpaceLowValue & texture->colorKeyMask;
	texture->colorKeyHigh = textureColorKey.dwColorSpaceHighValue & texture->colorKeyMask;
	if (palette) {
		texture->paletteAlpha = (palette->flags & NPDISP_DDRAWIPAL_ALPHA) ? 1U : 0U;
		for (UINT32 i = 0; i < 256U; ++i) {
			UINT32 entry = palette->entries[i];
			UINT32 a = texture->paletteAlpha ? ((entry >> 24) & 0xffU) : 0xffU;
			UINT32 r = entry & 0xffU;
			UINT32 g = (entry >> 8) & 0xffU;
			UINT32 bl = (entry >> 16) & 0xffU;
			texture->palette[i] = (a << 24) | (r << 16) | (g << 8) | bl;
		}
	}
	texture->wrap = context->renderStates[NPDISP_D3DRENDERSTATE_WRAP0 + texCoordIndex];
	texture->bumpMat00 = npdisp_d3d_stateFloat(context->textureStageStates[stage][NPDISP_D3DTSS_BUMPENVMAT00]);
	texture->bumpMat01 = npdisp_d3d_stateFloat(context->textureStageStates[stage][NPDISP_D3DTSS_BUMPENVMAT01]);
	texture->bumpMat10 = npdisp_d3d_stateFloat(context->textureStageStates[stage][NPDISP_D3DTSS_BUMPENVMAT10]);
	texture->bumpMat11 = npdisp_d3d_stateFloat(context->textureStageStates[stage][NPDISP_D3DTSS_BUMPENVMAT11]);
	texture->bumpLScale = npdisp_d3d_stateFloat(context->textureStageStates[stage][NPDISP_D3DTSS_BUMPENVLSCALE]);
	texture->bumpLOffset = npdisp_d3d_stateFloat(context->textureStageStates[stage][NPDISP_D3DTSS_BUMPENVLOFFSET]);
	state->textures[stage] = texture;
	TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_BIND stage=%u ns=%08x handle=%08x size=%ux%u pitch=%d fp=%08x format=%u op=%u tc=%u addr=%u/%u filter=%u/%u mip=%u levels=%u cube=%u ckey=%u/%08x-%08x",
		stage, context->ddLocalNamespace, handle, texture->width, texture->height, texture->pitch,
		snapshot->linearBase, texture->format, colorOp, texCoordIndex, addressU, addressV, minFilter, magFilter, mipFilter, texture->mipCount, cubeMap ? 1U : 0U,
		texture->colorKeyEnable, texture->colorKeyLow, texture->colorKeyHigh));
	return true;
}

static bool npdisp_d3d_prepareTextures(NPDISP_D3D_CONTEXT* context, NPDISP_D3D_RASTERSTATE* state, std::vector<UINT8>* storage, NPDISP_D3D_TEXTURE* textures, bool directVideo, UINT32* boundSurfaces)
{
	if (!context || !state || !storage || !textures) return false;
	if (boundSurfaces) memset(boundSurfaces, 0, sizeof(UINT32) * NPDISP_D3D_RASTER_TEXTURE_STAGES);
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		UINT32 colorOp = context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP];
		if (stage && (!colorOp || colorOp == NPDISP_D3DTOP_DISABLE ||
			!context->textureStageStates[stage][NPDISP_D3DTSS_TEXTUREMAP])) break;
		if (!npdisp_d3d_prepareTextureStage(context, stage, state, &storage[stage], &textures[stage], directVideo, boundSurfaces ? &boundSurfaces[stage] : NULL)) return false;
		if (colorOp == NPDISP_D3DTOP_DISABLE) break;
	}
	return true;
}

static UINT32 npdisp_d3d_contextHandle(const NPDISP_D3D_CONTEXT* context)
{
	std::map<UINT32, NPDISP_D3D_CONTEXT>::const_iterator it;
	if (!context) return 0;
	for (it = npdisp_d3d_contexts.begin(); it != npdisp_d3d_contexts.end(); ++it) {
		if (&it->second == context) return it->first;
	}
	return 0;
}

static void npdisp_d3d_matrixMultiply(const NPDISP_D3DMATRIX32* a, const NPDISP_D3DMATRIX32* b, NPDISP_D3DMATRIX32* result)
{
	NPDISP_D3DMATRIX32 value;
	for (UINT32 i = 0; i < 4U; ++i) {
		for (UINT32 j = 0; j < 4U; ++j) {
			value.m[i][j] = 0.0f;
			for (UINT32 k = 0; k < 4U; ++k) value.m[i][j] += a->m[i][k] * b->m[k][j];
		}
	}
	*result = value;
}

static bool npdisp_d3d_matrixInverse(const NPDISP_D3DMATRIX32* matrix, NPDISP_D3DMATRIX32* inverse)
{
	float work[4][8];
	if (!matrix || !inverse) return false;
	for (UINT32 row = 0; row < 4U; ++row) {
		for (UINT32 col = 0; col < 4U; ++col) work[row][col] = matrix->m[row][col];
		for (UINT32 col = 0; col < 4U; ++col) work[row][4U + col] = row == col ? 1.0f : 0.0f;
	}
	for (UINT32 col = 0; col < 4U; ++col) {
		UINT32 pivot = col;
		float pivotValue = (float)fabs(work[pivot][col]);
		for (UINT32 row = col + 1U; row < 4U; ++row) {
			float value = (float)fabs(work[row][col]);
			if (value > pivotValue) { pivot = row; pivotValue = value; }
		}
		if (pivotValue <= 0.0000001f) return false;
		if (pivot != col) {
			for (UINT32 i = 0; i < 8U; ++i) {
				float value = work[col][i];
				work[col][i] = work[pivot][i];
				work[pivot][i] = value;
			}
		}
		{
			float scale = 1.0f / work[col][col];
			for (UINT32 i = 0; i < 8U; ++i) work[col][i] *= scale;
		}
		for (UINT32 row = 0; row < 4U; ++row) {
			float scale;
			if (row == col) continue;
			scale = work[row][col];
			if (scale == 0.0f) continue;
			for (UINT32 i = 0; i < 8U; ++i) work[row][i] -= scale * work[col][i];
		}
	}
	for (UINT32 row = 0; row < 4U; ++row) for (UINT32 col = 0; col < 4U; ++col) inverse->m[row][col] = work[row][4U + col];
	return true;
}

static UINT32 npdisp_d3d_clipLerpColor(UINT32 first, UINT32 second, float t)
{
	UINT32 result = 0;
	for (UINT32 shift = 0; shift <= 24U; shift += 8U) {
		float a = (float)((first >> shift) & 0xffU);
		float b = (float)((second >> shift) & 0xffU);
		SINT32 value = (SINT32)(a + (b - a) * t + 0.5f);
		if (value < 0) value = 0;
		if (value > 255) value = 255;
		result |= ((UINT32)value << shift);
	}
	return result;
}

static bool npdisp_d3d_clipVertexFromTL(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3DTLVERTEX32* vertex, NPDISP_D3D_CLIP_VERTEX* result)
{
	float w;
	float halfWidth;
	float halfHeight;
	float zScale;
	if (!context || !vertex || !result) return false;
	result->vertex = *vertex;
	if (vertex->clipValid) {
		result->clip[0] = vertex->clipX;
		result->clip[1] = vertex->clipY;
		result->clip[2] = vertex->clipZ;
		result->clip[3] = vertex->clipW;
		return true;
	}
	if (vertex->rhw <= 0.000001f || !context->viewportWidth || !context->viewportHeight) return false;
	w = 1.0f / vertex->rhw;
	halfWidth = (float)context->viewportWidth * 0.5f;
	halfHeight = (float)context->viewportHeight * 0.5f;
	zScale = context->zMax - context->zMin;
	if (halfWidth <= 0.0f || halfHeight <= 0.0f || (float)fabs(zScale) <= 0.000001f) return false;
	result->clip[3] = w;
	result->clip[0] = (((vertex->sx - (float)context->viewportX) / halfWidth) - 1.0f) * w;
	result->clip[1] = (1.0f - ((vertex->sy - (float)context->viewportY) / halfHeight)) * w;
	result->clip[2] = ((vertex->sz - context->zMin) / zScale) * w;
	return true;
}

static void npdisp_d3d_clipVertexProject(const NPDISP_D3D_CONTEXT* context, NPDISP_D3D_CLIP_VERTEX* vertex)
{
	float rhw;
	if (!context || !vertex || vertex->clip[3] <= 0.000001f) {
		if (vertex) vertex->vertex.rhw = 0.0f;
		return;
	}
	rhw = 1.0f / vertex->clip[3];
	vertex->vertex.clipX = vertex->clip[0];
	vertex->vertex.clipY = vertex->clip[1];
	vertex->vertex.clipZ = vertex->clip[2];
	vertex->vertex.clipW = vertex->clip[3];
	vertex->vertex.clipValid = 1U;
	vertex->vertex.rhw = rhw;
	vertex->vertex.sx = (float)context->viewportX + (1.0f + vertex->clip[0] * rhw) * (float)context->viewportWidth * 0.5f;
	vertex->vertex.sy = (float)context->viewportY + (1.0f - vertex->clip[1] * rhw) * (float)context->viewportHeight * 0.5f;
	vertex->vertex.sz = context->zMin + vertex->clip[2] * rhw * (context->zMax - context->zMin);
}

static NPDISP_D3D_CLIP_VERTEX npdisp_d3d_clipVertexLerp(const NPDISP_D3D_CLIP_VERTEX* first, const NPDISP_D3D_CLIP_VERTEX* second, float t)
{
	NPDISP_D3D_CLIP_VERTEX result;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	for (UINT32 i = 0; i < 4U; ++i) result.clip[i] = first->clip[i] + (second->clip[i] - first->clip[i]) * t;
	result.vertex = first->vertex;
	result.vertex.color = npdisp_d3d_clipLerpColor(first->vertex.color, second->vertex.color, t);
	result.vertex.specular = npdisp_d3d_clipLerpColor(first->vertex.specular, second->vertex.specular, t);
	result.vertex.tu = first->vertex.tu + (second->vertex.tu - first->vertex.tu) * t;
	result.vertex.tv = first->vertex.tv + (second->vertex.tv - first->vertex.tv) * t;
	result.vertex.tw = first->vertex.tw + (second->vertex.tw - first->vertex.tw) * t;
	result.vertex.tu2 = first->vertex.tu2 + (second->vertex.tu2 - first->vertex.tu2) * t;
	result.vertex.tv2 = first->vertex.tv2 + (second->vertex.tv2 - first->vertex.tv2) * t;
	result.vertex.tw2 = first->vertex.tw2 + (second->vertex.tw2 - first->vertex.tw2) * t;
	result.vertex.tq = first->vertex.tq + (second->vertex.tq - first->vertex.tq) * t;
	result.vertex.tq2 = first->vertex.tq2 + (second->vertex.tq2 - first->vertex.tq2) * t;
	for (UINT32 coord = 0; coord < NPDISP_D3D_TEXTURE_COORD_SETS; ++coord)
		for (UINT32 component = 0; component < 4U; ++component)
			result.vertex.sourceTex[coord][component] = first->vertex.sourceTex[coord][component] +
				(second->vertex.sourceTex[coord][component] - first->vertex.sourceTex[coord][component]) * t;
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage)
		for (UINT32 component = 0; component < 4U; ++component)
			result.vertex.stageTex[stage][component] = first->vertex.stageTex[stage][component] +
				(second->vertex.stageTex[stage][component] - first->vertex.stageTex[stage][component]) * t;
	return result;
}

static float npdisp_d3d_clipDistance(const NPDISP_D3D_CLIP_VERTEX* vertex, const float* plane)
{
	return vertex->clip[0] * plane[0] + vertex->clip[1] * plane[1] + vertex->clip[2] * plane[2] + vertex->clip[3] * plane[3];
}

static bool npdisp_d3d_clipPlanes(const NPDISP_D3D_CONTEXT* context, float planes[NPDISP_D3D_MAX_USER_CLIP_PLANES][4], UINT32* mask)
{
	UINT32 handle;
	UINT32 enabled;
	NPDISP_D3D_TNL_CONTEXT* tnl;
	std::map<UINT32, NPDISP_D3D_CLIP_CONTEXT>::const_iterator clipIt;
	NPDISP_D3DMATRIX32 viewProjection;
	NPDISP_D3DMATRIX32 inverse;
	if (!context || !planes || !mask) return false;
	*mask = 0;
	enabled = context->renderStates[NPDISP_D3DRENDERSTATE_CLIPPLANEENABLE] & ((1U << NPDISP_D3D_MAX_USER_CLIP_PLANES) - 1U);
	if (!enabled) return true;
	handle = npdisp_d3d_contextHandle(context);
	if (!handle || !(tnl = npdisp_d3d_getTnlContext(handle))) return false;
	clipIt = npdisp_d3d_clipContexts.find(handle);
	if (clipIt == npdisp_d3d_clipContexts.end()) return true;
	npdisp_d3d_matrixMultiply(&tnl->view, &tnl->projection, &viewProjection);
	if (!npdisp_d3d_matrixInverse(&viewProjection, &inverse)) return false;
	for (UINT32 i = 0; i < NPDISP_D3D_MAX_USER_CLIP_PLANES; ++i) {
		const NPDISP_D3D_CLIP_PLANE* source;
		if (!(enabled & (1U << i))) continue;
		source = &clipIt->second.planes[i];
		if (!source->valid) continue;
		for (UINT32 row = 0; row < 4U; ++row) {
			planes[i][row] = inverse.m[row][0] * source->plane[0] + inverse.m[row][1] * source->plane[1] +
				inverse.m[row][2] * source->plane[2] + inverse.m[row][3] * source->plane[3];
		}
		*mask |= 1U << i;
	}
	return true;
}

static bool npdisp_d3d_clipLine(NPDISP_D3D_CLIP_VERTEX* first, NPDISP_D3D_CLIP_VERTEX* second,
	const float (*planes)[4], UINT32 planeCount, UINT32 mask)
{
	for (UINT32 plane = 0; plane < planeCount; ++plane) {
		float d0, d1;
		if (!(mask & (1U << plane))) continue;
		d0 = npdisp_d3d_clipDistance(first, planes[plane]);
		d1 = npdisp_d3d_clipDistance(second, planes[plane]);
		if (d0 < 0.0f && d1 < 0.0f) return false;
		if ((d0 < 0.0f) != (d1 < 0.0f)) {
			float denominator = d0 - d1;
			float t = (float)fabs(denominator) > 0.0000001f ? d0 / denominator : 0.0f;
			NPDISP_D3D_CLIP_VERTEX intersection = npdisp_d3d_clipVertexLerp(first, second, t);
			if (d0 < 0.0f) *first = intersection;
			else *second = intersection;
		}
	}
	return true;
}

static bool npdisp_d3d_clipTriangle(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_CLIP_VERTEX input[3],
	const float (*planes)[4], UINT32 planeCount, UINT32 mask, std::vector<NPDISP_D3DTLVERTEX32>* output)
{
	std::vector<NPDISP_D3D_CLIP_VERTEX> polygon;
	std::vector<NPDISP_D3D_CLIP_VERTEX> clipped;
	if (!context || !input || !output) return false;
	polygon.assign(input, input + 3);
	for (UINT32 plane = 0; plane < planeCount && !polygon.empty(); ++plane) {
		if (!(mask & (1U << plane))) continue;
		clipped.clear();
		NPDISP_D3D_CLIP_VERTEX previous = polygon.back();
		float previousDistance = npdisp_d3d_clipDistance(&previous, planes[plane]);
		for (UINT32 i = 0; i < (UINT32)polygon.size(); ++i) {
			const NPDISP_D3D_CLIP_VERTEX current = polygon[i];
			float currentDistance = npdisp_d3d_clipDistance(&current, planes[plane]);
			bool previousInside = previousDistance >= 0.0f;
			bool currentInside = currentDistance >= 0.0f;
			if (previousInside != currentInside) {
				float denominator = previousDistance - currentDistance;
				float t = (float)fabs(denominator) > 0.0000001f ? previousDistance / denominator : 0.0f;
				clipped.push_back(npdisp_d3d_clipVertexLerp(&previous, &current, t));
			}
			if (currentInside) clipped.push_back(current);
			previous = current;
			previousDistance = currentDistance;
		}
		polygon.swap(clipped);
	}
	if (polygon.size() < 3U) return true;
	for (UINT32 i = 0; i < (UINT32)polygon.size(); ++i) npdisp_d3d_clipVertexProject(context, &polygon[i]);
	for (UINT32 i = 1; i + 1U < (UINT32)polygon.size(); ++i) {
		if (output->size() > NPDISP_D3D_MAX_VERTICES - 3U) return false;
		output->push_back(polygon[0].vertex);
		output->push_back(polygon[i].vertex);
		output->push_back(polygon[i + 1U].vertex);
	}
	return true;
}

static bool npdisp_d3d_clipPrimitivePlanes(NPDISP_D3D_CONTEXT* context, UINT32 primitiveType, const NPDISP_D3DTLVERTEX32* vertices,
	UINT32 vertexCount, const UINT16* indices, UINT32 indexCount, const float (*planes)[4], UINT32 planeCount, UINT32 mask,
	UINT32* clippedType, std::vector<NPDISP_D3DTLVERTEX32>* clippedVertices)
{
	UINT32 count = indices ? indexCount : vertexCount;
	if (!context || !vertices || !planes || !planeCount || !clippedType || !clippedVertices || count > NPDISP_D3D_MAX_VERTICES) return false;
	clippedVertices->clear();
	if (primitiveType == NPDISP_D3DPT_POINTLIST) {
		*clippedType = NPDISP_D3DPT_POINTLIST;
		for (UINT32 i = 0; i < count; ++i) {
			UINT32 index = indices ? indices[i] : i;
			NPDISP_D3D_CLIP_VERTEX vertex;
			bool visible = true;
			if (index >= vertexCount) return false;
			if (!npdisp_d3d_clipVertexFromTL(context, &vertices[index], &vertex)) continue;
			for (UINT32 plane = 0; plane < planeCount; ++plane) if ((mask & (1U << plane)) && npdisp_d3d_clipDistance(&vertex, planes[plane]) < 0.0f) { visible = false; break; }
			if (visible) {
				npdisp_d3d_clipVertexProject(context, &vertex);
				clippedVertices->push_back(vertex.vertex);
			}
		}
	}
	else if (primitiveType == NPDISP_D3DPT_LINELIST || primitiveType == NPDISP_D3DPT_LINESTRIP) {
		UINT32 lineCount;
		*clippedType = NPDISP_D3DPT_LINELIST;
		if (primitiveType == NPDISP_D3DPT_LINELIST && (count & 1U)) return false;
		lineCount = primitiveType == NPDISP_D3DPT_LINELIST ? count / 2U : (count >= 2U ? count - 1U : 0U);
		for (UINT32 i = 0; i < lineCount; ++i) {
			UINT32 p0 = primitiveType == NPDISP_D3DPT_LINELIST ? i * 2U : i;
			UINT32 p1 = p0 + 1U;
			UINT32 i0 = indices ? indices[p0] : p0;
			UINT32 i1 = indices ? indices[p1] : p1;
			NPDISP_D3D_CLIP_VERTEX v0, v1;
			if (i0 >= vertexCount || i1 >= vertexCount) return false;
			if (!npdisp_d3d_clipVertexFromTL(context, &vertices[i0], &v0) || !npdisp_d3d_clipVertexFromTL(context, &vertices[i1], &v1)) continue;
			if (npdisp_d3d_clipLine(&v0, &v1, planes, planeCount, mask)) {
				if (clippedVertices->size() > NPDISP_D3D_MAX_VERTICES - 2U) return false;
				npdisp_d3d_clipVertexProject(context, &v0);
				npdisp_d3d_clipVertexProject(context, &v1);
				clippedVertices->push_back(v0.vertex);
				clippedVertices->push_back(v1.vertex);
			}
		}
	}
	else if (primitiveType == NPDISP_D3DPT_TRIANGLELIST || primitiveType == NPDISP_D3DPT_TRIANGLESTRIP || primitiveType == NPDISP_D3DPT_TRIANGLEFAN) {
		UINT32 triangleCount;
		*clippedType = NPDISP_D3DPT_TRIANGLELIST;
		if (primitiveType == NPDISP_D3DPT_TRIANGLELIST && count % 3U) return false;
		triangleCount = primitiveType == NPDISP_D3DPT_TRIANGLELIST ? count / 3U : (count >= 3U ? count - 2U : 0U);
		for (UINT32 i = 0; i < triangleCount; ++i) {
			UINT32 p0, p1, p2;
			UINT32 i0, i1, i2;
			NPDISP_D3D_CLIP_VERTEX triangle[3];
			if (primitiveType == NPDISP_D3DPT_TRIANGLELIST) { p0 = i * 3U; p1 = p0 + 1U; p2 = p0 + 2U; }
			else if (primitiveType == NPDISP_D3DPT_TRIANGLESTRIP) { p0 = i; p1 = i + 1U; p2 = i + 2U; if (i & 1U) { UINT32 t = p0; p0 = p1; p1 = t; } }
			else { p0 = 0; p1 = i + 1U; p2 = i + 2U; }
			i0 = indices ? indices[p0] : p0;
			i1 = indices ? indices[p1] : p1;
			i2 = indices ? indices[p2] : p2;
			if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) return false;
			if (!npdisp_d3d_clipVertexFromTL(context, &vertices[i0], &triangle[0]) ||
				!npdisp_d3d_clipVertexFromTL(context, &vertices[i1], &triangle[1]) ||
				!npdisp_d3d_clipVertexFromTL(context, &vertices[i2], &triangle[2])) continue;
			if (!npdisp_d3d_clipTriangle(context, triangle, planes, planeCount, mask, clippedVertices)) return false;
		}
	}
	else return false;
	return true;
}

static bool npdisp_d3d_clipDrawPrimitive(NPDISP_D3D_CONTEXT* context, UINT32 primitiveType, const NPDISP_D3DTLVERTEX32* vertices,
	UINT32 vertexCount, const UINT16* indices, UINT32 indexCount, UINT32* clippedType, std::vector<NPDISP_D3DTLVERTEX32>* clippedVertices)
{
	static const float frustum[NPDISP_D3D_FRUSTUM_PLANES][4] = {
		{ 1.0f,  0.0f,  0.0f, 1.0f },
		{-1.0f,  0.0f,  0.0f, 1.0f },
		{ 0.0f,  1.0f,  0.0f, 1.0f },
		{ 0.0f, -1.0f,  0.0f, 1.0f },
		{ 0.0f,  0.0f,  1.0f, 0.0f },
		{ 0.0f,  0.0f, -1.0f, 1.0f }
	};
	float planes[NPDISP_D3D_FRUSTUM_PLANES + NPDISP_D3D_MAX_USER_CLIP_PLANES][4] = { 0 };
	float userPlanes[NPDISP_D3D_MAX_USER_CLIP_PLANES][4] = { 0 };
	UINT32 mask = 0, userMask = 0;
	UINT32 count = indices ? indexCount : vertexCount;
	bool allClipValid = true, outside = false;
	if (!context || !vertices || !clippedType || !clippedVertices || count > NPDISP_D3D_MAX_VERTICES) return false;
	*clippedType = 0xffffffffUL;
	if (context->renderStates[NPDISP_D3DRENDERSTATE_CLIPPING]) {
		for (UINT32 i = 0; i < count; ++i) {
			UINT32 index = indices ? indices[i] : i;
			if (index >= vertexCount) return false;
			if (!vertices[index].clipValid) { allClipValid = false; break; }
		}
		if (allClipValid) {
			memcpy(planes, frustum, sizeof(frustum));
			mask = (1U << NPDISP_D3D_FRUSTUM_PLANES) - 1U;
		}
	}
	if (context->renderStates[NPDISP_D3DRENDERSTATE_CLIPPLANEENABLE] & ((1U << NPDISP_D3D_MAX_USER_CLIP_PLANES) - 1U)) {
		if (!npdisp_d3d_clipPlanes(context, userPlanes, &userMask)) return false;
		memcpy(planes + NPDISP_D3D_FRUSTUM_PLANES, userPlanes, sizeof(userPlanes));
		mask |= userMask << NPDISP_D3D_FRUSTUM_PLANES;
	}
	if (!mask) return true;
	for (UINT32 i = 0; i < count && !outside; ++i) {
		UINT32 index = indices ? indices[i] : i;
		NPDISP_D3D_CLIP_VERTEX vertex;
		if (index >= vertexCount) return false;
		if (!npdisp_d3d_clipVertexFromTL(context, &vertices[index], &vertex)) { outside = true; break; }
		for (UINT32 plane = 0; plane < NPDISP_D3D_FRUSTUM_PLANES + NPDISP_D3D_MAX_USER_CLIP_PLANES; ++plane) {
			if ((mask & (1U << plane)) && npdisp_d3d_clipDistance(&vertex, planes[plane]) < 0.0f) { outside = true; break; }
		}
	}
	if (!outside) return true;
	if (!npdisp_d3d_clipPrimitivePlanes(context, primitiveType, vertices, vertexCount, indices, indexCount, planes,
		NPDISP_D3D_FRUSTUM_PLANES + NPDISP_D3D_MAX_USER_CLIP_PLANES, mask, clippedType, clippedVertices)) return false;
	TRACEOUTD3D(("NPDISP11 D3D_CLIP type=%u outtype=%u mask=%08x in=%u out=%u", primitiveType, *clippedType, mask, count, (UINT32)clippedVertices->size()));
	return true;
}

static NPDISP_D3D_VERTEX npdisp_d3d_vertex(const NPDISP_D3DTLVERTEX32* vertex)
{
	NPDISP_D3D_VERTEX v;
	v.x = vertex->sx;
	v.y = vertex->sy;
	v.z = vertex->sz;
	v.rhw = vertex->rhw;
	v.diffuse = vertex->color;
	v.specular = vertex->specular;
	v.tu = vertex->tu;
	v.tv = vertex->tv;
	v.tu2 = vertex->tu2;
	v.tv2 = vertex->tv2;
	v.tw = vertex->tw;
	v.tw2 = vertex->tw2;
	memcpy(v.stageTex, vertex->stageTex, sizeof(v.stageTex));
	v.stageTexValid = vertex->stageTexValid;
	return v;
}

static bool npdisp_d3d_point(NPDISP_D3D_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, UINT32 index, const NPDISP_D3D_RASTERSTATE* state)
{
	NPDISP_D3D_VERTEX vertex;
	if (!target || !vertices || !state || index >= vertexCount) return false;
	if (vertices[index].rhw <= 0.0f) return true;
	vertex = npdisp_d3d_vertex(&vertices[index]);
	return npdisp_d3d_sw_point(&target->sw, depthTarget, &vertex, state);
}

static bool npdisp_d3d_line(NPDISP_D3D_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, UINT32 i0, UINT32 i1, const NPDISP_D3D_RASTERSTATE* state)
{
	NPDISP_D3D_VERTEX v0;
	NPDISP_D3D_VERTEX v1;
	if (!target || !vertices || !state || i0 >= vertexCount || i1 >= vertexCount) return false;
	if (vertices[i0].rhw <= 0.0f || vertices[i1].rhw <= 0.0f) return true;
	v0 = npdisp_d3d_vertex(&vertices[i0]);
	v1 = npdisp_d3d_vertex(&vertices[i1]);
	return npdisp_d3d_sw_line(&target->sw, depthTarget, &v0, &v1, state);
}

static void npdisp_d3d_profileTriangle(const NPDISP_D3D_TARGET* target, const NPDISP_D3D_VERTEX* v0, const NPDISP_D3D_VERTEX* v1, const NPDISP_D3D_VERTEX* v2, const NPDISP_D3D_RASTERSTATE* state)
{
	float minxf;
	float minyf;
	float maxxf;
	float maxyf;
	SINT32 left;
	SINT32 top;
	SINT32 right;
	SINT32 bottom;
	LONG pixels;
	LONG oldMax;
	bool textured = false;
	bool mip = false;

	if (!target || !v0 || !v1 || !v2 || !state || !target->sw.width || !target->sw.height) return;
	if (state->fillMode == NPDISP_D3DFILL_WIREFRAME) return;

	minxf = v0->x; if (v1->x < minxf) minxf = v1->x; if (v2->x < minxf) minxf = v2->x;
	maxxf = v0->x; if (v1->x > maxxf) maxxf = v1->x; if (v2->x > maxxf) maxxf = v2->x;
	minyf = v0->y; if (v1->y < minyf) minyf = v1->y; if (v2->y < minyf) minyf = v2->y;
	maxyf = v0->y; if (v1->y > maxyf) maxyf = v1->y; if (v2->y > maxyf) maxyf = v2->y;
	if (maxxf < 0.0f || maxyf < 0.0f || minxf >= (float)target->sw.width || minyf >= (float)target->sw.height) return;

	left = (minxf <= 0.0f) ? 0 : (SINT32)minxf;
	top = (minyf <= 0.0f) ? 0 : (SINT32)minyf;
	right = (maxxf >= (float)target->sw.width) ? (SINT32)target->sw.width : (SINT32)maxxf + 1;
	bottom = (maxyf >= (float)target->sw.height) ? (SINT32)target->sw.height : (SINT32)maxyf + 1;
	if (right <= left || bottom <= top) return;
	pixels = (LONG)((right - left) * (bottom - top));

	InterlockedExchangeAdd(&npdisp_d3d_profileBoxKPixels, (pixels + 1023) >> 10);
	oldMax = npdisp_d3d_profileBoxMaxPixels;
	while (pixels > oldMax) {
		LONG previous = InterlockedCompareExchange(&npdisp_d3d_profileBoxMaxPixels, pixels, oldMax);
		if (previous == oldMax) break;
		oldMax = previous;
	}
	if (pixels >= 4096) InterlockedIncrement(&npdisp_d3d_profileBox4K);
	if (pixels >= 16384) InterlockedIncrement(&npdisp_d3d_profileBox16K);
	if (pixels >= 65536) InterlockedIncrement(&npdisp_d3d_profileBox64K);

	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		const NPDISP_D3D_TEXTURE* texture = state->textures[stage];
		if (!texture || texture->colorOp == NPDISP_D3DTOP_DISABLE) break;
		textured = true;
		if (!stage) {
			if (texture->magFilter == NPDISP_D3DTFG_POINT && texture->minFilter == NPDISP_D3DTFN_POINT)
				InterlockedIncrement(&npdisp_d3d_profileFilterPP);
			else if (texture->magFilter == NPDISP_D3DTFG_POINT && texture->minFilter == NPDISP_D3DTFN_LINEAR)
				InterlockedIncrement(&npdisp_d3d_profileFilterPL);
			else if (texture->magFilter == NPDISP_D3DTFG_LINEAR && texture->minFilter == NPDISP_D3DTFN_POINT)
				InterlockedIncrement(&npdisp_d3d_profileFilterLP);
			else if (texture->magFilter == NPDISP_D3DTFG_LINEAR && texture->minFilter == NPDISP_D3DTFN_LINEAR)
				InterlockedIncrement(&npdisp_d3d_profileFilterLL);
			if (texture->bpp == 16U) InterlockedIncrement(&npdisp_d3d_profileTex16Triangles);
			else if (texture->bpp == 32U) InterlockedIncrement(&npdisp_d3d_profileTex32Triangles);
		}
		if (texture->mipCount > 1U && texture->mipFilter != NPDISP_D3DTFP_NONE) mip = true;
	}
	if (textured) InterlockedIncrement(&npdisp_d3d_profileTextureTriangles);
	if (state->zEnable) InterlockedIncrement(&npdisp_d3d_profileZTriangles);
	if (state->alphaBlendEnable) InterlockedIncrement(&npdisp_d3d_profileBlendTriangles);
	if (state->alphaTestEnable) InterlockedIncrement(&npdisp_d3d_profileAlphaTestTriangles);
	if (state->fogEnable) InterlockedIncrement(&npdisp_d3d_profileFogTriangles);
	if (mip) InterlockedIncrement(&npdisp_d3d_profileMipTriangles);
}

static bool npdisp_d3d_triangle(NPDISP_D3D_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, UINT32 i0, UINT32 i1, UINT32 i2, const NPDISP_D3D_RASTERSTATE* state)
{
	NPDISP_D3D_VERTEX v0;
	NPDISP_D3D_VERTEX v1;
	NPDISP_D3D_VERTEX v2;
	if (!target || !vertices || !state || i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) return false;
	if (vertices[i0].rhw <= 0.0f || vertices[i1].rhw <= 0.0f || vertices[i2].rhw <= 0.0f) return true;
	v0 = npdisp_d3d_vertex(&vertices[i0]);
	v1 = npdisp_d3d_vertex(&vertices[i1]);
	v2 = npdisp_d3d_vertex(&vertices[i2]);
	npdisp_d3d_profileTriangle(target, &v0, &v1, &v2, state);
	return npdisp_d3d_sw_triangle(&target->sw, depthTarget, &v0, &v1, &v2, state);
}

static bool npdisp_d3d_rasterize(NPDISP_D3D_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depth, UINT32 primitiveType, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, const UINT16* indices, UINT32 indexCount, const NPDISP_D3D_RASTERSTATE* state)
{
	UINT32 count = indices ? indexCount : vertexCount;
	UINT32 triangles = 0;
	if (!target || !vertices || !state || count > NPDISP_D3D_MAX_VERTICES) return false;
	if (primitiveType == NPDISP_D3DPT_TRIANGLELIST) triangles = count / 3U;
	else if ((primitiveType == NPDISP_D3DPT_TRIANGLESTRIP || primitiveType == NPDISP_D3DPT_TRIANGLEFAN) && count >= 3U) triangles = count - 2U;
	InterlockedIncrement(&npdisp_d3d_profileRasterCalls);
	if (triangles) InterlockedExchangeAdd(&npdisp_d3d_profileTriangles, (LONG)triangles);
	if (primitiveType == NPDISP_D3DPT_POINTLIST) {
		for (UINT32 i = 0; i < count; ++i) {
			UINT32 index = indices ? indices[i] : i;
			if (!npdisp_d3d_point(target, depth, vertices, vertexCount, index, state)) return false;
		}
	}
	else if (primitiveType == NPDISP_D3DPT_LINELIST) {
		if (count % 2) return false;
		for (UINT32 i = 0; i < count; i += 2) {
			UINT32 i0 = indices ? indices[i] : i;
			UINT32 i1 = indices ? indices[i + 1] : i + 1;
			if (!npdisp_d3d_line(target, depth, vertices, vertexCount, i0, i1, state)) return false;
		}
	}
	else if (primitiveType == NPDISP_D3DPT_LINESTRIP) {
		if (count < 2) return count == 0;
		for (UINT32 i = 0; i + 1 < count; ++i) {
			UINT32 i0 = indices ? indices[i] : i;
			UINT32 i1 = indices ? indices[i + 1] : i + 1;
			if (!npdisp_d3d_line(target, depth, vertices, vertexCount, i0, i1, state)) return false;
		}
	}
	else if (primitiveType == NPDISP_D3DPT_TRIANGLELIST) {
		if (count % 3) return false;
		for (UINT32 i = 0; i < count; i += 3) {
			UINT32 i0 = indices ? indices[i] : i;
			UINT32 i1 = indices ? indices[i + 1] : i + 1;
			UINT32 i2 = indices ? indices[i + 2] : i + 2;
			if (!npdisp_d3d_triangle(target, depth, vertices, vertexCount, i0, i1, i2, state)) return false;
		}
	}
	else if (primitiveType == NPDISP_D3DPT_TRIANGLESTRIP) {
		if (count < 3) return count == 0;
		for (UINT32 i = 0; i + 2 < count; ++i) {
			UINT32 a = indices ? indices[i] : i;
			UINT32 b = indices ? indices[i + 1] : i + 1;
			UINT32 c = indices ? indices[i + 2] : i + 2;
			if (i & 1) { UINT32 t = a; a = b; b = t; }
			if (!npdisp_d3d_triangle(target, depth, vertices, vertexCount, a, b, c, state)) return false;
		}
	}
	else if (primitiveType == NPDISP_D3DPT_TRIANGLEFAN) {
		if (count < 3) return count == 0;
		UINT32 first = indices ? indices[0] : 0;
		for (UINT32 i = 1; i + 1 < count; ++i) {
			UINT32 b = indices ? indices[i] : i;
			UINT32 c = indices ? indices[i + 1] : i + 1;
			if (!npdisp_d3d_triangle(target, depth, vertices, vertexCount, first, b, c, state)) return false;
		}
	}
	else return false;
	return true;
}

static bool npdisp_d3d_asyncExecute(NPDISP_D3D_ASYNC_COMMAND* command)
{
	if (!command) return false;
	if (command->type == NPDISP_D3D_ASYNC_DRAW) {
		NPDISP_D3D_SW_DEPTH_TARGET* depth = command->hasDepth ? &command->depth : NULL;
		const UINT16* indices = command->indices.empty() ? NULL : &command->indices[0];
		if (command->vertices.empty()) return false;
		return npdisp_d3d_rasterize(&command->target, depth, command->primitiveType, &command->vertices[0], (UINT32)command->vertices.size(), indices, (UINT32)command->indices.size(), &command->state);
	}
	if (command->type == NPDISP_D3D_ASYNC_CLEAR) {
		if (command->clearFlags & NPDISP_D3DCLEAR_TARGET) {
			if (command->rects.empty()) {
				if (!npdisp_d3d_sw_clear(&command->target.sw, 0, 0, (SINT32)command->target.sw.width, (SINT32)command->target.sw.height, command->clearColor)) return false;
			}
			else for (UINT32 i = 0; i < command->rects.size(); ++i) if (!npdisp_d3d_sw_clear(&command->target.sw, command->rects[i].left, command->rects[i].top, command->rects[i].right, command->rects[i].bottom, command->clearColor)) return false;
		}
		if (command->clearFlags & (NPDISP_D3DCLEAR_ZBUFFER | NPDISP_D3DCLEAR_STENCIL)) {
			if (!command->hasDepth) return false;
			if (command->rects.empty()) {
				if (!npdisp_d3d_sw_clearDepthStencil(&command->depth, 0, 0, (SINT32)command->depth.width, (SINT32)command->depth.height,
					command->clearDepth, command->clearStencil, command->clearFlags, command->state.depthMask, command->state.stencilBits)) return false;
			}
			else for (UINT32 i = 0; i < command->rects.size(); ++i) if (!npdisp_d3d_sw_clearDepthStencil(&command->depth, command->rects[i].left, command->rects[i].top, command->rects[i].right, command->rects[i].bottom,
				command->clearDepth, command->clearStencil, command->clearFlags, command->state.depthMask, command->state.stencilBits)) return false;
		}
		return true;
	}
	return false;
}

static bool npdisp_d3d_drawPrimitiveImmediate(NPDISP_D3D_CONTEXT* context, UINT32 primitiveType, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, const UINT16* indices, UINT32 indexCount)
{
	NPDISP_D3D_TARGET target;
	NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
	NPDISP_D3D_RASTERSTATE state;
	NPDISP_D3D_TEXTURE textures[NPDISP_D3D_RASTER_TEXTURE_STAGES];
	std::vector<UINT8> textureStorage[NPDISP_D3D_RASTER_TEXTURE_STAGES];
	std::vector<UINT8> targetStorage;
	NPDISP_D3D_SW_DEPTH_TARGET* depth = NULL;
	if (!context || !vertices || !context->lpTarget ||
		!(context->targetSnapshot.valid ? npdisp_d3d_targetFromSnapshotBuffered(&context->targetSnapshot, &targetStorage, &target) : npdisp_d3d_target(context->lpTarget, &target)) ||
		!npdisp_d3d_drawState(context, &state) || !npdisp_d3d_prepareTextures(context, &state, textureStorage, textures, false, NULL)) return false;
	if (state.zEnable || state.stencilEnable) {
		if (!context->lpDepth || !npdisp_d3d_depthSurfaceInfo(context->lpDepth, &depthTarget, &state.depthMask, &state.stencilBits) ||
			depthTarget.width < target.sw.width || depthTarget.height < target.sw.height || (state.stencilEnable && !state.stencilBits)) return false;
		depth = &depthTarget;
	}
	if (!npdisp_d3d_rasterize(&target, depth, primitiveType, vertices, vertexCount, indices, indexCount, &state)) return false;
	if (context->targetSnapshot.valid && context->targetSnapshot.systemMemory &&
		(!targetStorage.size() || !npdisp_d3d_write(&targetStorage[0], context->targetSnapshot.linearBase, (UINT32)targetStorage.size()))) return false;
	if (target.visible) { npdisp_setDirtyAll(); npdisp.updated = 1; }
	return true;
}

static bool npdisp_d3d_drawPrimitive(NPDISP_D3D_CONTEXT* context, UINT32 primitiveType, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, const UINT16* indices, UINT32 indexCount)
{
	NPDISP_D3D_ASYNC_COMMAND* command;
	std::vector<NPDISP_D3DTLVERTEX32> clippedVertices;
	UINT32 drawPrimitiveType = primitiveType;
	const NPDISP_D3DTLVERTEX32* drawVertices = vertices;
	UINT32 drawVertexCount = vertexCount;
	const UINT16* drawIndices = indices;
	UINT32 drawIndexCount = indexCount;
	UINT32 count = indices ? indexCount : vertexCount;
	if (!context || !vertices || !context->lpTarget || count > NPDISP_D3D_MAX_VERTICES) return false;
	{
		UINT32 clipPrimitiveType = 0xffffffffUL;
		if (!npdisp_d3d_clipDrawPrimitive(context, primitiveType, vertices, vertexCount, indices, indexCount,
			&clipPrimitiveType, &clippedVertices)) return false;
		if (clipPrimitiveType != 0xffffffffUL) {
			if (clippedVertices.empty()) return true;
			drawPrimitiveType = clipPrimitiveType;
			drawVertices = &clippedVertices[0];
			drawVertexCount = (UINT32)clippedVertices.size();
			drawIndices = NULL;
			drawIndexCount = 0;
		}
	}
	if (context->targetSnapshot.valid && context->targetSnapshot.systemMemory) {
		npdisp_d3d_flush();
		return npdisp_d3d_drawPrimitiveImmediate(context, drawPrimitiveType, drawVertices, drawVertexCount, drawIndices, drawIndexCount);
	}
	command = new NPDISP_D3D_ASYNC_COMMAND;
	if (!command) return false;
	memset(&command->target, 0, sizeof(command->target));
	memset(&command->depth, 0, sizeof(command->depth));
	memset(&command->state, 0, sizeof(command->state));
	memset(command->textures, 0, sizeof(command->textures));
	memset(command->textureSurfaces, 0, sizeof(command->textureSurfaces));
	command->type = NPDISP_D3D_ASYNC_DRAW;
	command->targetSurface = context->lpTarget;
	command->primitiveType = drawPrimitiveType;
	command->hasDepth = false;
	command->clearFlags = 0;
	command->clearColor = 0;
	command->clearDepth = 0;
	command->clearStencil = 0;
	command->bytes = 0;
	if (!(context->targetSnapshot.valid ? npdisp_d3d_targetFromSnapshot(&context->targetSnapshot, &command->target) : npdisp_d3d_target(context->lpTarget, &command->target)) ||
		!npdisp_d3d_drawState(context, &command->state) || !npdisp_d3d_prepareTextures(context, &command->state, command->textureStorage, command->textures, true, command->textureSurfaces)) {
		delete command;
		return false;
	}
	if (command->state.zEnable || command->state.stencilEnable) {
		if (!context->lpDepth || !npdisp_d3d_depthSurfaceInfo(context->lpDepth, &command->depth, &command->state.depthMask, &command->state.stencilBits) ||
			command->depth.width < command->target.sw.width || command->depth.height < command->target.sw.height ||
			(command->state.stencilEnable && !command->state.stencilBits)) {
			TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=zsurface depth=%08x stencil=%u", context->lpDepth, command->state.stencilEnable));
			delete command;
			return false;
		}
		command->hasDepth = true;
	}
	command->vertices.assign(drawVertices, drawVertices + drawVertexCount);
	if (drawIndices && drawIndexCount) command->indices.assign(drawIndices, drawIndices + drawIndexCount);
	if (!npdisp_d3d_asyncEnqueue(command)) {
		delete command;
		return npdisp_d3d_drawPrimitiveImmediate(context, drawPrimitiveType, drawVertices, drawVertexCount, drawIndices, drawIndexCount);
	}
	return true;
}

UINT32 npdisp_d3d_stateSize(void)
{
	UINT64 size = sizeof(NPDISP_D3D_STATE_HEADER);
	size += (UINT64)npdisp_d3d_contexts.size() * (sizeof(UINT32) + sizeof(NPDISP_D3D_CONTEXT));
	size += (UINT64)npdisp_d3d_tnlContexts.size() * (sizeof(UINT32) + sizeof(NPDISP_D3D_TNL_CONTEXT));
	size += (UINT64)npdisp_d3d_ddLocalNamespaces.size() * (sizeof(UINT32) + sizeof(UINT32));
	size += (UINT64)npdisp_d3d_surfaces.size() * (sizeof(UINT64) + sizeof(NPDISP_D3D_SURFACE));
	size += (UINT64)npdisp_d3d_surfaceSwapPending.size() * (sizeof(UINT32) + sizeof(UINT32));
	size += (UINT64)npdisp_d3d_legacyTextures.size() * (sizeof(UINT32) + sizeof(NPDISP_D3D_LEGACY_TEXTURE));
	size += (UINT64)npdisp_d3d_palettes.size() * (sizeof(UINT64) + sizeof(NPDISP_D3D_PALETTE));
	size += (UINT64)npdisp_d3d_surfacePalettes.size() * (sizeof(UINT64) + sizeof(UINT32));
	return (size <= (UINT64)0xffffffffUL) ? (UINT32)size : 0;
}

bool npdisp_d3d_saveState(UINT8* dst, UINT32 size)
{
	NPDISP_D3D_STATE_HEADER header = { 0 };
	UINT32 required = npdisp_d3d_stateSize();
	UINT32 remain;
	UINT8* out;
	if (!dst || !required || size < required) return false;
	header.magic = NPDISP_D3D_STATE_MAGIC;
	header.version = NPDISP_D3D_STATE_VERSION;
	header.nextContext = npdisp_d3d_nextContext;
	header.nextDDLocalNamespace = npdisp_d3d_nextDDLocalNamespace;
	header.nextLegacyTexture = npdisp_d3d_nextLegacyTexture;
	header.contextCount = (UINT32)npdisp_d3d_contexts.size();
	header.namespaceCount = (UINT32)npdisp_d3d_ddLocalNamespaces.size();
	header.surfaceCount = (UINT32)npdisp_d3d_surfaces.size();
	header.swapCount = (UINT32)npdisp_d3d_surfaceSwapPending.size();
	header.legacyTextureCount = (UINT32)npdisp_d3d_legacyTextures.size();
	header.paletteCount = (UINT32)npdisp_d3d_palettes.size();
	header.surfacePaletteCount = (UINT32)npdisp_d3d_surfacePalettes.size();
	memcpy(dst, &header, sizeof(header));
	out = dst + sizeof(header);
	remain = size - sizeof(header);
	return npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_contexts) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_tnlContexts) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_ddLocalNamespaces) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_surfaces) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_surfaceSwapPending) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_legacyTextures) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_palettes) &&
		npdisp_d3d_stateWriteMap(&out, &remain, npdisp_d3d_surfacePalettes) && remain == 0;
}

bool npdisp_d3d_loadState(const UINT8* src, UINT32 size)
{
	NPDISP_D3D_STATE_HEADER header;
	UINT32 remain;
	const UINT8* in;
	npdisp_d3d_reset();
	if (!src || size < sizeof(header)) return false;
	memcpy(&header, src, sizeof(header));
	if (header.magic != NPDISP_D3D_STATE_MAGIC || (header.version != 1U && header.version != NPDISP_D3D_STATE_VERSION) ||
		!header.nextContext || !header.nextDDLocalNamespace || !header.nextLegacyTexture) return false;
	in = src + sizeof(header);
	remain = size - sizeof(header);
	if (!npdisp_d3d_stateReadMap(&in, &remain, header.contextCount, &npdisp_d3d_contexts) ||
		(header.version >= 2U && !npdisp_d3d_stateReadMap(&in, &remain, header.contextCount, &npdisp_d3d_tnlContexts)) ||
		!npdisp_d3d_stateReadMap(&in, &remain, header.namespaceCount, &npdisp_d3d_ddLocalNamespaces) ||
		!npdisp_d3d_stateReadMap(&in, &remain, header.surfaceCount, &npdisp_d3d_surfaces) ||
		!npdisp_d3d_stateReadMap(&in, &remain, header.swapCount, &npdisp_d3d_surfaceSwapPending) ||
		!npdisp_d3d_stateReadMap(&in, &remain, header.legacyTextureCount, &npdisp_d3d_legacyTextures) ||
		!npdisp_d3d_stateReadMap(&in, &remain, header.paletteCount, &npdisp_d3d_palettes) ||
		!npdisp_d3d_stateReadMap(&in, &remain, header.surfacePaletteCount, &npdisp_d3d_surfacePalettes) || remain) {
		npdisp_d3d_reset();
		return false;
	}
	npdisp_d3d_nextContext = header.nextContext;
	npdisp_d3d_nextDDLocalNamespace = header.nextDDLocalNamespace;
	npdisp_d3d_nextLegacyTexture = header.nextLegacyTexture;
	if (header.version == 1U) {
		for (std::map<UINT32, NPDISP_D3D_CONTEXT>::const_iterator it = npdisp_d3d_contexts.begin(); it != npdisp_d3d_contexts.end(); ++it) {
			NPDISP_D3D_TNL_CONTEXT tnl;
			npdisp_d3d_tnlInitialize(&tnl);
			npdisp_d3d_tnlContexts[it->first] = tnl;
		}
	}
	return true;
}

void npdisp_d3d_reset(void)
{
	npdisp_d3d_asyncStopWorker();
	npdisp_d3d_contexts.clear();
	npdisp_d3d_tnlContexts.clear();
	npdisp_d3d_clipContexts.clear();
	npdisp_d3d_ddLocalNamespaces.clear();
	npdisp_d3d_surfaces.clear();
#if defined(SUPPORT_NPDISP_W2K)
	npdisp_d3d_w2kDepthSurfaces.clear();
#endif
	npdisp_d3d_surfaceSwapPending.clear();
	npdisp_d3d_legacyTextures.clear();
	npdisp_d3d_palettes.clear();
	npdisp_d3d_surfacePalettes.clear();
	npdisp_d3d_nextContext = 1;
	npdisp_d3d_nextDDLocalNamespace = 1;
	npdisp_d3d_nextLegacyTexture = 1;
	npdisp_d3d_tnlEnabled = np2cfg.npdisptnlenable != 0;
	InterlockedExchange(&npdisp_d3d_asyncVisibleDirty, 0);
	InterlockedExchange(&npdisp_d3d_asyncError, 0);
}

void npdisp_d3d_destroySurface(UINT32 lpSurfaceAddr)
{
	npdisp_d3d_flush();
#if defined(SUPPORT_NPDISP_W2K)
	npdisp_d3d_w2kDepthSurfaces.erase(lpSurfaceAddr);
#endif
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it = npdisp_d3d_surfaces.begin();
	while (it != npdisp_d3d_surfaces.end()) {
		if (it->second.lpSurface == lpSurfaceAddr) {
			npdisp_d3d_surfacePalettes.erase(it->first);
			npdisp_d3d_surfaces.erase(it++);
		}
		else ++it;
	}
}

static UINT32 npdisp_d3d_contextCreate(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_CONTEXTCREATEDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT context = { 0 };
	NPDISP_D3D_TARGET target;
	NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
	UINT32 targetLocal = 0;
	UINT32 depthLocal = 0;
	UINT32 handle;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) {
		TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_REJECT stage=read data=%08x", lpDataAddr));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_IN dd=%08x target=%08x depth=%08x pid=%08x version=%08x", data.lpDD, data.lpDDS, data.lpDDSZ, data.dwPID, data.dwhContext));
	context.ddLocalNamespace = npdisp_d3d_getDDLocalNamespace(data.lpDD, true);
	context.dwPID = data.dwPID;
	context.lpTarget = data.lpDDS;
	if (!context.lpTarget || !npdisp_d3d_target(context.lpTarget, &target)) {
		targetLocal = npdisp_d3d_indirectSurfaceLocal(data.lpDDS);
		if (!targetLocal || targetLocal == data.lpDDS || !npdisp_d3d_target(targetLocal, &target)) {
			TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_REJECT stage=target direct=%08x indirect=%08x", data.lpDDS, targetLocal));
			TRACEOUT11(("NPDISP11 D3D_CTX_CREATE_REJECT stage=target direct=%08x indirect=%08x", data.lpDDS, targetLocal));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		context.lpTarget = targetLocal;
	}
	context.lpDepth = data.lpDDSZ;
	if (!context.lpDepth && npdisp_d3d_attachedSurfaceByCaps(context.lpTarget, NPDISP_DDSCAPS_ZBUFFER, &context.lpDepth)) {
		TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_ZATTACH target=%08x depth=%08x", context.lpTarget, context.lpDepth));
		TRACEOUT11(("NPDISP11 D3D_CTX_CREATE_ZATTACH target=%08x depth=%08x", context.lpTarget, context.lpDepth));
	}
	if (context.lpDepth) {
		if (!npdisp_d3d_depthTarget(context.lpDepth, &depthTarget)) {
			depthLocal = npdisp_d3d_indirectSurfaceLocal(context.lpDepth);
			if (!depthLocal || depthLocal == context.lpDepth || !npdisp_d3d_depthTarget(depthLocal, &depthTarget)) {
				TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_REJECT stage=depth direct=%08x indirect=%08x", context.lpDepth, depthLocal));
				TRACEOUT11(("NPDISP11 D3D_CTX_CREATE_REJECT stage=depth direct=%08x indirect=%08x", context.lpDepth, depthLocal));
				return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			}
			context.lpDepth = depthLocal;
		}
		if (depthTarget.width < target.sw.width || depthTarget.height < target.sw.height) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	handle = npdisp_d3d_allocContext();
	if (!handle) {
		TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_REJECT stage=handle"));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	context.renderStates[NPDISP_D3DRENDERSTATE_FILLMODE] = NPDISP_D3DFILL_SOLID;
	context.renderStates[NPDISP_D3DRENDERSTATE_SHADEMODE] = NPDISP_D3DSHADE_GOURAUD;
	context.renderStates[NPDISP_D3DRENDERSTATE_CULLMODE] = NPDISP_D3DCULL_CCW;
	context.renderStates[NPDISP_D3DRENDERSTATE_LASTPIXEL] = 1U;
	context.renderStates[NPDISP_D3DRENDERSTATE_ALPHAFUNC] = NPDISP_D3DCMP_ALWAYS;
	context.renderStates[NPDISP_D3DRENDERSTATE_SRCBLEND] = NPDISP_D3DBLEND_ONE;
	context.renderStates[NPDISP_D3DRENDERSTATE_DESTBLEND] = NPDISP_D3DBLEND_ZERO;
	context.renderStates[NPDISP_D3DRENDERSTATE_TEXTUREMAPBLEND] = NPDISP_D3DTBLEND_MODULATE;
	context.renderStates[NPDISP_D3DRENDERSTATE_ZWRITEENABLE] = 1U;
	context.renderStates[NPDISP_D3DRENDERSTATE_ZFUNC] = NPDISP_D3DCMP_LESSEQUAL;
	context.renderStates[NPDISP_D3DRENDERSTATE_STENCILFAIL] = NPDISP_D3DSTENCILOP_KEEP;
	context.renderStates[NPDISP_D3DRENDERSTATE_STENCILZFAIL] = NPDISP_D3DSTENCILOP_KEEP;
	context.renderStates[NPDISP_D3DRENDERSTATE_STENCILPASS] = NPDISP_D3DSTENCILOP_KEEP;
	context.renderStates[NPDISP_D3DRENDERSTATE_STENCILFUNC] = NPDISP_D3DCMP_ALWAYS;
	context.renderStates[NPDISP_D3DRENDERSTATE_STENCILMASK] = 0xffffffffUL;
	context.renderStates[NPDISP_D3DRENDERSTATE_STENCILWRITEMASK] = 0xffffffffUL;
	context.renderStates[NPDISP_D3DRENDERSTATE_LIGHTING] = 1U;
	context.renderStates[NPDISP_D3DRENDERSTATE_COLORVERTEX] = 1U;
	context.renderStates[NPDISP_D3DRENDERSTATE_CLIPPING] = 1U;
	context.renderStates[NPDISP_D3DRENDERSTATE_LOCALVIEWER] = 1U;
	context.renderStates[NPDISP_D3DRENDERSTATE_DIFFUSEMATERIALSOURCE] = NPDISP_D3DMCS_COLOR1;
	context.renderStates[NPDISP_D3DRENDERSTATE_SPECULARMATERIALSOURCE] = NPDISP_D3DMCS_COLOR2;
	context.renderStates[NPDISP_D3DRENDERSTATE_AMBIENTMATERIALSOURCE] = NPDISP_D3DMCS_MATERIAL;
	context.renderStates[NPDISP_D3DRENDERSTATE_EMISSIVEMATERIALSOURCE] = NPDISP_D3DMCS_MATERIAL;
	context.viewportWidth = target.sw.width;
	context.viewportHeight = target.sw.height;
	context.zMax = 1.0f;
	context.wFar = 1.0f;
	context.textureStageStates[0][NPDISP_D3DTSS_COLOROP] = NPDISP_D3DTOP_MODULATE;
	context.textureStageStates[0][NPDISP_D3DTSS_COLORARG1] = NPDISP_D3DTA_TEXTURE;
	context.textureStageStates[0][NPDISP_D3DTSS_COLORARG2] = NPDISP_D3DTA_DIFFUSE;
	context.textureStageStates[0][NPDISP_D3DTSS_ALPHAOP] = NPDISP_D3DTOP_SELECTARG1;
	context.textureStageStates[0][NPDISP_D3DTSS_ALPHAARG1] = NPDISP_D3DTA_TEXTURE;
	context.textureStageStates[0][NPDISP_D3DTSS_ALPHAARG2] = NPDISP_D3DTA_DIFFUSE;
	for (UINT32 stage = 0; stage < NPDISP_D3D_MAX_TEXTURE_STAGES; ++stage) {
		context.textureStageStates[stage][NPDISP_D3DTSS_ADDRESSU] = NPDISP_D3DTADDRESS_WRAP;
		context.textureStageStates[stage][NPDISP_D3DTSS_ADDRESSV] = NPDISP_D3DTADDRESS_WRAP;
		context.textureStageStates[stage][NPDISP_D3DTSS_MAGFILTER] = NPDISP_D3DTFG_POINT;
		context.textureStageStates[stage][NPDISP_D3DTSS_MINFILTER] = NPDISP_D3DTFN_POINT;
		if (stage) context.textureStageStates[stage][NPDISP_D3DTSS_COLOROP] = NPDISP_D3DTOP_DISABLE;
	}
	npdisp_d3d_contexts[handle] = context;
	{
		NPDISP_D3D_TNL_CONTEXT tnl;
		npdisp_d3d_tnlInitialize(&tnl);
		npdisp_d3d_tnlContexts[handle] = tnl;
	}
	data.dwhContext = handle;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) {
		npdisp_d3d_contexts.erase(handle);
		npdisp_d3d_tnlContexts.erase(handle);
		TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_REJECT stage=write handle=%08x", handle));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_OK handle=%08x ns=%08x target=%08x depth=%08x size=%ux%u pitch=%d visible=%u", handle, context.ddLocalNamespace, context.lpTarget, context.lpDepth, target.sw.width, target.sw.height, target.sw.pitch, target.visible ? 1U : 0U));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_sceneCapture(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_SCENECAPTUREDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.dwhContext)) ||
		(data.dwFlag != NPDISP_D3DHAL_SCENE_CAPTURE_START && data.dwFlag != NPDISP_D3DHAL_SCENE_CAPTURE_END)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	context->sceneActive = (data.dwFlag == NPDISP_D3DHAL_SCENE_CAPTURE_START);
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_SCENE ctx=%08x flag=%u active=%u", data.dwhContext, data.dwFlag, context->sceneActive ? 1U : 0U));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_contextDestroy(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_CONTEXTDESTROYDATA32 data = { 0 };
	npdisp_d3d_flush();
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !npdisp_d3d_getContext(data.dwhContext)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	npdisp_d3d_destroyLegacyTextures(data.dwhContext);
	npdisp_d3d_contexts.erase(data.dwhContext);
	npdisp_d3d_tnlContexts.erase(data.dwhContext);
	npdisp_d3d_clipContexts.erase(data.dwhContext);
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_contextDestroyAll(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_CONTEXTDESTROYALLDATA32 data = { 0 };
	npdisp_d3d_flush();
	std::map<UINT32, NPDISP_D3D_CONTEXT>::iterator it;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	it = npdisp_d3d_contexts.begin();
	while (it != npdisp_d3d_contexts.end()) {
		if (it->second.dwPID == data.dwPID) {
			UINT32 handle = it->first;
			npdisp_d3d_destroyLegacyTextures(handle);
			npdisp_d3d_tnlContexts.erase(handle);
			npdisp_d3d_clipContexts.erase(handle);
			npdisp_d3d_contexts.erase(it++);
		}
		else ++it;
	}
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_textureCreate(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_TEXTURECREATEDATA32 data = { 0 };
	NPDISP_D3D_LEGACY_TEXTURE texture = { 0 };
	NPDISP_D3D_CONTEXT* context;
	UINT32 handle;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.dwhContext)) || !data.lpDDS) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	texture.lpSurface = data.lpDDS;
	if (!npdisp_d3d_captureTextureSnapshot(texture.lpSurface, &texture.textureSnapshot)) {
		texture.lpSurface = npdisp_d3d_indirectSurfaceLocal(data.lpDDS);
		if (!texture.lpSurface || !npdisp_d3d_captureTextureSnapshot(texture.lpSurface, &texture.textureSnapshot)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	handle = npdisp_d3d_allocLegacyTexture();
	if (!handle) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	texture.dwhContext = data.dwhContext;
	texture.lpDDS = data.lpDDS;
	npdisp_d3d_legacyTextures[handle] = texture;
	data.dwHandle = handle;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) {
		npdisp_d3d_legacyTextures.erase(handle);
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_TEXTURECREATE ctx=%08x handle=%08x dds=%08x surf=%08x", data.dwhContext, handle, data.lpDDS, texture.lpSurface));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_textureDestroy(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_TEXTUREDESTROYDATA32 data = { 0 };
	std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator it;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	it = npdisp_d3d_legacyTextures.find(data.dwHandle);
	if (it == npdisp_d3d_legacyTextures.end() || it->second.dwhContext != data.dwhContext) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	npdisp_d3d_legacyTextures.erase(it);
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_TEXTUREDESTROY ctx=%08x handle=%08x", data.dwhContext, data.dwHandle));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_textureSwap(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_TEXTURESWAPDATA32 data = { 0 };
	std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator first;
	std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator second;
	NPDISP_D3D_LEGACY_TEXTURE tmp;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || data.dwHandle1 == data.dwHandle2) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	first = npdisp_d3d_legacyTextures.find(data.dwHandle1);
	second = npdisp_d3d_legacyTextures.find(data.dwHandle2);
	if (first == npdisp_d3d_legacyTextures.end() || second == npdisp_d3d_legacyTextures.end() ||
		first->second.dwhContext != data.dwhContext || second->second.dwhContext != data.dwhContext) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	tmp = first->second;
	first->second.lpDDS = second->second.lpDDS;
	first->second.lpSurface = second->second.lpSurface;
	first->second.textureSnapshot = second->second.textureSnapshot;
	second->second.lpDDS = tmp.lpDDS;
	second->second.lpSurface = tmp.lpSurface;
	second->second.textureSnapshot = tmp.textureSnapshot;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_TEXTURESWAP ctx=%08x first=%08x second=%08x", data.dwhContext, data.dwHandle1, data.dwHandle2));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_textureGetSurf(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_TEXTUREGETSURFDATA32 data = { 0 };
	std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE>::iterator it;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	it = npdisp_d3d_legacyTextures.find(data.dwHandle);
	if (it == npdisp_d3d_legacyTextures.end() || it->second.dwhContext != data.dwhContext) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.lpDDS = it->second.lpDDS;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_renderState(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_RENDERSTATEDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	std::vector<NPDISP_D3DSTATE32> states;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) {
		TRACEOUTD3D(("NPDISP11 D3D_RS_REJECT stage=read data=%08x", lpDataAddr));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_RS_IN ctx=%08x exe=%08x off=%u count=%u", data.dwhContext, data.lpExeBuf, data.dwOffset, data.dwCount));
	context = npdisp_d3d_getContext(data.dwhContext);
	if (!context || data.dwCount > NPDISP_D3D_MAX_RENDERSTATES) {
		TRACEOUTD3D(("NPDISP11 D3D_RS_REJECT stage=args ctx=%08x count=%u", data.dwhContext, data.dwCount));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (data.dwCount) {
		states.resize(data.dwCount);
		if (!npdisp_d3d_bufferRead(data.lpExeBuf, data.dwOffset, &states[0], data.dwCount * (UINT32)sizeof(states[0]))) {
			TRACEOUTD3D(("NPDISP11 D3D_RS_REJECT stage=buffer exe=%08x off=%u count=%u", data.lpExeBuf, data.dwOffset, data.dwCount));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (!npdisp_d3d_applyStates(context, &states[0], data.dwCount)) {
			TRACEOUTD3D(("NPDISP11 D3D_RS_REJECT stage=states ctx=%08x count=%u", data.dwhContext, data.dwCount));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_RS_OK ctx=%08x count=%u", data.dwhContext, data.dwCount));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_renderPrimitive(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_RENDERPRIMITIVEDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	std::vector<NPDISP_D3DTRIANGLE32> triangles;
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	UINT32 opcode;
	UINT32 stride;
	UINT32 count;
	UINT32 maxIndex = 0;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) {
		TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=read data=%08x", lpDataAddr));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_RP_IN ctx=%08x exe=%08x tl=%08x off=%u tloff=%u inst=%08x", data.dwhContext, data.lpExeBuf, data.lpTLBuf, data.dwOffset, data.dwTLOffset, data.dwInstruction));
	context = npdisp_d3d_getContext(data.dwhContext);
	if (!context) {
		TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=context ctx=%08x", data.dwhContext));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	opcode = data.dwInstruction & 0xff;
	stride = (data.dwInstruction >> 8) & 0xff;
	count = data.dwInstruction >> 16;
	if (opcode != NPDISP_D3DOP_TRIANGLE || stride < sizeof(NPDISP_D3DTRIANGLE32) || count > NPDISP_D3D_MAX_VERTICES) {
		TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=instruction op=%u stride=%u count=%u", opcode, stride, count));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	// ZVISIBLE queryは描画せず、未実装時は可視として処理する。
	if (context->renderStates[NPDISP_D3DRENDERSTATE_ZVISIBLE]) {
		data.dwStatus &= ~NPDISP_D3DSTATUS_ZNOTVISIBLE;
		data.ddrval = 0;
		return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (!count) {
		data.ddrval = 0;
		return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	triangles.resize(count);
	for (UINT32 i = 0; i < count; ++i) {
		UINT32 offset;
		if (i > (0xffffffffUL - data.dwOffset) / stride) {
			TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=triangle-offset index=%u", i));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		offset = data.dwOffset + i * stride;
		if (!npdisp_d3d_bufferRead(data.lpExeBuf, offset, &triangles[i], sizeof(triangles[i]))) {
			TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=triangle-read index=%u off=%u", i, offset));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		UINT32 triFlags = triangles[i].wFlags & ~NPDISP_D3DTRIFLAG_EDGEENABLEMASK;
		if (triFlags > NPDISP_D3DTRIFLAG_EVEN) {
			TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=triangle-flags index=%u flags=%04x", i, triangles[i].wFlags));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (triangles[i].v1 > maxIndex) maxIndex = triangles[i].v1;
		if (triangles[i].v2 > maxIndex) maxIndex = triangles[i].v2;
		if (triangles[i].v3 > maxIndex) maxIndex = triangles[i].v3;
	}
	vertices.resize(maxIndex + 1U);
	for (UINT32 i = 0; i <= maxIndex; ++i) {
		UINT64 offset = (UINT64)data.dwTLOffset + (UINT64)i * NPDISP_D3D_LEGACY_TLVERTEX_SIZE;
		if (offset > 0xffffffffULL) {
			TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=vertex-offset index=%u", i));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (!npdisp_d3d_bufferRead(data.lpTLBuf, (UINT32)offset, &vertices[i], NPDISP_D3D_LEGACY_TLVERTEX_SIZE)) {
			TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=vertex-read index=%u off=%08x", i, (UINT32)offset));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		vertices[i].tu2 = vertices[i].tv2 = vertices[i].tw = vertices[i].tw2 = 0.0f;
	}
	for (UINT32 i = 0; i < count; ++i) {
		UINT16 index[3] = { triangles[i].v1, triangles[i].v2, triangles[i].v3 };
		if (!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_TRIANGLELIST, &vertices[0], (UINT32)vertices.size(), index, 3)) {
			TRACEOUTD3D(("NPDISP11 D3D_RP_REJECT stage=draw index=%u", i));
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_RP_OK ctx=%08x count=%u max=%u", data.dwhContext, count, maxIndex));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_getState(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_GETSTATEDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.dwhContext))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (data.dwWhich != NPDISP_D3DHALSTATE_GET_RENDER || data.dwStateType >= NPDISP_D3D_MAX_RENDERSTATES) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.dwStateValue = context->renderStates[data.dwStateType];
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_setRenderTarget(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_SETRENDERTARGETDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	NPDISP_D3D_TARGET target;
	NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
	UINT32 lpTarget = 0;
	UINT32 lpDepth = 0;
	bool targetReady = false;
	bool depthReady = false;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) {
		TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=read data=%08x", lpDataAddr));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_IN ctx=%08x target=%08x depth=%08x", data.dwhContext, data.lpDDS, data.lpDDSZ));
	context = npdisp_d3d_getContext(data.dwhContext);
	if (!context) {
		TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=context ctx=%08x", data.dwhContext));
		TRACEOUT11(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=context ctx=%08x", data.dwhContext));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	lpTarget = data.lpDDS;
	if (lpTarget && npdisp_d3d_target(lpTarget, &target)) targetReady = true;
	if (!targetReady) {
		lpTarget = npdisp_d3d_indirectSurfaceLocal(data.lpDDS);
		if (lpTarget && npdisp_d3d_target(lpTarget, &target)) targetReady = true;
	}
	if (!targetReady) {
		TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=target lcl=%08x raw=%08x", lpTarget, data.lpDDS));
		TRACEOUT11(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=target lcl=%08x raw=%08x", lpTarget, data.lpDDS));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (!data.lpDDSZ) {
		if (npdisp_d3d_attachedSurfaceByCaps(lpTarget, NPDISP_DDSCAPS_ZBUFFER, &lpDepth)) {
			depthReady = npdisp_d3d_depthTarget(lpDepth, &depthTarget);
			TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_ZATTACH target=%08x depth=%08x ready=%u", lpTarget, lpDepth, depthReady ? 1U : 0U));
		}
		else depthReady = true;
	}
	else {
		lpDepth = data.lpDDSZ;
		if (npdisp_d3d_depthTarget(lpDepth, &depthTarget)) depthReady = true;
		else {
			lpDepth = npdisp_d3d_indirectSurfaceLocal(data.lpDDSZ);
			if (lpDepth && npdisp_d3d_depthTarget(lpDepth, &depthTarget)) depthReady = true;
		}
	}
	if (!depthReady) {
		TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=depth lcl=%08x raw=%08x", lpDepth, data.lpDDSZ));
		TRACEOUT11(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=depth lcl=%08x raw=%08x", lpDepth, data.lpDDSZ));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (lpDepth && (depthTarget.width < target.sw.width || depthTarget.height < target.sw.height)) {
		TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=size target=%ux%u depth=%ux%u", target.sw.width, target.sw.height, depthTarget.width, depthTarget.height));
		TRACEOUT11(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=size target=%ux%u depth=%ux%u", target.sw.width, target.sw.height, depthTarget.width, depthTarget.height));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	context->lpTarget = lpTarget;
	context->lpDepth = lpDepth;
	memset(&context->targetSnapshot, 0, sizeof(context->targetSnapshot));
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_OK ctx=%08x target=%08x depth=%08x", data.dwhContext, lpTarget, lpDepth));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static bool npdisp_d3d_enqueueClear(NPDISP_D3D_CONTEXT* context, UINT32 flags, UINT32 color, UINT16 depthValue, UINT32 stencilValue, const std::vector<NPDISP_DDRECTL>& rects, bool viewportOnly)
{
	NPDISP_D3D_ASYNC_COMMAND* command;
	if (!context || !flags) return false;
	if ((flags & NPDISP_D3DCLEAR_TARGET) && context->targetSnapshot.valid && context->targetSnapshot.systemMemory) return false;
	command = new NPDISP_D3D_ASYNC_COMMAND;
	if (!command) return false;
	memset(&command->target, 0, sizeof(command->target));
	memset(&command->depth, 0, sizeof(command->depth));
	memset(&command->state, 0, sizeof(command->state));
	memset(command->textures, 0, sizeof(command->textures));
	memset(command->textureSurfaces, 0, sizeof(command->textureSurfaces));
	command->type = NPDISP_D3D_ASYNC_CLEAR;
	command->targetSurface = (flags & NPDISP_D3DCLEAR_TARGET) ? context->lpTarget : 0;
	command->hasDepth = false;
	command->primitiveType = 0;
	command->clearFlags = flags;
	command->clearColor = color;
	command->clearDepth = depthValue;
	command->clearStencil = stencilValue;
	command->bytes = 0;
	command->rects = rects;
	if ((flags & NPDISP_D3DCLEAR_TARGET) && (!context->lpTarget ||
		!(context->targetSnapshot.valid ? npdisp_d3d_targetFromSnapshot(&context->targetSnapshot, &command->target) : npdisp_d3d_target(context->lpTarget, &command->target)))) {
		delete command;
		return false;
	}
	if (flags & (NPDISP_D3DCLEAR_ZBUFFER | NPDISP_D3DCLEAR_STENCIL)) {
		if (!context->lpDepth || !npdisp_d3d_depthSurfaceInfo(context->lpDepth, &command->depth, &command->state.depthMask, &command->state.stencilBits) ||
			((flags & NPDISP_D3DCLEAR_STENCIL) && !command->state.stencilBits)) { delete command; return false; }
		command->hasDepth = true;
	}
	if (viewportOnly && command->rects.empty()) {
		NPDISP_DDRECTL rect;
		rect.left = context->viewportWidth ? (SINT32)context->viewportX : 0;
		rect.top = context->viewportHeight ? (SINT32)context->viewportY : 0;
		rect.right = context->viewportWidth ? (SINT32)(context->viewportX + context->viewportWidth) :
			((flags & NPDISP_D3DCLEAR_TARGET) ? (SINT32)command->target.sw.width : (SINT32)command->depth.width);
		rect.bottom = context->viewportHeight ? (SINT32)(context->viewportY + context->viewportHeight) :
			((flags & NPDISP_D3DCLEAR_TARGET) ? (SINT32)command->target.sw.height : (SINT32)command->depth.height);
		command->rects.push_back(rect);
	}
	if (!npdisp_d3d_asyncEnqueue(command)) { delete command; return false; }
	return true;
}

static UINT32 npdisp_d3d_clear(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_CLEARDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	std::vector<NPDISP_DDRECTL> rects;
	const UINT32 validFlags = NPDISP_D3DCLEAR_TARGET | NPDISP_D3DCLEAR_ZBUFFER;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	context = npdisp_d3d_getContext(data.dwhContext);
	TRACEOUTD3D(("NPDISP11 D3D_CLEAR_IN ctx=%08x flags=%08x color=%08x depth=%08x rects=%08x count=%u", data.dwhContext, data.dwFlags, data.dwFillColor, data.dwFillDepth, data.lpRects, data.dwNumRects));
	if (!context || !data.dwFlags || (data.dwFlags & ~validFlags) || data.dwNumRects > NPDISP_D3D_MAX_RECTS) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (data.dwNumRects) {
		rects.resize(data.dwNumRects);
		if (!data.lpRects || !npdisp_d3d_read(&rects[0], data.lpRects, data.dwNumRects * (UINT32)sizeof(rects[0]))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (!npdisp_d3d_enqueueClear(context, data.dwFlags, data.dwFillColor, (UINT16)data.dwFillDepth, 0, rects, false)) {
		NPDISP_D3D_TARGET target;
		NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
		UINT32 depthMask = 0x0000ffffUL;
		UINT32 stencilBits = 0;
		npdisp_d3d_flush();
		if ((data.dwFlags & NPDISP_D3DCLEAR_TARGET) && (!context->lpTarget || !npdisp_d3d_target(context->lpTarget, &target))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if ((data.dwFlags & NPDISP_D3DCLEAR_ZBUFFER) && (!context->lpDepth || !npdisp_d3d_depthSurfaceInfo(context->lpDepth, &depthTarget, &depthMask, &stencilBits))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if (data.dwFlags & NPDISP_D3DCLEAR_TARGET) {
			if (rects.empty()) { if (!npdisp_d3d_sw_clear(&target.sw, 0, 0, (SINT32)target.sw.width, (SINT32)target.sw.height, data.dwFillColor)) return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
			else for (UINT32 i = 0; i < rects.size(); ++i) if (!npdisp_d3d_sw_clear(&target.sw, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom, data.dwFillColor)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			if (target.visible) { npdisp_setDirtyAll(); npdisp.updated = 1; }
		}
		if (data.dwFlags & NPDISP_D3DCLEAR_ZBUFFER) {
			const UINT16 depth = (UINT16)data.dwFillDepth;
			if (rects.empty()) { if (!npdisp_d3d_sw_clearDepthStencil(&depthTarget, 0, 0, (SINT32)depthTarget.width, (SINT32)depthTarget.height, depth, 0, NPDISP_D3DCLEAR_ZBUFFER, depthMask, stencilBits)) return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
			else for (UINT32 i = 0; i < rects.size(); ++i) if (!npdisp_d3d_sw_clearDepthStencil(&depthTarget, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom, depth, 0, NPDISP_D3DCLEAR_ZBUFFER, depthMask, stencilBits)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
	}
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_CLEAR_QUEUE ctx=%08x flags=%08x count=%u", data.dwhContext, data.dwFlags, data.dwNumRects));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_drawOnePrimitive(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_DRAWONEPRIMITIVEDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) { TRACEOUTD3D(("NPDISP11 D3D_DRAW1_REJECT stage=data addr=%08x", lpDataAddr)); return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
	TRACEOUTD3D(("NPDISP11 D3D_DRAW1_IN ctx=%08x flags=%08x prim=%u vtype=%u verts=%u ptr=%08x", data.dwhContext, data.dwFlags, data.primitiveType, data.vertexType, data.dwNumVertices, data.lpVertices));
	if (!(context = npdisp_d3d_getContext(data.dwhContext))) { TRACEOUTD3D(("NPDISP11 D3D_DRAW1_REJECT stage=context ctx=%08x", data.dwhContext)); return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
	if (data.vertexType != NPDISP_D3DVT_TLVERTEX || !data.dwNumVertices || data.dwNumVertices > NPDISP_D3D_MAX_VERTICES || !data.lpVertices) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAW1_REJECT stage=params vtype=%u verts=%u ptr=%08x", data.vertexType, data.dwNumVertices, data.lpVertices));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	vertices.resize(data.dwNumVertices);
	for (UINT32 i = 0; i < data.dwNumVertices; ++i) {
		UINT64 addr = (UINT64)data.lpVertices + (UINT64)i * NPDISP_D3D_LEGACY_TLVERTEX_SIZE;
		if (addr > 0xffffffffULL || !npdisp_d3d_read(&vertices[i], (UINT32)addr, NPDISP_D3D_LEGACY_TLVERTEX_SIZE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		vertices[i].tu2 = vertices[i].tv2 = vertices[i].tw = vertices[i].tw2 = 0.0f;
	}
	if (!npdisp_d3d_drawPrimitive(context, data.primitiveType, &vertices[0], data.dwNumVertices, NULL, 0)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_drawOneIndexedPrimitive(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_DRAWONEINDEXEDPRIMITIVEDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	std::vector<UINT16> indices;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) { TRACEOUTD3D(("NPDISP11 D3D_DRAW1I_REJECT stage=data addr=%08x", lpDataAddr)); return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
	TRACEOUTD3D(("NPDISP11 D3D_DRAW1I_IN ctx=%08x flags=%08x prim=%u vtype=%u verts=%u idx=%u vptr=%08x iptr=%08x", data.dwhContext, data.dwFlags, data.primitiveType, data.vertexType, data.dwNumVertices, data.dwNumIndices, data.lpVertices, data.lpIndices));
	if (!(context = npdisp_d3d_getContext(data.dwhContext))) { TRACEOUTD3D(("NPDISP11 D3D_DRAW1I_REJECT stage=context ctx=%08x", data.dwhContext)); return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
	if (data.vertexType != NPDISP_D3DVT_TLVERTEX || !data.dwNumVertices || data.dwNumVertices > NPDISP_D3D_MAX_VERTICES ||
		!data.dwNumIndices || data.dwNumIndices > NPDISP_D3D_MAX_VERTICES || !data.lpVertices || !data.lpIndices) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAW1I_REJECT stage=params vtype=%u verts=%u idx=%u vptr=%08x iptr=%08x", data.vertexType, data.dwNumVertices, data.dwNumIndices, data.lpVertices, data.lpIndices));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	vertices.resize(data.dwNumVertices);
	indices.resize(data.dwNumIndices);
	for (UINT32 i = 0; i < data.dwNumVertices; ++i) {
		UINT64 addr = (UINT64)data.lpVertices + (UINT64)i * NPDISP_D3D_LEGACY_TLVERTEX_SIZE;
		if (addr > 0xffffffffULL || !npdisp_d3d_read(&vertices[i], (UINT32)addr, NPDISP_D3D_LEGACY_TLVERTEX_SIZE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		vertices[i].tu2 = vertices[i].tv2 = vertices[i].tw = vertices[i].tw2 = 0.0f;
	}
	if (!npdisp_d3d_read(&indices[0], data.lpIndices, data.dwNumIndices * (UINT32)sizeof(indices[0]))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	for (UINT32 i = 0; i < data.dwNumIndices; ++i) if (indices[i] >= data.dwNumVertices) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_d3d_drawPrimitive(context, data.primitiveType, &vertices[0], data.dwNumVertices, &indices[0], data.dwNumIndices)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_drawPrimitives(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_DRAWPRIMITIVESDATA32 data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	UINT32 cursor;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) { TRACEOUTD3D(("NPDISP11 D3D_DRAWS_REJECT stage=data addr=%08x", lpDataAddr)); return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
	TRACEOUTD3D(("NPDISP11 D3D_DRAWS_IN ctx=%08x flags=%08x data=%08x", data.dwhContext, data.dwFlags, data.lpData));
	if (!(context = npdisp_d3d_getContext(data.dwhContext)) || !data.lpData) { TRACEOUTD3D(("NPDISP11 D3D_DRAWS_REJECT stage=params ctx=%08x data=%08x", data.dwhContext, data.lpData)); return NPDISP_DDHAL_DRIVER_NOTHANDLED; }
	cursor = data.lpData;
	for (UINT32 block = 0; block < NPDISP_D3D_MAX_DRAW_BLOCKS; ++block) {
		NPDISP_D3DHAL_DRAWPRIMCOUNTS32 counts = { 0 };
		std::vector<NPDISP_D3DSTATE32> states;
		UINT32 vertexAddr;
		UINT32 next;
		if (!npdisp_d3d_read(&counts, cursor, sizeof(counts)) || !npdisp_d3d_add(cursor, sizeof(counts), &cursor) ||
			counts.wNumStateChanges > NPDISP_D3D_MAX_RENDERSTATES) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		if (counts.wNumStateChanges) {
			UINT32 stateBytes = (UINT32)counts.wNumStateChanges * sizeof(NPDISP_D3DSTATE32);
			states.resize(counts.wNumStateChanges);
			if (!npdisp_d3d_read(&states[0], cursor, stateBytes) || !npdisp_d3d_applyStates(context, &states[0], counts.wNumStateChanges) ||
				!npdisp_d3d_add(cursor, stateBytes, &cursor)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (!counts.wNumVertices) {
			data.ddrval = 0;
			return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		if (counts.wVertexType != NPDISP_D3DVT_TLVERTEX || cursor > 0xffffffe0UL) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		vertexAddr = (cursor + 31U) & ~31U;
		std::vector<NPDISP_D3DTLVERTEX32> vertices(counts.wNumVertices);
		UINT32 vertexBytes = (UINT32)counts.wNumVertices * NPDISP_D3D_LEGACY_TLVERTEX_SIZE;
		for (UINT32 i = 0; i < counts.wNumVertices; ++i) {
			UINT32 addr;
			if (!npdisp_d3d_add(vertexAddr, i * NPDISP_D3D_LEGACY_TLVERTEX_SIZE, &addr) ||
				!npdisp_d3d_read(&vertices[i], addr, NPDISP_D3D_LEGACY_TLVERTEX_SIZE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
			vertices[i].tu2 = vertices[i].tv2 = vertices[i].tw = vertices[i].tw2 = 0.0f;
		}
		if (!npdisp_d3d_drawPrimitive(context, counts.wPrimitiveType, &vertices[0], counts.wNumVertices, NULL, 0) ||
			!npdisp_d3d_add(vertexAddr, vertexBytes, &next)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		cursor = next;
	}
	return NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static bool npdisp_d3d_dp2CommandRead(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 offset, void* dst, UINT32 size)
{
	UINT64 end;
	UINT32 surfaceOffset;
	if (!data || !dst || !size) return false;
	end = (UINT64)offset + size;
	if (end > data->dwCommandLength || !npdisp_d3d_add(data->dwCommandOffset, offset, &surfaceOffset)) return false;
	if (data->dwFlags & NPDISP_D3DHALDP2_W2K_DIRECTBUFFERS)
		return npdisp_d3d_rawBufferRead(data->lpDDCommands, surfaceOffset, dst, size);
	return npdisp_d3d_lclBufferRead(data->lpDDCommands, surfaceOffset, dst, size);
}

static void npdisp_d3d_matrixIdentity(NPDISP_D3DMATRIX32* matrix)
{
	memset(matrix, 0, sizeof(*matrix));
	for (UINT32 i = 0; i < 4U; ++i) matrix->m[i][i] = 1.0f;
}

static void npdisp_d3d_tnlInitialize(NPDISP_D3D_TNL_CONTEXT* tnl)
{
	memset(tnl, 0, sizeof(*tnl));
	npdisp_d3d_matrixIdentity(&tnl->world);
	for (UINT32 i = 0; i < 3U; ++i) npdisp_d3d_matrixIdentity(&tnl->worldBlend[i]);
	npdisp_d3d_matrixIdentity(&tnl->view);
	npdisp_d3d_matrixIdentity(&tnl->projection);
	for (UINT32 i = 0; i < 8U; ++i) npdisp_d3d_matrixIdentity(&tnl->texture[i]);
	tnl->material.diffuse.r = tnl->material.diffuse.g = tnl->material.diffuse.b = tnl->material.diffuse.a = 1.0f;
	tnl->material.ambient = tnl->material.diffuse;
}

static NPDISP_D3DVECTOR32 npdisp_d3d_transformPoint(const NPDISP_D3DVECTOR32* value, const NPDISP_D3DMATRIX32* matrix, float* w)
{
	NPDISP_D3DVECTOR32 result;
	result.x = value->x * matrix->m[0][0] + value->y * matrix->m[1][0] + value->z * matrix->m[2][0] + matrix->m[3][0];
	result.y = value->x * matrix->m[0][1] + value->y * matrix->m[1][1] + value->z * matrix->m[2][1] + matrix->m[3][1];
	result.z = value->x * matrix->m[0][2] + value->y * matrix->m[1][2] + value->z * matrix->m[2][2] + matrix->m[3][2];
	if (w) *w = value->x * matrix->m[0][3] + value->y * matrix->m[1][3] + value->z * matrix->m[2][3] + matrix->m[3][3];
	return result;
}

static bool npdisp_d3d_normalize(NPDISP_D3DVECTOR32* value)
{
	float length = (float)sqrt(value->x * value->x + value->y * value->y + value->z * value->z);
	if (length <= 0.000001f) return false;
	value->x /= length;
	value->y /= length;
	value->z /= length;
	return true;
}

static float npdisp_d3d_colorComponent(UINT32 color, UINT32 shift)
{
	return (float)((color >> shift) & 0xffU) / 255.0f;
}

static UINT32 npdisp_d3d_packColor(float a, float r, float g, float b)
{
	UINT32 aa, rr, gg, bb;
	if (a < 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;
	if (r < 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
	if (g < 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
	if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;
	aa = (UINT32)(a * 255.0f + 0.5f);
	rr = (UINT32)(r * 255.0f + 0.5f);
	gg = (UINT32)(g * 255.0f + 0.5f);
	bb = (UINT32)(b * 255.0f + 0.5f);
	return (aa << 24) | (rr << 16) | (gg << 8) | bb;
}

static NPDISP_D3DCOLORVALUE32 npdisp_d3d_tnlColor(UINT32 color)
{
	NPDISP_D3DCOLORVALUE32 value;
	value.a = npdisp_d3d_colorComponent(color, 24);
	value.r = npdisp_d3d_colorComponent(color, 16);
	value.g = npdisp_d3d_colorComponent(color, 8);
	value.b = npdisp_d3d_colorComponent(color, 0);
	return value;
}

static NPDISP_D3DCOLORVALUE32 npdisp_d3d_tnlMaterialSource(const NPDISP_D3D_CONTEXT* context,
	const NPDISP_D3DCOLORVALUE32* material, UINT32 state, UINT32 color1, UINT32 color2, bool hasColor1, bool hasColor2)
{
	UINT32 source;
	if (!context->renderStates[NPDISP_D3DRENDERSTATE_COLORVERTEX]) return *material;
	source = context->renderStates[state];
	if (source == NPDISP_D3DMCS_COLOR1 && hasColor1) return npdisp_d3d_tnlColor(color1);
	if (source == NPDISP_D3DMCS_COLOR2 && hasColor2) return npdisp_d3d_tnlColor(color2);
	return *material;
}

static NPDISP_D3DVECTOR32 npdisp_d3d_transformVector(const NPDISP_D3DVECTOR32* value, const NPDISP_D3DMATRIX32* matrix)
{
	NPDISP_D3DVECTOR32 result;
	result.x = value->x * matrix->m[0][0] + value->y * matrix->m[1][0] + value->z * matrix->m[2][0];
	result.y = value->x * matrix->m[0][1] + value->y * matrix->m[1][1] + value->z * matrix->m[2][1];
	result.z = value->x * matrix->m[0][2] + value->y * matrix->m[1][2] + value->z * matrix->m[2][2];
	return result;
}

static NPDISP_D3DVECTOR32 npdisp_d3d_transformNormal(const NPDISP_D3DVECTOR32* value, const NPDISP_D3DMATRIX32* matrix)
{
	const float a = matrix->m[0][0], b = matrix->m[0][1], c = matrix->m[0][2];
	const float d = matrix->m[1][0], e = matrix->m[1][1], f = matrix->m[1][2];
	const float g = matrix->m[2][0], h = matrix->m[2][1], i = matrix->m[2][2];
	const float c00 = e * i - f * h, c01 = f * g - d * i, c02 = d * h - e * g;
	const float c10 = c * h - b * i, c11 = a * i - c * g, c12 = b * g - a * h;
	const float c20 = b * f - c * e, c21 = c * d - a * f, c22 = a * e - b * d;
	const float determinant = a * c00 + b * c01 + c * c02;
	NPDISP_D3DVECTOR32 result;
	if (determinant < 0.00000001f && determinant > -0.00000001f)
		return npdisp_d3d_transformVector(value, matrix);
	result.x = (value->x * c00 + value->y * c10 + value->z * c20) / determinant;
	result.y = (value->x * c01 + value->y * c11 + value->z * c21) / determinant;
	result.z = (value->x * c02 + value->y * c12 + value->z * c22) / determinant;
	return result;
}

typedef struct {
	const NPDISP_D3DMATRIX32* matrix;
	float c00, c01, c02, c10, c11, c12, c20, c21, c22, determinant, inverseDeterminant;
} NPDISP_D3D_TNL_NORMAL_CACHE;

typedef struct NPDISP_D3D_TNL_CACHE {
	NPDISP_D3D_TNL_NORMAL_CACHE normal[5];
	NPDISP_D3DVECTOR32 lightView[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	NPDISP_D3DVECTOR32 spotView[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	bool lightDirectionValid[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	bool spotDirectionValid[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	float spotInner[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	float spotOuter[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	bool needsNormal;
	bool specularEnabled;
	bool fastTnl;
	bool useWorldView;
	bool useWorldViewProjection;
	NPDISP_D3DMATRIX32 worldView;
	NPDISP_D3DMATRIX32 worldViewProjection;
	UINT32 activeLights[NPDISP_D3D_MAX_ACTIVE_LIGHTS];
	UINT32 activeLightCount;
	UINT32 texCoordIndex[NPDISP_D3D_RASTER_TEXTURE_STAGES];
	UINT32 texTransformFlags[NPDISP_D3D_RASTER_TEXTURE_STAGES];
} NPDISP_D3D_TNL_CACHE;

static void npdisp_d3d_tnlCacheNormal(const NPDISP_D3DMATRIX32* matrix, NPDISP_D3D_TNL_NORMAL_CACHE* cache)
{
	const float a = matrix->m[0][0], b = matrix->m[0][1], c = matrix->m[0][2];
	const float d = matrix->m[1][0], e = matrix->m[1][1], f = matrix->m[1][2];
	const float g = matrix->m[2][0], h = matrix->m[2][1], i = matrix->m[2][2];
	cache->matrix = matrix;
	cache->c00 = e * i - f * h; cache->c01 = f * g - d * i; cache->c02 = d * h - e * g;
	cache->c10 = c * h - b * i; cache->c11 = a * i - c * g; cache->c12 = b * g - a * h;
	cache->c20 = b * f - c * e; cache->c21 = c * d - a * f; cache->c22 = a * e - b * d;
	cache->determinant = a * cache->c00 + b * cache->c01 + c * cache->c02;
	cache->inverseDeterminant = (cache->determinant > 0.0001f || cache->determinant < -0.0001f) ?
		1.0f / cache->determinant : 0.0f;
}

static NPDISP_D3DVECTOR32 npdisp_d3d_tnlCachedNormal(const NPDISP_D3DVECTOR32* value, const NPDISP_D3D_TNL_NORMAL_CACHE* cache)
{
	NPDISP_D3DVECTOR32 result;
	if (cache->determinant < 0.00000001f && cache->determinant > -0.00000001f)
		return npdisp_d3d_transformNormal(value, cache->matrix);
	if (cache->inverseDeterminant) {
		result.x = (value->x * cache->c00 + value->y * cache->c10 + value->z * cache->c20) * cache->inverseDeterminant;
		result.y = (value->x * cache->c01 + value->y * cache->c11 + value->z * cache->c21) * cache->inverseDeterminant;
		result.z = (value->x * cache->c02 + value->y * cache->c12 + value->z * cache->c22) * cache->inverseDeterminant;
	}
	else {
		result.x = (value->x * cache->c00 + value->y * cache->c10 + value->z * cache->c20) / cache->determinant;
		result.y = (value->x * cache->c01 + value->y * cache->c11 + value->z * cache->c21) / cache->determinant;
		result.z = (value->x * cache->c02 + value->y * cache->c12 + value->z * cache->c22) / cache->determinant;
	}
	return result;
}

static void npdisp_d3d_tnlBuildCache(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl, UINT32 fvf, NPDISP_D3D_TNL_CACHE* cache)
{
	cache->activeLightCount = 0;
	cache->fastTnl = np2cfg.npdispd3dquality >= NPDISP_D3D_QUALITY_FAST_TNL || np2cfg.npdisptnlfast != 0;
	cache->useWorldView = (fvf & NPDISP_D3DFVF_POSITION_MASK) == NPDISP_D3DFVF_XYZ &&
		!context->renderStates[NPDISP_D3DRENDERSTATE_VERTEXBLEND] &&
		tnl->world.m[0][3] == 0.0f && tnl->world.m[1][3] == 0.0f && tnl->world.m[2][3] == 0.0f &&
		tnl->world.m[3][3] == 1.0f &&
		tnl->view.m[0][3] == 0.0f && tnl->view.m[1][3] == 0.0f && tnl->view.m[2][3] == 0.0f &&
		tnl->view.m[3][3] == 1.0f;
	if (cache->useWorldView) npdisp_d3d_matrixMultiply(&tnl->world, &tnl->view, &cache->worldView);
	cache->useWorldViewProjection = cache->useWorldView &&
		!context->renderStates[NPDISP_D3DRENDERSTATE_LIGHTING] &&
		!context->renderStates[NPDISP_D3DRENDERSTATE_FOGENABLE];
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		UINT32 generated = context->textureStageStates[stage][NPDISP_D3DTSS_TEXCOORDINDEX] & NPDISP_D3DTSS_TCI_GENMASK;
		if (generated == 0x00020000UL || generated == 0x00030000UL) cache->useWorldViewProjection = false;
	}
	if (cache->useWorldViewProjection) npdisp_d3d_matrixMultiply(&cache->worldView, &tnl->projection, &cache->worldViewProjection);
	cache->needsNormal = context->renderStates[NPDISP_D3DRENDERSTATE_LIGHTING] != 0;
	cache->specularEnabled = !cache->fastTnl && context->renderStates[NPDISP_D3DRENDERSTATE_SPECULARENABLE] != 0 && tnl->material.power > 0.0f;
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		UINT32 generated = context->textureStageStates[stage][NPDISP_D3DTSS_TEXCOORDINDEX] & NPDISP_D3DTSS_TCI_GENMASK;
		if (generated == 0x00010000UL || generated == 0x00030000UL) cache->needsNormal = true;
	}
	if (cache->needsNormal && (fvf & NPDISP_D3DFVF_NORMAL)) {
		UINT32 blendCount = context->renderStates[NPDISP_D3DRENDERSTATE_VERTEXBLEND];
		npdisp_d3d_tnlCacheNormal(&tnl->world, &cache->normal[0]);
		for (UINT32 i = 0; i < blendCount && i < 3U; ++i)
			npdisp_d3d_tnlCacheNormal(&tnl->worldBlend[i], &cache->normal[i + 1U]);
		npdisp_d3d_tnlCacheNormal(&tnl->view, &cache->normal[4]);
	}
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		cache->texCoordIndex[stage] = context->textureStageStates[stage][NPDISP_D3DTSS_TEXCOORDINDEX];
		cache->texTransformFlags[stage] = context->textureStageStates[stage][NPDISP_D3DTSS_TEXTURETRANSFORMFLAGS];
	}
	if (context->renderStates[NPDISP_D3DRENDERSTATE_LIGHTING]) {
		for (UINT32 i = 0; i < NPDISP_D3D_MAX_ACTIVE_LIGHTS; ++i) {
			const NPDISP_D3D_TNL_LIGHT* entry = &tnl->lights[i];
			if (!entry->valid || !entry->enabled) continue;
			if (entry->light.type == NPDISP_D3DLIGHT_DIRECTIONAL) {
				cache->lightView[i] = npdisp_d3d_transformVector(&entry->light.direction, &tnl->view);
				cache->lightView[i].x = -cache->lightView[i].x;
				cache->lightView[i].y = -cache->lightView[i].y;
				cache->lightView[i].z = -cache->lightView[i].z;
				cache->lightDirectionValid[i] = npdisp_d3d_normalize(&cache->lightView[i]);
			}
			else if (entry->light.type == NPDISP_D3DLIGHT_POINT || entry->light.type == NPDISP_D3DLIGHT_SPOT) {
				cache->lightView[i] = npdisp_d3d_transformPoint(&entry->light.position, &tnl->view, NULL);
				if (entry->light.type == NPDISP_D3DLIGHT_SPOT) {
					cache->spotView[i] = npdisp_d3d_transformVector(&entry->light.direction, &tnl->view);
					cache->spotDirectionValid[i] = npdisp_d3d_normalize(&cache->spotView[i]);
					cache->spotInner[i] = (float)cos(entry->light.theta * 0.5f);
					cache->spotOuter[i] = (float)cos(entry->light.phi * 0.5f);
				}
			}
			else continue;
			cache->activeLights[cache->activeLightCount++] = i;
			if (cache->fastTnl && cache->activeLightCount == 2U) break;
		}
	}
}

static void npdisp_d3d_tnlLightVertex(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl,
	const NPDISP_D3D_TNL_CACHE* cache, const NPDISP_D3DVECTOR32* viewPosition, NPDISP_D3DVECTOR32 normal, UINT32 color1, UINT32 color2,
	bool hasColor1, bool hasColor2, UINT32* diffuseColor, UINT32* specularColor)
{
	NPDISP_D3DCOLORVALUE32 diffuse = npdisp_d3d_tnlMaterialSource(context, &tnl->material.diffuse,
		NPDISP_D3DRENDERSTATE_DIFFUSEMATERIALSOURCE, color1, color2, hasColor1, hasColor2);
	NPDISP_D3DCOLORVALUE32 ambient = npdisp_d3d_tnlMaterialSource(context, &tnl->material.ambient,
		NPDISP_D3DRENDERSTATE_AMBIENTMATERIALSOURCE, color1, color2, hasColor1, hasColor2);
	NPDISP_D3DCOLORVALUE32 specular = npdisp_d3d_tnlMaterialSource(context, &tnl->material.specular,
		NPDISP_D3DRENDERSTATE_SPECULARMATERIALSOURCE, color1, color2, hasColor1, hasColor2);
	NPDISP_D3DCOLORVALUE32 emissive = npdisp_d3d_tnlMaterialSource(context, &tnl->material.emissive,
		NPDISP_D3DRENDERSTATE_EMISSIVEMATERIALSOURCE, color1, color2, hasColor1, hasColor2);
	UINT32 globalAmbient = context->renderStates[NPDISP_D3DRENDERSTATE_AMBIENT];
	float r = emissive.r + ambient.r * npdisp_d3d_colorComponent(globalAmbient, 16);
	float g = emissive.g + ambient.g * npdisp_d3d_colorComponent(globalAmbient, 8);
	float b = emissive.b + ambient.b * npdisp_d3d_colorComponent(globalAmbient, 0);
	float sr = 0.0f, sg = 0.0f, sb = 0.0f;
	NPDISP_D3DVECTOR32 viewer = { 0.0f, 0.0f, -1.0f };
	if (cache->specularEnabled && context->renderStates[NPDISP_D3DRENDERSTATE_LOCALVIEWER]) {
		viewer.x = -viewPosition->x;
		viewer.y = -viewPosition->y;
		viewer.z = -viewPosition->z;
		if (!npdisp_d3d_normalize(&viewer)) viewer.z = -1.0f;
	}
	for (UINT32 active = 0; active < cache->activeLightCount; ++active) {
		UINT32 i = cache->activeLights[active];
		const NPDISP_D3D_TNL_LIGHT& entry = tnl->lights[i];
		NPDISP_D3DVECTOR32 direction;
		float attenuation = 1.0f;
		float distance = 0.0f;
		float dot;
		if (!entry.valid || !entry.enabled) continue;
		if (entry.light.type == NPDISP_D3DLIGHT_DIRECTIONAL) {
			direction = cache->lightView[i];
		}
		else if (entry.light.type == NPDISP_D3DLIGHT_POINT || entry.light.type == NPDISP_D3DLIGHT_SPOT) {
			NPDISP_D3DVECTOR32 lightPosition = cache->lightView[i];
			direction.x = lightPosition.x - viewPosition->x;
			direction.y = lightPosition.y - viewPosition->y;
			direction.z = lightPosition.z - viewPosition->z;
			distance = (float)sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
			if (entry.light.range > 0.0f && distance > entry.light.range) continue;
			attenuation = entry.light.attenuation0 + entry.light.attenuation1 * distance + entry.light.attenuation2 * distance * distance;
			if (attenuation <= 0.000001f) continue;
			attenuation = 1.0f / attenuation;
		}
		else continue;
		if (entry.light.type == NPDISP_D3DLIGHT_DIRECTIONAL) {
			if (!cache->lightDirectionValid[i]) continue;
		}
		else if (cache->fastTnl) {
			if (distance <= 0.000001f) continue;
			direction.x /= distance;
			direction.y /= distance;
			direction.z /= distance;
		}
		else if (!npdisp_d3d_normalize(&direction)) continue;
		if (entry.light.type == NPDISP_D3DLIGHT_SPOT) {
			const NPDISP_D3DVECTOR32& spotDirection = cache->spotView[i];
			float spotCos;
			float inner = cache->spotInner[i];
			float outer = cache->spotOuter[i];
			if (!cache->spotDirectionValid[i]) continue;
			spotCos = -(direction.x * spotDirection.x + direction.y * spotDirection.y + direction.z * spotDirection.z);
			if (spotCos <= outer) continue;
			if (spotCos < inner && inner > outer) {
				float spotScale = (spotCos - outer) / (inner - outer);
				if (!cache->fastTnl && entry.light.falloff > 0.0f) spotScale = (float)pow(spotScale, entry.light.falloff);
				attenuation *= spotScale;
			}
		}
		dot = normal.x * direction.x + normal.y * direction.y + normal.z * direction.z;
		if (dot < 0.0f) dot = 0.0f;
		r += (ambient.r * entry.light.ambient.r + diffuse.r * entry.light.diffuse.r * dot) * attenuation;
		g += (ambient.g * entry.light.ambient.g + diffuse.g * entry.light.diffuse.g * dot) * attenuation;
		b += (ambient.b * entry.light.ambient.b + diffuse.b * entry.light.diffuse.b * dot) * attenuation;
		if (dot > 0.0f && cache->specularEnabled) {
			NPDISP_D3DVECTOR32 halfVector = { direction.x + viewer.x, direction.y + viewer.y, direction.z + viewer.z };
			if (npdisp_d3d_normalize(&halfVector)) {
				float specDot = normal.x * halfVector.x + normal.y * halfVector.y + normal.z * halfVector.z;
				if (specDot > 0.0f) {
					float factor = (float)pow(specDot, tnl->material.power) * attenuation;
					sr += specular.r * entry.light.specular.r * factor;
					sg += specular.g * entry.light.specular.g * factor;
					sb += specular.b * entry.light.specular.b * factor;
				}
			}
		}
	}
	*diffuseColor = npdisp_d3d_packColor(diffuse.a, r, g, b);
	*specularColor = npdisp_d3d_packColor(1.0f, sr, sg, sb);
}

static bool npdisp_d3d_tnlFogFactor(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3DVECTOR32* viewPosition, bool fastTnl, float* factor)
{
	UINT32 mode;
	float distance;
	float value;
	if (!context || !viewPosition || !factor) return false;
	mode = context->renderStates[NPDISP_D3DRENDERSTATE_FOGVERTEXMODE];
	if (!context->renderStates[NPDISP_D3DRENDERSTATE_FOGENABLE] || mode == NPDISP_D3DFOG_NONE) return false;
	if (context->renderStates[NPDISP_D3DRENDERSTATE_RANGEFOGENABLE])
		distance = fastTnl ? (float)fabs(viewPosition->z) :
			(float)sqrt(viewPosition->x * viewPosition->x + viewPosition->y * viewPosition->y + viewPosition->z * viewPosition->z);
	else distance = viewPosition->z;
	if (distance < 0.0f) distance = 0.0f;
	if (mode == NPDISP_D3DFOG_LINEAR) {
		float start = npdisp_d3d_stateFloat(context->renderStates[NPDISP_D3DRENDERSTATE_FOGSTART]);
		float end = npdisp_d3d_stateFloat(context->renderStates[NPDISP_D3DRENDERSTATE_FOGEND]);
		float span = end - start;
		if ((float)fabs(span) <= 0.000001f) value = distance <= end ? 1.0f : 0.0f;
		else value = (end - distance) / span;
	}
	else if (mode == NPDISP_D3DFOG_EXP || mode == NPDISP_D3DFOG_EXP2) {
		float density = npdisp_d3d_stateFloat(context->renderStates[NPDISP_D3DRENDERSTATE_FOGDENSITY]);
		float d = density * distance;
		value = mode == NPDISP_D3DFOG_EXP ? (float)exp(-d) : (float)exp(-(d * d));
	}
	else return false;
	if (value < 0.0f) value = 0.0f;
	if (value > 1.0f) value = 1.0f;
	*factor = value;
	return true;
}

static void npdisp_d3d_tnlTextureVertex(const NPDISP_D3D_TNL_CACHE* cache, const NPDISP_D3D_TNL_CONTEXT* tnl,
	NPDISP_D3DTLVERTEX32* vertex, const NPDISP_D3DVECTOR32* viewPosition, NPDISP_D3DVECTOR32 viewNormal)
{
	NPDISP_D3DVECTOR32 eye = { 0.0f, 0.0f, 0.0f };
	NPDISP_D3DVECTOR32 reflectionNormal = { 0.0f, 0.0f, 0.0f };
	bool reflectionReady = false;
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		UINT32 index = cache->texCoordIndex[stage];
		UINT32 flags = cache->texTransformFlags[stage];
		UINT32 generated = index & NPDISP_D3DTSS_TCI_GENMASK;
		float input[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		if ((index & NPDISP_D3DTSS_TCI_INDEXMASK) < NPDISP_D3D_TEXTURE_COORD_SETS)
			memcpy(input, vertex->sourceTex[index & NPDISP_D3DTSS_TCI_INDEXMASK], sizeof(input));
		if (generated == 0x00010000UL) {
			input[0] = viewNormal.x; input[1] = viewNormal.y; input[2] = viewNormal.z;
		}
		else if (generated == 0x00020000UL) {
			input[0] = viewPosition->x; input[1] = viewPosition->y; input[2] = viewPosition->z;
		}
		else if (generated == 0x00030000UL) {
			float dot;
			if (!reflectionReady) {
				eye.x = -viewPosition->x; eye.y = -viewPosition->y; eye.z = -viewPosition->z;
				npdisp_d3d_normalize(&eye);
				reflectionNormal = viewNormal;
				if (!npdisp_d3d_normalize(&reflectionNormal)) reflectionNormal.z = 1.0f;
				reflectionReady = true;
			}
			dot = eye.x * reflectionNormal.x + eye.y * reflectionNormal.y + eye.z * reflectionNormal.z;
			input[0] = 2.0f * dot * reflectionNormal.x - eye.x;
			input[1] = 2.0f * dot * reflectionNormal.y - eye.y;
			input[2] = 2.0f * dot * reflectionNormal.z - eye.z;
		}
		if (flags & 7U) {
			for (UINT32 column = 0; column < 4U; ++column) {
				float result = 0.0f;
				for (UINT32 row = 0; row < 4U; ++row) result += input[row] * tnl->texture[stage].m[row][column];
				vertex->stageTex[stage][column] = result;
			}
		}
		else memcpy(vertex->stageTex[stage], input, sizeof(input));
	}
	vertex->stageTexValid = 1U;
}


static bool npdisp_d3d_tnlTransformVertex(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl,
	const NPDISP_D3D_TNL_CACHE* cache, NPDISP_D3DTLVERTEX32* vertex, const NPDISP_D3DVECTOR32* normal, const float* blendWeights,
	UINT32 weightCount, bool hasDiffuse, bool hasSpecular)
{
	NPDISP_D3DVECTOR32 source = { vertex->sx, vertex->sy, vertex->sz };
	NPDISP_D3DVECTOR32 worldPosition;
	NPDISP_D3DVECTOR32 viewPosition = { 0.0f, 0.0f, 0.0f };
	NPDISP_D3DVECTOR32 clipPosition;
	NPDISP_D3DVECTOR32 viewNormal = { 0.0f, 0.0f, 1.0f };
	float w;
	UINT32 blendCount = context->renderStates[NPDISP_D3DRENDERSTATE_VERTEXBLEND];
	if (blendCount > 3U || (blendCount && (weightCount < blendCount || !blendWeights))) return false;
	if (blendCount) {
		const NPDISP_D3DMATRIX32* matrices[4] = { &tnl->world, &tnl->worldBlend[0], &tnl->worldBlend[1], &tnl->worldBlend[2] };
		float lastWeight = 1.0f;
		worldPosition.x = worldPosition.y = worldPosition.z = 0.0f;
		if (normal && cache->needsNormal) viewNormal.x = viewNormal.y = viewNormal.z = 0.0f;
		for (UINT32 i = 0; i <= blendCount; ++i) {
			float weight = i < blendCount ? blendWeights[i] : lastWeight;
			NPDISP_D3DVECTOR32 point = npdisp_d3d_transformPoint(&source, matrices[i], NULL);
			worldPosition.x += point.x * weight;
			worldPosition.y += point.y * weight;
			worldPosition.z += point.z * weight;
			if (normal && cache->needsNormal) {
				NPDISP_D3DVECTOR32 transformed = npdisp_d3d_tnlCachedNormal(normal, &cache->normal[i]);
				viewNormal.x += transformed.x * weight;
				viewNormal.y += transformed.y * weight;
				viewNormal.z += transformed.z * weight;
			}
			if (i < blendCount) lastWeight -= weight;
		}
		if (normal && cache->needsNormal) viewNormal = npdisp_d3d_tnlCachedNormal(&viewNormal, &cache->normal[4]);
	}
	else if (cache->useWorldViewProjection)
		clipPosition = npdisp_d3d_transformPoint(&source, &cache->worldViewProjection, &w);
	else if (cache->useWorldView) viewPosition = npdisp_d3d_transformPoint(&source, &cache->worldView, NULL);
	else worldPosition = npdisp_d3d_transformPoint(&source, &tnl->world, NULL);
	if (!cache->useWorldViewProjection) {
		if (blendCount || !cache->useWorldView)
			viewPosition = npdisp_d3d_transformPoint(&worldPosition, &tnl->view, NULL);
		clipPosition = npdisp_d3d_transformPoint(&viewPosition, &tnl->projection, &w);
	}
	vertex->clipX = clipPosition.x;
	vertex->clipY = clipPosition.y;
	vertex->clipZ = clipPosition.z;
	vertex->clipW = w;
	vertex->clipValid = 1U;
	if (normal && cache->needsNormal && !blendCount) {
		NPDISP_D3DVECTOR32 worldNormal = npdisp_d3d_tnlCachedNormal(normal, &cache->normal[0]);
		viewNormal = npdisp_d3d_tnlCachedNormal(&worldNormal, &cache->normal[4]);
	}
	if (normal && cache->needsNormal && !cache->fastTnl && context->renderStates[NPDISP_D3DRENDERSTATE_NORMALIZENORMALS] && !npdisp_d3d_normalize(&viewNormal)) {
		viewNormal.x = viewNormal.y = 0.0f;
		viewNormal.z = 1.0f;
	}
	npdisp_d3d_tnlTextureVertex(cache, tnl, vertex, &viewPosition, viewNormal);
	if (context->renderStates[NPDISP_D3DRENDERSTATE_LIGHTING])
		npdisp_d3d_tnlLightVertex(context, tnl, cache, &viewPosition, viewNormal, vertex->color, vertex->specular,
			hasDiffuse, hasSpecular, &vertex->color, &vertex->specular);
	else if (!hasDiffuse) vertex->color = 0xffffffffUL;
	{
		float fogFactor;
		if (npdisp_d3d_tnlFogFactor(context, &viewPosition, cache->fastTnl, &fogFactor)) {
			UINT32 fog = (UINT32)(fogFactor * 255.0f + 0.5f);
			if (fog > 255U) fog = 255U;
			vertex->specular = (vertex->specular & 0x00ffffffUL) | (fog << 24);
		}
	}
	if (w <= 0.000001f) {
		vertex->sx = vertex->sy = vertex->sz = vertex->rhw = 0.0f;
		return true;
	}
	vertex->rhw = 1.0f / w;
	vertex->sx = (float)context->viewportX + (1.0f + clipPosition.x * vertex->rhw) * (float)context->viewportWidth * 0.5f;
	vertex->sy = (float)context->viewportY + (1.0f - clipPosition.y * vertex->rhw) * (float)context->viewportHeight * 0.5f;
	vertex->sz = context->zMin + clipPosition.z * vertex->rhw * (context->zMax - context->zMin);
	return true;
}

static UINT32 npdisp_d3d_fvfWeightCount(UINT32 position)
{
	if (position == NPDISP_D3DFVF_XYZB1) return 1U;
	if (position == NPDISP_D3DFVF_XYZB2) return 2U;
	if (position == NPDISP_D3DFVF_XYZB3) return 3U;
	return 0U;
}

static bool npdisp_d3d_dp2VertexSize(UINT32 fvf, UINT32* vertexSize)
{
	UINT32 position = fvf & NPDISP_D3DFVF_POSITION_MASK;
	UINT32 size;
	UINT32 texCount;
	if (!vertexSize || (position != NPDISP_D3DFVF_XYZRHW && position != NPDISP_D3DFVF_XYZ &&
		position != NPDISP_D3DFVF_XYZB1 && position != NPDISP_D3DFVF_XYZB2 && position != NPDISP_D3DFVF_XYZB3) ||
		(fvf & (NPDISP_D3DFVF_RESERVED0 | NPDISP_D3DFVF_RESERVED2)) ||
		(position == NPDISP_D3DFVF_XYZRHW && (fvf & (NPDISP_D3DFVF_NORMAL | NPDISP_D3DFVF_RESERVED1)))) return false;
	size = (position == NPDISP_D3DFVF_XYZRHW ? 16U : 12U) + npdisp_d3d_fvfWeightCount(position) * 4U;
	if (fvf & NPDISP_D3DFVF_RESERVED1) size += 4U;
	if (fvf & NPDISP_D3DFVF_NORMAL) size += 12U;
	texCount = (fvf & NPDISP_D3DFVF_TEXCOUNT_MASK) >> 8;
	if (texCount > NPDISP_D3D_TEXTURE_COORD_SETS) return false;
	for (UINT32 i = texCount; i < NPDISP_D3D_TEXTURE_COORD_SETS; ++i) {
		if ((fvf >> (16U + i * 2U)) & 3U) return false;
	}
	if (fvf & NPDISP_D3DFVF_DIFFUSE) size += 4U;
	if (fvf & NPDISP_D3DFVF_SPECULAR) size += 4U;
	for (UINT32 i = 0; i < texCount; ++i) {
		UINT32 format = (fvf >> (16U + i * 2U)) & 3U;
		UINT32 dimensions = format == 3U ? 1U : format + 2U;
		if (size > NPDISP_D3D_MAX_VERTEX_SIZE - dimensions * 4U) return false;
		size += dimensions * 4U;
	}
	*vertexSize = size;
	return true;
}

static bool npdisp_d3d_dp2VertexFormat(UINT32 fvf, UINT32 vertexSize)
{
	UINT32 minSize;
	return vertexSize >= 12U && vertexSize <= NPDISP_D3D_MAX_VERTEX_SIZE && npdisp_d3d_dp2VertexSize(fvf, &minSize) && vertexSize >= minSize;
}

static bool npdisp_d3d_dp2VertexBufferRead(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 offset, void* dst, UINT32 size)
{
	if (!data || !dst || !size) return false;
	if (data->dwFlags & NPDISP_D3DHALDP2_W2K_DIRECTBUFFERS)
		return npdisp_d3d_rawBufferRead(data->lpDDVertexOrVertices, offset, dst, size);
	return npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, offset, dst, size);
}

static void npdisp_d3d_dp2SetTexCoord(NPDISP_D3DTLVERTEX32* vertex, UINT32 index, const float values[4])
{
	memcpy(vertex->sourceTex[index], values, sizeof(vertex->sourceTex[index]));
	if (index == 0U) {
		vertex->tu = values[0]; vertex->tv = values[1]; vertex->tw = values[2]; vertex->tq = values[3];
	}
	else if (index == 1U) {
		vertex->tu2 = values[0]; vertex->tv2 = values[1]; vertex->tw2 = values[2]; vertex->tq2 = values[3];
	}
}

static bool npdisp_d3d_dp2DecodeVertex(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl,
	const NPDISP_D3D_TNL_CACHE* cache, UINT32 fvf, const UINT8* bytes, NPDISP_D3DTLVERTEX32* vertex)
{
	UINT32 position = fvf & NPDISP_D3DFVF_POSITION_MASK;
	bool transformed = position == NPDISP_D3DFVF_XYZRHW;
	bool hasNormal = (fvf & NPDISP_D3DFVF_NORMAL) != 0;
	UINT32 positionSize = transformed ? 16U : 12U;
	UINT32 weightCount = npdisp_d3d_fvfWeightCount(position);
	UINT32 attributeOffset = positionSize + weightCount * 4U;
	UINT32 texCount = (fvf & NPDISP_D3DFVF_TEXCOUNT_MASK) >> 8;
	float blendWeights[3] = { 0.0f, 0.0f, 0.0f };
	NPDISP_D3DVECTOR32 normal = { 0.0f, 0.0f, 1.0f };
	if (fvf & NPDISP_D3DFVF_RESERVED1) attributeOffset += 4U;
	*vertex = NPDISP_D3DTLVERTEX32();
	vertex->rhw = 1.0f;
	vertex->tq = vertex->tq2 = 1.0f;
	for (UINT32 coord = 0; coord < NPDISP_D3D_TEXTURE_COORD_SETS; ++coord) vertex->sourceTex[coord][3] = 1.0f;
	vertex->color = 0xffffffffUL;
	vertex->specular = 0xff000000UL;
	memcpy(vertex, bytes, positionSize);
	if (weightCount) memcpy(blendWeights, bytes + positionSize, weightCount * 4U);
	if (hasNormal) {
		memcpy(&normal, bytes + attributeOffset, sizeof(normal));
		attributeOffset += sizeof(normal);
	}
	if (fvf & NPDISP_D3DFVF_DIFFUSE) {
		memcpy(&vertex->color, bytes + attributeOffset, sizeof(vertex->color));
		attributeOffset += 4U;
	}
	if (fvf & NPDISP_D3DFVF_SPECULAR) {
		memcpy(&vertex->specular, bytes + attributeOffset, sizeof(vertex->specular));
		attributeOffset += 4U;
	}
	for (UINT32 i = 0; i < texCount; ++i) {
		UINT32 format = (fvf >> (16U + i * 2U)) & 3U;
		UINT32 dimensions = format == 3U ? 1U : format + 2U;
		float values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		memcpy(values, bytes + attributeOffset, dimensions * 4U);
		npdisp_d3d_dp2SetTexCoord(vertex, i, values);
		attributeOffset += dimensions * 4U;
	}
	if (!transformed && !npdisp_d3d_tnlTransformVertex(context, tnl, cache, vertex, hasNormal ? &normal : NULL, blendWeights, weightCount,
		(fvf & NPDISP_D3DFVF_DIFFUSE) != 0, (fvf & NPDISP_D3DFVF_SPECULAR) != 0)) return false;
	if (transformed) {
		NPDISP_D3DVECTOR32 viewPosition = { 0.0f, 0.0f, 0.0f };
		npdisp_d3d_tnlTextureVertex(cache, tnl, vertex, &viewPosition, normal);
	}
	return true;
}

static bool npdisp_d3d_dp2VertexRead(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl, const NPDISP_D3D_TNL_CACHE* cache,
	const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 readSize, UINT32 index, NPDISP_D3DTLVERTEX32* vertex)
{
	UINT8 bytes[NPDISP_D3D_MAX_VERTEX_SIZE];
	UINT64 byteOffset;
	UINT32 offset;
	if (!context || !tnl || !cache || !data || !vertex || !readSize || readSize > sizeof(bytes) ||
		index >= data->dwVertexLength) return false;
	byteOffset = (UINT64)data->dwVertexOffset + (UINT64)index * vertexSize;
	if (byteOffset > 0xffffffffULL || byteOffset + readSize > 0x100000000ULL) return false;
	offset = (UINT32)byteOffset;
	if (data->dwFlags & NPDISP_D3DHALDP2_USERMEMVERTICES) {
		UINT32 addr;
		if (!npdisp_d3d_add(data->lpDDVertexOrVertices, offset, &addr) ||
			(UINT64)addr + readSize > 0x100000000ULL || !npdisp_d3d_read(bytes, addr, readSize)) return false;
	}
	else if (!npdisp_d3d_dp2VertexBufferRead(data, offset, bytes, readSize)) return false;
	return npdisp_d3d_dp2DecodeVertex(context, tnl, cache, data->dwVertexType, bytes, vertex);
}

static bool npdisp_d3d_dp2VerticesRead(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl,
	const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 first, UINT32 count, std::vector<NPDISP_D3DTLVERTEX32>* vertices)
{
	UINT64 end;
	UINT32 readSize;
	NPDISP_D3D_TNL_CACHE cache;
	if (!context || !tnl || !data || !vertices || count > NPDISP_D3D_MAX_VERTICES) return false;
	end = (UINT64)first + count;
	if (end > data->dwVertexLength) return false;
	if (count && !npdisp_d3d_dp2VertexSize(data->dwVertexType, &readSize)) return false;
	vertices->resize(count);
	if (count) npdisp_d3d_tnlBuildCache(context, tnl, data->dwVertexType, &cache);
	for (UINT32 i = 0; i < count; ++i) if (!npdisp_d3d_dp2VertexRead(context, tnl, &cache, data, vertexSize, readSize, first + i, &(*vertices)[i])) return false;
	return true;
}

static bool npdisp_d3d_dp2CommandVertexRead(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl, const NPDISP_D3D_TNL_CACHE* cache,
	const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 offset, UINT32 readSize, NPDISP_D3DTLVERTEX32* vertex)
{
	UINT8 bytes[NPDISP_D3D_MAX_VERTEX_SIZE];
	if (!context || !tnl || !cache || !data || !vertex || !readSize || readSize > sizeof(bytes) ||
		!npdisp_d3d_dp2CommandRead(data, offset, bytes, readSize)) return false;
	return npdisp_d3d_dp2DecodeVertex(context, tnl, cache, data->dwVertexType, bytes, vertex);
}

static bool npdisp_d3d_dp2AlignImmediateCursor(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 cursor, UINT32* alignedCursor)
{
	UINT64 absolute;
	UINT64 aligned;
	if (!data || !alignedCursor) return false;
	absolute = (UINT64)data->dwCommandOffset + cursor;
	aligned = (absolute + 3ULL) & ~3ULL;
	if (aligned < data->dwCommandOffset || aligned - data->dwCommandOffset > data->dwCommandLength) return false;
	*alignedCursor = (UINT32)(aligned - data->dwCommandOffset);
	return true;
}

static bool npdisp_d3d_dp2ImmediateVerticesRead(const NPDISP_D3D_CONTEXT* context, const NPDISP_D3D_TNL_CONTEXT* tnl,
	const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 offset, UINT32 count, std::vector<NPDISP_D3DTLVERTEX32>* vertices)
{
	UINT64 bytes64;
	UINT32 readSize;
	NPDISP_D3D_TNL_CACHE cache;
	if (!context || !tnl || !data || !vertices || count > NPDISP_D3D_MAX_VERTICES) return false;
	bytes64 = (UINT64)vertexSize * count;
	if (bytes64 > 0xffffffffULL || (UINT64)offset + bytes64 > data->dwCommandLength) return false;
	if (count && !npdisp_d3d_dp2VertexSize(data->dwVertexType, &readSize)) return false;
	vertices->resize(count);
	if (count) npdisp_d3d_tnlBuildCache(context, tnl, data->dwVertexType, &cache);
	for (UINT32 i = 0; i < count; ++i) {
		UINT64 vertexOffset = (UINT64)offset + (UINT64)i * vertexSize;
		if (vertexOffset > 0xffffffffULL || !npdisp_d3d_dp2CommandVertexRead(context, tnl, &cache, data, (UINT32)vertexOffset, readSize, &(*vertices)[i])) return false;
	}
	return true;
}

static bool npdisp_d3d_dp2IndexedTriangleList(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 command, UINT32 primitiveCount, UINT32* cursor)
{
	NPDISP_D3DHAL_DP2STARTVERTEX32 start = { 0 };
	std::vector<NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST32> triangles;
	std::vector<NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST2_32> triangles2;
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	std::vector<UINT16> indices;
	UINT64 bytes64;
	UINT32 maxIndex = 0;
	if (!context || !data || !cursor) return false;
	if (!npdisp_d3d_dp2VertexFormat(data->dwVertexType, vertexSize)) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=fvf op=%u fvf=%08x vsize=%u", command, data->dwVertexType, vertexSize));
		return false;
	}
	if (command == NPDISP_D3DDP2OP_INDEXEDTRIANGLELIST2) {
		if (!npdisp_d3d_dp2CommandRead(data, *cursor, &start, sizeof(start)) || !npdisp_d3d_add(*cursor, sizeof(start), cursor)) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=start op=%u", command));
			return false;
		}
	}
	if (!primitiveCount) return true;
	if (primitiveCount > NPDISP_D3D_MAX_VERTICES / 3U) return false;
	indices.resize(primitiveCount * 3U);
	if (command == NPDISP_D3DDP2OP_INDEXEDTRIANGLELIST2) {
		bytes64 = (UINT64)primitiveCount * sizeof(NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST2_32);
		if (bytes64 > 0xffffffffULL || (UINT64)*cursor + bytes64 > data->dwCommandLength) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=payload op=%u count=%u", command, primitiveCount));
			return false;
		}
		triangles2.resize(primitiveCount);
		if (!npdisp_d3d_dp2CommandRead(data, *cursor, &triangles2[0], (UINT32)bytes64)) return false;
		for (UINT32 i = 0; i < primitiveCount; ++i) {
			UINT32 base = i * 3U;
			indices[base] = triangles2[i].wV1;
			indices[base + 1U] = triangles2[i].wV2;
			indices[base + 2U] = triangles2[i].wV3;
			if (triangles2[i].wV1 > maxIndex) maxIndex = triangles2[i].wV1;
			if (triangles2[i].wV2 > maxIndex) maxIndex = triangles2[i].wV2;
			if (triangles2[i].wV3 > maxIndex) maxIndex = triangles2[i].wV3;
		}
	}
	else {
		bytes64 = (UINT64)primitiveCount * sizeof(NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST32);
		if (bytes64 > 0xffffffffULL || (UINT64)*cursor + bytes64 > data->dwCommandLength) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=payload op=%u count=%u", command, primitiveCount));
			return false;
		}
		triangles.resize(primitiveCount);
		if (!npdisp_d3d_dp2CommandRead(data, *cursor, &triangles[0], (UINT32)bytes64)) return false;
		for (UINT32 i = 0; i < primitiveCount; ++i) {
			UINT32 base = i * 3U;
			indices[base] = triangles[i].wV1;
			indices[base + 1U] = triangles[i].wV2;
			indices[base + 2U] = triangles[i].wV3;
			if (triangles[i].wV1 > maxIndex) maxIndex = triangles[i].wV1;
			if (triangles[i].wV2 > maxIndex) maxIndex = triangles[i].wV2;
			if (triangles[i].wV3 > maxIndex) maxIndex = triangles[i].wV3;
		}
	}
	if ((UINT64)start.wVStart + maxIndex + 1U > data->dwVertexLength || maxIndex >= NPDISP_D3D_MAX_VERTICES) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=range op=%u base=%u max=%u vlen=%u", command, start.wVStart, maxIndex, data->dwVertexLength));
		return false;
	}
	if (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data->dwhContext), data, vertexSize, start.wVStart, maxIndex + 1U, &vertices)) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=vertices op=%u base=%u max=%u", command, start.wVStart, maxIndex));
		return false;
	}
	if ((data->dwFlags & NPDISP_D3DHALDP2_EXECUTEBUFFER) && context->renderStates[NPDISP_D3DRENDERSTATE_ALPHABLENDENABLE] && !triangles.empty()) {
		UINT32 alphaMin = 255U, alphaMax = 0U, positive = 0U, negative = 0U, zero = 0U;
		UINT32 startCount = 0U, oddCount = 0U, evenCount = 0U, flatCount = 0U;
		float zMin = vertices[indices[0]].sz, zMax = zMin;
		for (UINT32 i = 0; i < (UINT32)indices.size(); ++i) {
			const NPDISP_D3DTLVERTEX32& v = vertices[indices[i]];
			UINT32 alpha = (v.color >> 24) & 0xffU;
			if (alpha < alphaMin) alphaMin = alpha;
			if (alpha > alphaMax) alphaMax = alpha;
			if (v.sz < zMin) zMin = v.sz;
			if (v.sz > zMax) zMax = v.sz;
		}
		for (UINT32 i = 0; i < primitiveCount; ++i) {
			const NPDISP_D3DTLVERTEX32& v0 = vertices[triangles[i].wV1];
			const NPDISP_D3DTLVERTEX32& v1 = vertices[triangles[i].wV2];
			const NPDISP_D3DTLVERTEX32& v2 = vertices[triangles[i].wV3];
			float area = (v2.sx - v0.sx) * (v1.sy - v0.sy) - (v2.sy - v0.sy) * (v1.sx - v0.sx);
			UINT32 continuation = triangles[i].wFlags & 0x1fU;
			if (area > 0.0f) ++positive; else if (area < 0.0f) ++negative; else ++zero;
			if (continuation == NPDISP_D3DTRIFLAG_START) ++startCount;
			else if (continuation == NPDISP_D3DTRIFLAG_ODD) ++oddCount;
			else if (continuation == NPDISP_D3DTRIFLAG_EVEN) ++evenCount;
			else ++flatCount;
		}
		UINT32 zMinBits = 0U, zMaxBits = 0U;
		memcpy(&zMinBits, &zMin, sizeof(zMinBits));
		memcpy(&zMaxBits, &zMax, sizeof(zMaxBits));
		TRACEOUTD3D(("NPDISP11 D3D_ALPHA_DRAW count=%u amin=%u amax=%u zmin=%08x zmax=%08x pos=%u neg=%u zero=%u start=%u odd=%u even=%u flat=%u cull=%u zfunc=%u zwrite=%u", primitiveCount, alphaMin, alphaMax, zMinBits, zMaxBits, positive, negative, zero, startCount, oddCount, evenCount, flatCount, context->renderStates[NPDISP_D3DRENDERSTATE_CULLMODE], context->renderStates[NPDISP_D3DRENDERSTATE_ZFUNC], context->renderStates[NPDISP_D3DRENDERSTATE_ZWRITEENABLE]));
	}
	if (!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_TRIANGLELIST, &vertices[0], maxIndex + 1U, &indices[0], (UINT32)indices.size())) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST_REJECT stage=draw op=%u tex=%08x legacytex=%08x z=%08x at=%08x ab=%08x fog=%08x spec=%08x fill=%08x shade=%08x cull=%08x", command, context->textureStageStates[0][NPDISP_D3DTSS_TEXTUREMAP], context->renderStates[NPDISP_D3DRENDERSTATE_TEXTUREHANDLE], context->renderStates[NPDISP_D3DRENDERSTATE_ZENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_ALPHATESTENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_ALPHABLENDENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_FOGENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_SPECULARENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_FILLMODE], context->renderStates[NPDISP_D3DRENDERSTATE_SHADEMODE], context->renderStates[NPDISP_D3DRENDERSTATE_CULLMODE]));
		return false;
	}
	*cursor += (UINT32)bytes64;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRILIST ctx=%08x op=%u count=%u base=%u max=%u", data->dwhContext, command, primitiveCount, start.wVStart, maxIndex));
	return true;
}

static bool npdisp_d3d_dp2IndexedTriangleSequence(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 command, UINT32 primitiveCount, UINT32* cursor)
{
	NPDISP_D3DHAL_DP2STARTVERTEX32 start = { 0 };
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	std::vector<UINT16> indices;
	UINT32 primitiveType;
	UINT32 indexCount;
	UINT32 maxIndex = 0;
	UINT64 bytes64;
	if (!context || !data || !cursor ||
		(command != NPDISP_D3DDP2OP_INDEXEDTRIANGLESTRIP && command != NPDISP_D3DDP2OP_INDEXEDTRIANGLEFAN)) return false;
	if (!npdisp_d3d_dp2VertexFormat(data->dwVertexType, vertexSize)) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ_REJECT stage=fvf op=%u fvf=%08x vsize=%u", command, data->dwVertexType, vertexSize));
		return false;
	}
	if (!npdisp_d3d_dp2CommandRead(data, *cursor, &start, sizeof(start)) || !npdisp_d3d_add(*cursor, sizeof(start), cursor)) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ_REJECT stage=start op=%u", command));
		return false;
	}
	if (!primitiveCount) return true;
	if (primitiveCount > NPDISP_D3D_MAX_VERTICES - 2U) return false;
	indexCount = primitiveCount + 2U;
	bytes64 = (UINT64)indexCount * sizeof(UINT16);
	if (bytes64 > 0xffffffffULL || (UINT64)*cursor + bytes64 > data->dwCommandLength) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ_REJECT stage=payload op=%u count=%u", command, primitiveCount));
		return false;
	}
	indices.resize(indexCount);
	if (!npdisp_d3d_dp2CommandRead(data, *cursor, &indices[0], (UINT32)bytes64)) return false;
	for (UINT32 i = 0; i < indexCount; ++i) if (indices[i] > maxIndex) maxIndex = indices[i];
	if ((UINT64)start.wVStart + maxIndex + 1U > data->dwVertexLength || maxIndex >= NPDISP_D3D_MAX_VERTICES) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ_REJECT stage=range op=%u base=%u max=%u vlen=%u", command, start.wVStart, maxIndex, data->dwVertexLength));
		return false;
	}
	if (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data->dwhContext), data, vertexSize, start.wVStart, maxIndex + 1U, &vertices)) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ_REJECT stage=vertices op=%u base=%u max=%u", command, start.wVStart, maxIndex));
		return false;
	}
	primitiveType = command == NPDISP_D3DDP2OP_INDEXEDTRIANGLESTRIP ? NPDISP_D3DPT_TRIANGLESTRIP : NPDISP_D3DPT_TRIANGLEFAN;
	if (!npdisp_d3d_drawPrimitive(context, primitiveType, &vertices[0], maxIndex + 1U, &indices[0], indexCount)) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ_REJECT stage=draw op=%u tex=%08x legacytex=%08x z=%08x at=%08x ab=%08x fog=%08x spec=%08x fill=%08x shade=%08x cull=%08x", command, context->textureStageStates[0][NPDISP_D3DTSS_TEXTUREMAP], context->renderStates[NPDISP_D3DRENDERSTATE_TEXTUREHANDLE], context->renderStates[NPDISP_D3DRENDERSTATE_ZENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_ALPHATESTENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_ALPHABLENDENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_FOGENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_SPECULARENABLE], context->renderStates[NPDISP_D3DRENDERSTATE_FILLMODE], context->renderStates[NPDISP_D3DRENDERSTATE_SHADEMODE], context->renderStates[NPDISP_D3DRENDERSTATE_CULLMODE]));
		return false;
	}
	*cursor += (UINT32)bytes64;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_ITRISEQ ctx=%08x op=%u count=%u base=%u max=%u", data->dwhContext, command, primitiveCount, start.wVStart, maxIndex));
	return true;
}

static bool npdisp_d3d_dp2IndexedLineList(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 command, UINT32 primitiveCount, UINT32* cursor)
{
	NPDISP_D3DHAL_DP2STARTVERTEX32 start = { 0 };
	std::vector<NPDISP_D3DHAL_DP2INDEXEDLINELIST32> lines;
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	std::vector<UINT16> indices;
	UINT64 bytes64;
	UINT32 maxIndex = 0;
	if (!context || !data || !cursor ||
		(command != NPDISP_D3DDP2OP_INDEXEDLINELIST && command != NPDISP_D3DDP2OP_INDEXEDLINELIST2)) return false;
	if (!npdisp_d3d_dp2VertexFormat(data->dwVertexType, vertexSize)) return false;
	if (command == NPDISP_D3DDP2OP_INDEXEDLINELIST2) {
		if (!npdisp_d3d_dp2CommandRead(data, *cursor, &start, sizeof(start)) || !npdisp_d3d_add(*cursor, sizeof(start), cursor)) return false;
	}
	if (!primitiveCount) return true;
	if (primitiveCount > NPDISP_D3D_MAX_VERTICES / 2U) return false;
	bytes64 = (UINT64)primitiveCount * sizeof(NPDISP_D3DHAL_DP2INDEXEDLINELIST32);
	if (bytes64 > 0xffffffffULL || (UINT64)*cursor + bytes64 > data->dwCommandLength) return false;
	lines.resize(primitiveCount);
	indices.resize(primitiveCount * 2U);
	if (!npdisp_d3d_dp2CommandRead(data, *cursor, &lines[0], (UINT32)bytes64)) return false;
	for (UINT32 i = 0; i < primitiveCount; ++i) {
		UINT32 base = i * 2U;
		indices[base] = lines[i].wV1;
		indices[base + 1U] = lines[i].wV2;
		if (lines[i].wV1 > maxIndex) maxIndex = lines[i].wV1;
		if (lines[i].wV2 > maxIndex) maxIndex = lines[i].wV2;
	}
	if ((UINT64)start.wVStart + maxIndex + 1U > data->dwVertexLength || maxIndex >= NPDISP_D3D_MAX_VERTICES) return false;
	if (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data->dwhContext), data, vertexSize, start.wVStart, maxIndex + 1U, &vertices) ||
		!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_LINELIST, &vertices[0], maxIndex + 1U, &indices[0], (UINT32)indices.size())) return false;
	*cursor += (UINT32)bytes64;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_ILINELIST ctx=%08x op=%u count=%u base=%u max=%u", data->dwhContext, command, primitiveCount, start.wVStart, maxIndex));
	return true;
}

static bool npdisp_d3d_dp2IndexedLineStrip(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 primitiveCount, UINT32* cursor)
{
	NPDISP_D3DHAL_DP2STARTVERTEX32 start = { 0 };
	std::vector<NPDISP_D3DTLVERTEX32> vertices;
	std::vector<UINT16> indices;
	UINT32 indexCount;
	UINT32 maxIndex = 0;
	UINT64 bytes64;
	if (!context || !data || !cursor || !npdisp_d3d_dp2VertexFormat(data->dwVertexType, vertexSize)) return false;
	if (!npdisp_d3d_dp2CommandRead(data, *cursor, &start, sizeof(start)) || !npdisp_d3d_add(*cursor, sizeof(start), cursor)) return false;
	if (!primitiveCount) return true;
	if (primitiveCount >= NPDISP_D3D_MAX_VERTICES) return false;
	indexCount = primitiveCount + 1U;
	bytes64 = (UINT64)indexCount * sizeof(UINT16);
	if (bytes64 > 0xffffffffULL || (UINT64)*cursor + bytes64 > data->dwCommandLength) return false;
	indices.resize(indexCount);
	if (!npdisp_d3d_dp2CommandRead(data, *cursor, &indices[0], (UINT32)bytes64)) return false;
	for (UINT32 i = 0; i < indexCount; ++i) if (indices[i] > maxIndex) maxIndex = indices[i];
	if ((UINT64)start.wVStart + maxIndex + 1U > data->dwVertexLength || maxIndex >= NPDISP_D3D_MAX_VERTICES) return false;
	if (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data->dwhContext), data, vertexSize, start.wVStart, maxIndex + 1U, &vertices) ||
		!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_LINESTRIP, &vertices[0], maxIndex + 1U, &indices[0], indexCount)) return false;
	*cursor += (UINT32)bytes64;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_ILINESTRIP ctx=%08x count=%u base=%u max=%u", data->dwhContext, primitiveCount, start.wVStart, maxIndex));
	return true;
}

static bool npdisp_d3d_attachedSurfaceListByHandle(UINT32 listAddr, UINT32 handle, UINT32* lpSurface)
{
	for (UINT32 i = 0; listAddr && i < 64U; ++i) {
		NPDISP_DDATTACHLIST list = { 0 };
		UINT32 candidateHandle = 0;
		if (!npdisp_d3d_read(&list, listAddr, sizeof(list))) return false;
		if (list.lpAttached && npdisp_d3d_surfaceHandleFromLocal(list.lpAttached, &candidateHandle)) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_SURFACE_ATTACH_SEEN want=%08x got=%08x surf=%08x", handle, candidateHandle, list.lpAttached));
			if (candidateHandle == handle) {
				*lpSurface = list.lpAttached;
				return true;
			}
		}
		if (list.lpIAttached) {
			NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
			candidateHandle = 0;
			if (npdisp_d3d_read(&surfaceInt, list.lpIAttached, sizeof(surfaceInt)) && surfaceInt.lpLcl &&
				npdisp_d3d_surfaceHandleFromLocal(surfaceInt.lpLcl, &candidateHandle)) {
				TRACEOUTD3D(("NPDISP11 D3D_DP2_SURFACE_IATTACH_SEEN want=%08x got=%08x surf=%08x", handle, candidateHandle, surfaceInt.lpLcl));
				if (candidateHandle == handle) {
					*lpSurface = surfaceInt.lpLcl;
					return true;
				}
			}
		}
		listAddr = list.lpLink;
	}
	return false;
}

static bool npdisp_d3d_attachedSurfaceByHandle(NPDISP_D3D_CONTEXT* context, UINT32 handle, UINT32* lpSurface)
{
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it;
	if (!context || !context->ddLocalNamespace || !handle || !lpSurface) return false;
	for (it = npdisp_d3d_surfaces.begin(); it != npdisp_d3d_surfaces.end(); ++it) {
		NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
		UINT32 candidate = 0;
		if (it->second.ddLocalNamespace != context->ddLocalNamespace || !it->second.lpSurface ||
			!npdisp_d3d_read(&lcl, it->second.lpSurface, sizeof(lcl))) continue;
		TRACEOUTD3D(("NPDISP11 D3D_DP2_SURFACE_ATTACH_SCAN ns=%08x want=%08x base=%08x list=%08x from=%08x",
			context->ddLocalNamespace, handle, it->second.lpSurface, lcl.lpAttachList, lcl.lpAttachListFrom));
		if ((!npdisp_d3d_attachedSurfaceListByHandle(lcl.lpAttachList, handle, &candidate) &&
			!npdisp_d3d_attachedSurfaceListByHandle(lcl.lpAttachListFrom, handle, &candidate)) || !candidate) continue;
		NPDISP_D3D_SURFACE surface = { 0 };
		surface.ddLocalNamespace = context->ddLocalNamespace;
		surface.handle = handle;
		surface.lpSurface = candidate;
		npdisp_d3d_surfaceCaps2(surface.lpSurface, &surface.caps2);
		npdisp_d3d_captureTargetSnapshot(surface.lpSurface, &surface.targetSnapshot);
		npdisp_d3d_captureTextureSnapshot(surface.lpSurface, &surface.textureSnapshot);
		npdisp_d3d_surfaces[npdisp_d3d_surfaceKey(context->ddLocalNamespace, handle)] = surface;
		*lpSurface = candidate;
		TRACEOUTD3D(("NPDISP11 D3D_DP2_SURFACE_ATTACH_BIND ns=%08x handle=%08x surf=%08x base=%08x target=%u off=%08x",
			context->ddLocalNamespace, handle, candidate, it->second.lpSurface, surface.targetSnapshot.valid ? 1U : 0U, surface.targetSnapshot.apertureOffset));
		return true;
	}
	return false;
}

static bool npdisp_d3d_attachedSurfaceListByCaps(UINT32 listAddr, UINT32 caps, UINT32* lpSurface)
{
	if (!lpSurface || !caps) return false;
	*lpSurface = 0;
	for (UINT32 i = 0; listAddr && i < 64U; ++i) {
		NPDISP_DDATTACHLIST list = { 0 };
		if (!npdisp_d3d_read(&list, listAddr, sizeof(list))) return false;
		if (list.lpAttached) {
			NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
			if (npdisp_d3d_read(&lcl, list.lpAttached, sizeof(lcl)) && (lcl.ddsCaps.dwCaps & caps) == caps) {
				*lpSurface = list.lpAttached;
				return true;
			}
		}
		if (list.lpIAttached) {
			NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
			NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
			if (npdisp_d3d_read(&surfaceInt, list.lpIAttached, sizeof(surfaceInt)) && surfaceInt.lpLcl &&
				npdisp_d3d_read(&lcl, surfaceInt.lpLcl, sizeof(lcl)) && (lcl.ddsCaps.dwCaps & caps) == caps) {
				*lpSurface = surfaceInt.lpLcl;
				return true;
			}
		}
		listAddr = list.lpLink;
	}
	return false;
}

static bool npdisp_d3d_attachedSurfaceByCaps(UINT32 lpSurface, UINT32 caps, UINT32* lpAttachedSurface)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	if (!lpSurface || !caps || !lpAttachedSurface || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl))) return false;
	return npdisp_d3d_attachedSurfaceListByCaps(lcl.lpAttachList, caps, lpAttachedSurface) ||
		npdisp_d3d_attachedSurfaceListByCaps(lcl.lpAttachListFrom, caps, lpAttachedSurface);
}

static bool npdisp_d3d_dp2SurfaceByHandle(NPDISP_D3D_CONTEXT* context, UINT32 handle, UINT32* lpSurface)
{
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it;
	if (!context || !lpSurface) return false;
	*lpSurface = 0;
	if (!handle) return true;
	if (!context->ddLocalNamespace && !npdisp_d3d_bindDDLocalNamespaceByHandle(context, handle)) return false;
	it = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, handle));
	if (it == npdisp_d3d_surfaces.end()) {
		if (npdisp_d3d_attachedSurfaceByHandle(context, handle, lpSurface)) return true;
		TRACEOUTD3D(("NPDISP11 D3D_DP2_SURFACE_MISS ns=%08x handle=%08x", context->ddLocalNamespace, handle));
		return false;
	}
	*lpSurface = it->second.lpSurface;
	return true;
}

static bool npdisp_d3d_dp2SetRenderTarget(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DP2SETRENDERTARGET32* setTarget)
{
	NPDISP_D3D_TARGET target;
	NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it;
	UINT32 lpTarget;
	UINT32 lpDepth;
	if (!context || !setTarget) return false;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_SETRENDERTARGET_IN rt=%08x z=%08x ns=%08x", setTarget->hRenderTarget, setTarget->hZBuffer, context->ddLocalNamespace));
	if (!npdisp_d3d_dp2SurfaceByHandle(context, setTarget->hRenderTarget, &lpTarget) ||
		!npdisp_d3d_dp2SurfaceByHandle(context, setTarget->hZBuffer, &lpDepth) || !lpTarget) return false;
	it = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, setTarget->hRenderTarget));
	if (it == npdisp_d3d_surfaces.end() || !it->second.targetSnapshot.valid) return false;
	if (!it->second.targetSnapshot.systemMemory && !npdisp_d3d_targetFromSnapshot(&it->second.targetSnapshot, &target)) return false;
	if (!lpDepth && npdisp_d3d_attachedSurfaceByCaps(lpTarget, NPDISP_DDSCAPS_ZBUFFER, &lpDepth))
		TRACEOUTD3D(("NPDISP11 D3D_DP2_SETRENDERTARGET_ZATTACH rt=%08x z=%08x", lpTarget, lpDepth));
	if (lpDepth && (!npdisp_d3d_depthTarget(lpDepth, &depthTarget) || depthTarget.width < it->second.targetSnapshot.width || depthTarget.height < it->second.targetSnapshot.height)) return false;
	context->lpTarget = lpTarget;
	context->lpDepth = lpDepth;
	context->targetSnapshot = it->second.targetSnapshot;
	TRACEOUTD3D(("NPDISP11 D3D_DP2_SETRENDERTARGET_OK rt=%08x z=%08x ns=%08x off=%08x visible=%u sys=%u size=%ux%u", lpTarget, lpDepth, context->ddLocalNamespace,
		it->second.targetSnapshot.apertureOffset, (!it->second.targetSnapshot.systemMemory && it->second.targetSnapshot.apertureOffset == npdisp.mm_ddScanoutOffset) ? 1U : 0U,
		it->second.targetSnapshot.systemMemory ? 1U : 0U, it->second.targetSnapshot.width, it->second.targetSnapshot.height));
	return true;
}

static bool npdisp_d3d_dp2Clear(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DP2CLEAR32* clear, const std::vector<NPDISP_DDRECTL>& rects)
{
	NPDISP_D3D_TARGET target;
	NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
	std::vector<UINT8> targetStorage;
	const UINT32 validFlags = NPDISP_D3DCLEAR_TARGET | NPDISP_D3DCLEAR_ZBUFFER | NPDISP_D3DCLEAR_STENCIL;
	SINT32 viewportLeft;
	SINT32 viewportTop;
	SINT32 viewportRight;
	SINT32 viewportBottom;
	if (!context || !clear || !clear->dwFlags || (clear->dwFlags & ~validFlags)) return false;
	{
		float z = clear->dvFillDepth;
		UINT16 depth;
		if (z < 0.0f) z = 0.0f; else if (z > 1.0f) z = 1.0f;
		depth = (UINT16)(z * 65535.0f + 0.5f);
		if (npdisp_d3d_enqueueClear(context, clear->dwFlags, clear->dwFillColor, depth, clear->dwFillStencil, rects, true)) return true;
		npdisp_d3d_flush();
	}
	if ((clear->dwFlags & NPDISP_D3DCLEAR_TARGET) && (!context->lpTarget ||
		!(context->targetSnapshot.valid ? npdisp_d3d_targetFromSnapshotBuffered(&context->targetSnapshot, &targetStorage, &target) : npdisp_d3d_target(context->lpTarget, &target)))) return false;
	UINT32 depthMask = 0x0000ffffUL;
	UINT32 stencilBits = 0;
	if ((clear->dwFlags & (NPDISP_D3DCLEAR_ZBUFFER | NPDISP_D3DCLEAR_STENCIL)) &&
		(!context->lpDepth || !npdisp_d3d_depthSurfaceInfo(context->lpDepth, &depthTarget, &depthMask, &stencilBits) ||
		 ((clear->dwFlags & NPDISP_D3DCLEAR_STENCIL) && !stencilBits))) return false;
	viewportLeft = context->viewportWidth ? (SINT32)context->viewportX : 0;
	viewportTop = context->viewportHeight ? (SINT32)context->viewportY : 0;
	viewportRight = context->viewportWidth ? (SINT32)(context->viewportX + context->viewportWidth) :
		((clear->dwFlags & NPDISP_D3DCLEAR_TARGET) ? (SINT32)target.sw.width : (SINT32)depthTarget.width);
	viewportBottom = context->viewportHeight ? (SINT32)(context->viewportY + context->viewportHeight) :
		((clear->dwFlags & NPDISP_D3DCLEAR_TARGET) ? (SINT32)target.sw.height : (SINT32)depthTarget.height);
	if (clear->dwFlags & NPDISP_D3DCLEAR_TARGET) {
		if (rects.empty()) {
			if (!npdisp_d3d_sw_clear(&target.sw, viewportLeft, viewportTop, viewportRight, viewportBottom, clear->dwFillColor)) return false;
		}
		else for (UINT32 i = 0; i < rects.size(); ++i) if (!npdisp_d3d_sw_clear(&target.sw, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom, clear->dwFillColor)) return false;
		if (context->targetSnapshot.valid && context->targetSnapshot.systemMemory &&
			(!targetStorage.size() || !npdisp_d3d_write(&targetStorage[0], context->targetSnapshot.linearBase, (UINT32)targetStorage.size()))) return false;
		if (target.visible) { npdisp_setDirtyAll(); npdisp.updated = 1; }
	}
	if (clear->dwFlags & (NPDISP_D3DCLEAR_ZBUFFER | NPDISP_D3DCLEAR_STENCIL)) {
		float z = clear->dvFillDepth;
		UINT16 depth;
		if (z < 0.0f) z = 0.0f; else if (z > 1.0f) z = 1.0f;
		depth = (UINT16)(z * 65535.0f + 0.5f);
		if (rects.empty()) {
			if (!npdisp_d3d_sw_clearDepthStencil(&depthTarget, viewportLeft, viewportTop, viewportRight, viewportBottom, depth, clear->dwFillStencil, clear->dwFlags, depthMask, stencilBits)) return false;
		}
		else for (UINT32 i = 0; i < rects.size(); ++i) if (!npdisp_d3d_sw_clearDepthStencil(&depthTarget, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom, depth, clear->dwFillStencil, clear->dwFlags, depthMask, stencilBits)) return false;
	}
	return true;
}

static UINT32 npdisp_d3d_clear2(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_CLEAR2DATA32 data = { 0 };
	NPDISP_D3DHAL_DP2CLEAR32 clear = { 0 };
	NPDISP_D3D_CONTEXT* context;
	std::vector<NPDISP_DDRECTL> rects;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	context = npdisp_d3d_getContext(data.dwhContext);
	TRACEOUTD3D(("NPDISP11 D3D_CLEAR2_IN ctx=%08x flags=%08x color=%08x depth=%f stencil=%08x rects=%08x count=%u", data.dwhContext, data.dwFlags, data.dwFillColor, data.dvFillDepth, data.dwFillStencil, data.lpRects, data.dwNumRects));
	if (!context || data.dwNumRects > NPDISP_D3D_MAX_RECTS) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (data.dwNumRects) {
		rects.resize(data.dwNumRects);
		if (!data.lpRects || !npdisp_d3d_read(&rects[0], data.lpRects, data.dwNumRects * (UINT32)sizeof(rects[0]))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	clear.dwFlags = data.dwFlags;
	clear.dwFillColor = data.dwFillColor;
	clear.dvFillDepth = data.dvFillDepth;
	clear.dwFillStencil = data.dwFillStencil;
	if (!npdisp_d3d_dp2Clear(context, &clear, rects)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_CLEAR2_OK ctx=%08x flags=%08x count=%u", data.dwhContext, data.dwFlags, data.dwNumRects));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static bool npdisp_d3d_dp2TexBlt(NPDISP_D3D_CONTEXT* context, const NPDISP_D3DHAL_DP2TEXBLT32* blt)
{
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator srcIt;
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator dstIt;
	NPDISP_D3D_TEXTURE_SNAPSHOT srcMip[NPDISP_D3D_MAX_MIP_LEVELS];
	NPDISP_D3D_TEXTURE_SNAPSHOT dstMip[NPDISP_D3D_MAX_MIP_LEVELS];
	UINT32 srcMipCount = 1U;
	UINT32 dstMipCount = 1U;
	UINT32 levelCount;
	SINT32 srcLeft;
	SINT32 srcTop;
	SINT32 srcRight;
	SINT32 srcBottom;
	SINT32 dstX;
	SINT32 dstY;
	if (!context || !blt || !blt->dwDDDestSurface || !blt->dwDDSrcSurface) return false;
	srcIt = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, blt->dwDDSrcSurface));
	dstIt = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, blt->dwDDDestSurface));
	if (srcIt == npdisp_d3d_surfaces.end()) {
		UINT32 lpSurface = 0;
		if (!npdisp_d3d_dp2SurfaceByHandle(context, blt->dwDDSrcSurface, &lpSurface) || !lpSurface) return false;
		srcIt = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, blt->dwDDSrcSurface));
	}
	if (dstIt == npdisp_d3d_surfaces.end()) {
		UINT32 lpSurface = 0;
		if (!npdisp_d3d_dp2SurfaceByHandle(context, blt->dwDDDestSurface, &lpSurface) || !lpSurface) return false;
		dstIt = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(context->ddLocalNamespace, blt->dwDDDestSurface));
	}
	if (srcIt == npdisp_d3d_surfaces.end() || dstIt == npdisp_d3d_surfaces.end()) return false;
	{
		bool srcPending = npdisp_d3d_asyncPendingSurface(srcIt->second.lpSurface);
		bool dstPending = npdisp_d3d_asyncPendingSurface(dstIt->second.lpSurface);
		if (srcPending || dstPending) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TEXBLT_SYNC src=%08x dst=%08x srcpending=%u dstpending=%u",
				srcIt->second.lpSurface, dstIt->second.lpSurface, srcPending ? 1U : 0U, dstPending ? 1U : 0U));
			npdisp_d3d_flush();
		}
		else {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TEXBLT_NOSYNC src=%08x dst=%08x", srcIt->second.lpSurface, dstIt->second.lpSurface));
		}
	}
	if (!srcIt->second.textureSnapshot.valid && !npdisp_d3d_captureTextureSnapshot(srcIt->second.lpSurface, &srcIt->second.textureSnapshot)) return false;
	if (!dstIt->second.textureSnapshot.valid && !npdisp_d3d_captureTextureSnapshot(dstIt->second.lpSurface, &dstIt->second.textureSnapshot)) return false;
	memset(srcMip, 0, sizeof(srcMip));
	memset(dstMip, 0, sizeof(dstMip));
	if (!npdisp_d3d_captureMipSnapshots(srcIt->second.lpSurface, &srcIt->second.textureSnapshot, srcMip, &srcMipCount) ||
		!npdisp_d3d_captureMipSnapshots(dstIt->second.lpSurface, &dstIt->second.textureSnapshot, dstMip, &dstMipCount)) return false;
	levelCount = srcMipCount < dstMipCount ? srcMipCount : dstMipCount;
	if (!levelCount || blt->rSrc.left < 0 || blt->rSrc.top < 0 || blt->rSrc.right <= blt->rSrc.left ||
		blt->rSrc.bottom <= blt->rSrc.top || blt->x < 0 || blt->y < 0) return false;
	srcLeft = blt->rSrc.left;
	srcTop = blt->rSrc.top;
	srcRight = blt->rSrc.right;
	srcBottom = blt->rSrc.bottom;
	dstX = blt->x;
	dstY = blt->y;
	for (UINT32 level = 0; level < levelCount; ++level) {
		NPDISP_D3D_TEXTURE_SNAPSHOT* src = &srcMip[level];
		NPDISP_D3D_TEXTURE_SNAPSHOT* dst = &dstMip[level];
		SINT32 width = srcRight - srcLeft;
		SINT32 height = srcBottom - srcTop;
		std::vector<UINT8> row;
		if (!src->valid || !dst->valid || srcLeft < 0 || srcTop < 0 || width <= 0 || height <= 0 || dstX < 0 || dstY < 0 ||
			(UINT32)srcRight > src->width || (UINT32)srcBottom > src->height ||
			(UINT32)dstX > dst->width || (UINT32)dstY > dst->height || (UINT32)width > dst->width - (UINT32)dstX ||
			(UINT32)height > dst->height - (UINT32)dstY || src->bpp != dst->bpp || (src->bpp != 8U && src->bpp != 16U && src->bpp != 32U) || src->format != dst->format ||
			src->rMask != dst->rMask || src->gMask != dst->gMask || src->bMask != dst->bMask || src->aMask != dst->aMask ||
			src->duMask != dst->duMask || src->dvMask != dst->dvMask || src->luminanceMask != dst->luminanceMask) return false;
		row.resize((size_t)(UINT32)width * (src->bpp >> 3));
		for (SINT32 y = 0; y < height; ++y) {
			if (!npdisp_d3d_textureRow(src, (UINT32)srcLeft, (UINT32)(srcTop + y), (UINT32)width, &row[0], false) ||
				!npdisp_d3d_textureRow(dst, (UINT32)dstX, (UINT32)(dstY + y), (UINT32)width, &row[0], true)) return false;
		}
		TRACEOUTD3D(("NPDISP11 D3D_DP2_TEXBLT_LEVEL ctx=%08x level=%u size=%dx%d src=%ux%u dst=%ux%u srcmem=%u dstmem=%u",
			context->ddLocalNamespace, level, width, height, src->width, src->height, dst->width, dst->height,
			src->systemMemory ? 1U : 0U, dst->systemMemory ? 1U : 0U));
		if (level + 1U < levelCount) {
			SINT32 nextLeft = srcLeft >> 1;
			SINT32 nextTop = srcTop >> 1;
			SINT32 nextRight = (srcRight + 1) >> 1;
			SINT32 nextBottom = (srcBottom + 1) >> 1;
			if (nextRight <= nextLeft) nextRight = nextLeft + 1;
			if (nextBottom <= nextTop) nextBottom = nextTop + 1;
			srcLeft = nextLeft;
			srcTop = nextTop;
			srcRight = nextRight;
			srcBottom = nextBottom;
			dstX >>= 1;
			dstY >>= 1;
		}
	}
	TRACEOUTD3D(("NPDISP11 D3D_DP2_TEXBLT ctx=%08x src=%08x dst=%08x rect=%d,%d-%d,%d dest=%d,%d levels=%u",
		context->ddLocalNamespace, blt->dwDDSrcSurface, blt->dwDDDestSurface, blt->rSrc.left, blt->rSrc.top,
		blt->rSrc.right, blt->rSrc.bottom, blt->x, blt->y, levelCount));
	return true;
}

static UINT32 npdisp_d3d_dp2Finish(UINT32 lpDataAddr, NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 ddrval, UINT32 errorOffset)
{
	if (!data) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data->dwVertexSizeOrDdrval = ddrval;
	data->dwErrorOffset = errorOffset;
	{
		NPDISP_D3D_CONTEXT* context = npdisp_d3d_getContext(data->dwhContext);
		TRACEOUTD3D(("NPDISP11 D3D_DP2_RESULT ctx=%08x ddrval=%08x error=%u ns=%08x target=%08x depth=%08x", data->dwhContext, ddrval, errorOffset, context ? context->ddLocalNamespace : 0, context ? context->lpTarget : 0, context ? context->lpDepth : 0));
	}
	return npdisp_d3d_write(data, lpDataAddr, sizeof(*data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_drawPrimitives2(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32 data = { 0 };
	InterlockedIncrement(&npdisp_d3d_profileDP2);
	NPDISP_D3D_CONTEXT* context;
	NPDISP_D3D_TNL_CONTEXT* tnl;
	UINT32 vertexSize;
	UINT32 cursor = 0;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.dwhContext)) ||
		!(tnl = npdisp_d3d_getTnlContext(data.dwhContext)) ||
		data.dwCommandLength > NPDISP_D3D_MAX_DP2_COMMAND_BYTES) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_REJECT stage=header data=%08x", lpDataAddr));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_DP2_IN ctx=%08x flags=%08x fvf=%08x vsize=%u cmd=%08x off=%u len=%u vert=%08x voff=%u vlen=%u", data.dwhContext, data.dwFlags, data.dwVertexType, data.dwVertexSizeOrDdrval, data.lpDDCommands, data.dwCommandOffset, data.dwCommandLength, data.lpDDVertexOrVertices, data.dwVertexOffset, data.dwVertexLength));
	if (data.dwFlags & NPDISP_D3DHALDP2_EXECUTEBUFFER) TRACEOUTD3D(("NPDISP11 D3D_DP2_EXECUTEBUFFER ctx=%08x cmd=%08x vert=%08x rstates=%08x ns=%08x", data.dwhContext, data.lpDDCommands, data.lpDDVertexOrVertices, data.lpdwRStates, context->ddLocalNamespace));
	if (data.dwFlags & ~(NPDISP_D3DHALDP2_VALID_FLAGS | NPDISP_D3DHALDP2_W2K_DIRECTBUFFERS)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset);
	if (!data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, 0, 0);
	if (!data.lpDDCommands || data.dwCommandOffset > 0xffffffffUL - data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset);
	vertexSize = data.dwVertexSizeOrDdrval;
	if (!vertexSize && npdisp_d3d_dp2VertexSize(data.dwVertexType, &vertexSize))
		TRACEOUTD3D(("NPDISP11 D3D_DP2_FVF_VSIZE ctx=%08x fvf=%08x vsize=%u", data.dwhContext, data.dwVertexType, vertexSize));

	while (cursor < data.dwCommandLength) {
		NPDISP_D3DHAL_DP2COMMAND32 command = { 0 };
		UINT32 commandStart = cursor;
		if (!npdisp_d3d_dp2CommandRead(&data, cursor, &command, sizeof(command)) || !npdisp_d3d_add(cursor, sizeof(command), &cursor)) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_REJECT stage=command-read offset=%u", data.dwCommandOffset + commandStart));
			return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		if (!commandStart) TRACEOUTD3D(("NPDISP11 D3D_DP2_FIRST ctx=%08x op=%u reserved=%u count=%u", data.dwhContext, command.bCommand, command.bReserved, command.wCount));
		TRACEOUTD3D(("NPDISP11 D3D_DP2_CMD ctx=%08x offset=%u op=%u reserved=%u count=%u", data.dwhContext, data.dwCommandOffset + commandStart, command.bCommand, command.bReserved, command.wCount));

		if ((data.dwFlags & NPDISP_D3DHALDP2_EXECUTEBUFFER) && command.bCommand == NPDISP_D3DOP_EXIT) {
			TRACEOUTD3D(("NPDISP11 D3D_EXECBUF_EXIT ctx=%08x offset=%u", data.dwhContext, data.dwCommandOffset + commandStart));
			return npdisp_d3d_dp2Finish(lpDataAddr, &data, 0, 0);
		}
		if (command.bCommand == NPDISP_D3DDP2OP_RENDERSTATE) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2RENDERSTATE32);
			std::vector<NPDISP_D3DHAL_DP2RENDERSTATE32> renderStates;
			std::vector<NPDISP_D3DSTATE32> states;
			if (bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			if (command.wCount) {
				renderStates.resize(command.wCount);
				states.resize(command.wCount);
				if (!npdisp_d3d_dp2CommandRead(&data, cursor, &renderStates[0], (UINT32)bytes64)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				for (UINT32 i = 0; i < command.wCount; ++i) {
					states[i].type = renderStates[i].renderState;
					states[i].value = renderStates[i].value;
					if (states[i].type < NPDISP_D3D_MAX_RENDERSTATES) TRACEOUTD3D(("NPDISP11 D3D_DP2_RS ctx=%08x type=%u value=%08x", data.dwhContext, states[i].type, states[i].value));
					else TRACEOUTD3D(("NPDISP11 D3D_DP2_RS_PRIVATE ctx=%08x type=%u value=%08x", data.dwhContext, states[i].type, states[i].value));
				}
				if (!npdisp_d3d_applyStates(context, &states[0], command.wCount)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				if ((data.dwFlags & NPDISP_D3DHALDP2_EXECUTEBUFFER) && data.lpdwRStates) {
					for (UINT32 i = 0; i < command.wCount; ++i) {
						UINT32 addr;
						if (states[i].type >= NPDISP_D3D_MAX_RENDERSTATES) continue;
						if (states[i].type > (0xffffffffUL - data.lpdwRStates) / 4U || !npdisp_d3d_add(data.lpdwRStates, states[i].type * 4U, &addr) ||
							!npdisp_d3d_write(&states[i].value, addr, sizeof(states[i].value))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
					}
				}
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_TEXTURESTAGESTATE) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2TEXTURESTAGESTATE32);
			std::vector<NPDISP_D3DHAL_DP2TEXTURESTAGESTATE32> states;
			if (bytes64 > 0xffffffffULL || command.wCount > NPDISP_D3D_MAX_TEXTURESTATE_CHANGES || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			if (command.wCount) {
				states.resize(command.wCount);
				if (!npdisp_d3d_dp2CommandRead(&data, cursor, &states[0], (UINT32)bytes64) || !npdisp_d3d_applyTextureStageStates(context, &states[0], command.wCount)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			}
			cursor += (UINT32)bytes64;
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TSS ctx=%08x count=%u tex0=%08x", data.dwhContext, command.wCount, context->textureStageStates[0][NPDISP_D3DTSS_TEXTUREMAP]));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_SETTRANSFORM) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2SETTRANSFORM32);
			if (!command.wCount || bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength)
				return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				NPDISP_D3DHAL_DP2SETTRANSFORM32 transform;
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(transform), &transform, sizeof(transform)))
					return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				TRACEOUTD3D(("NPDISP11 D3D_DP2_XFORM ctx=%08x offset=%u state=%u", data.dwhContext, data.dwCommandOffset + commandStart, transform.transformState));
				if (transform.transformState == NPDISP_D3DTRANSFORMSTATE_WORLD) tnl->world = transform.matrix;
				else if (transform.transformState >= 4U && transform.transformState <= 6U) tnl->worldBlend[transform.transformState - 4U] = transform.matrix;
				else if (transform.transformState == NPDISP_D3DTRANSFORMSTATE_VIEW) tnl->view = transform.matrix;
				else if (transform.transformState == NPDISP_D3DTRANSFORMSTATE_PROJECTION) tnl->projection = transform.matrix;
				else if (transform.transformState >= 16U && transform.transformState <= 23U) tnl->texture[transform.transformState - 16U] = transform.matrix;
				else {
					TRACEOUTD3D(("NPDISP11 D3D_DP2_XFORM_REJECT ctx=%08x offset=%u state=%u", data.dwhContext, data.dwCommandOffset + commandStart, transform.transformState));
					return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				}
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_SETCLIPPLANE) {
			UINT64 bytes64 = (UINT64)command.wCount * 20U;
			if (bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength)
				return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				struct NPDISP_D3DHAL_DP2SETCLIPPLANE_LOCAL { UINT32 index; float plane[4]; } clipPlane;
				UINT32 offset = cursor + i * 20U;
				if (!npdisp_d3d_dp2CommandRead(&data, offset, &clipPlane, sizeof(clipPlane)) || clipPlane.index >= NPDISP_D3D_MAX_USER_CLIP_PLANES)
					return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				for (UINT32 component = 0; component < 4U; ++component) npdisp_d3d_clipContexts[data.dwhContext].planes[clipPlane.index].plane[component] = clipPlane.plane[component];
				npdisp_d3d_clipContexts[data.dwhContext].planes[clipPlane.index].valid = true;
				TRACEOUTD3D(("NPDISP11 D3D_DP2_CLIPPLANE ctx=%08x index=%u plane=%g,%g,%g,%g", data.dwhContext, clipPlane.index,
					clipPlane.plane[0], clipPlane.plane[1], clipPlane.plane[2], clipPlane.plane[3]));
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_SETMATERIAL) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DMATERIAL7_32);
			if (!command.wCount || bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength)
				return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(tnl->material), &tnl->material, sizeof(tnl->material)))
					return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_CREATELIGHT) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(UINT32);
			if (!command.wCount || bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength)
				return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				UINT32 index;
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * sizeof(index), &index, sizeof(index)) || index >= NPDISP_D3D_MAX_ACTIVE_LIGHTS)
					return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				tnl->lights[index].valid = true;
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_SETLIGHT) {
			for (UINT32 i = 0; i < command.wCount; ++i) {
				NPDISP_D3DHAL_DP2SETLIGHT32 setLight;
				if ((UINT64)cursor + sizeof(setLight) > data.dwCommandLength || !npdisp_d3d_dp2CommandRead(&data, cursor, &setLight, sizeof(setLight)) ||
					setLight.index >= NPDISP_D3D_MAX_ACTIVE_LIGHTS)
					return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				cursor += sizeof(setLight);
				if (setLight.dataType == NPDISP_D3DHAL_SETLIGHT_DATA) {
					if ((UINT64)cursor + sizeof(NPDISP_D3DLIGHT7_32) > data.dwCommandLength ||
						!npdisp_d3d_dp2CommandRead(&data, cursor, &tnl->lights[setLight.index].light, sizeof(NPDISP_D3DLIGHT7_32)))
						return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
					tnl->lights[setLight.index].valid = true;
					cursor += sizeof(NPDISP_D3DLIGHT7_32);
				}
				else if (setLight.dataType == NPDISP_D3DHAL_SETLIGHT_ENABLE) {
					if (!tnl->lights[setLight.index].valid) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
					tnl->lights[setLight.index].enabled = true;
				}
				else if (setLight.dataType == NPDISP_D3DHAL_SETLIGHT_DISABLE) tnl->lights[setLight.index].enabled = false;
				else return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			}
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_VIEWPORTINFO) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2VIEWPORTINFO32);
			NPDISP_D3DHAL_DP2VIEWPORTINFO32 viewport = { 0 };
			if (bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(viewport), &viewport, sizeof(viewport))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				context->viewportX = viewport.dwX;
				context->viewportY = viewport.dwY;
				context->viewportWidth = viewport.dwWidth;
				context->viewportHeight = viewport.dwHeight;
			}
			cursor += (UINT32)bytes64;
			TRACEOUTD3D(("NPDISP11 D3D_DP2_VIEWPORT ctx=%08x count=%u x=%u y=%u w=%u h=%u", data.dwhContext, command.wCount, context->viewportX, context->viewportY, context->viewportWidth, context->viewportHeight));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_WINFO) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2WINFO32);
			NPDISP_D3DHAL_DP2WINFO32 winfo = { 0 };
			if (bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(winfo), &winfo, sizeof(winfo))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				context->wNear = winfo.dvWNear;
				context->wFar = winfo.dvWFar;
			}
			cursor += (UINT32)bytes64;
			TRACEOUTD3D(("NPDISP11 D3D_DP2_WINFO ctx=%08x count=%u near=%g far=%g", data.dwhContext, command.wCount, context->wNear, context->wFar));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_SETPALETTE) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2SETPALETTE32);
			if (!command.wCount || bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				NPDISP_D3DHAL_DP2SETPALETTE32 setPalette = { 0 };
				UINT64 surfaceKey;
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(setPalette), &setPalette, sizeof(setPalette)) || !setPalette.dwSurfaceHandle ||
					!context->ddLocalNamespace) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				surfaceKey = npdisp_d3d_surfaceKey(context->ddLocalNamespace, setPalette.dwSurfaceHandle);
				if (!setPalette.dwPaletteHandle) npdisp_d3d_surfacePalettes.erase(surfaceKey);
				else {
					NPDISP_D3D_PALETTE& palette = npdisp_d3d_palettes[npdisp_d3d_surfaceKey(context->ddLocalNamespace, setPalette.dwPaletteHandle)];
					palette.flags = setPalette.dwPaletteFlags;
					npdisp_d3d_surfacePalettes[surfaceKey] = setPalette.dwPaletteHandle;
				}
				TRACEOUTD3D(("NPDISP11 D3D_DP2_SETPALETTE ctx=%08x ns=%08x surface=%08x palette=%08x flags=%08x", data.dwhContext, context->ddLocalNamespace, setPalette.dwSurfaceHandle, setPalette.dwPaletteHandle, setPalette.dwPaletteFlags));
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_UPDATEPALETTE) {
			NPDISP_D3DHAL_DP2UPDATEPALETTE32 updatePalette = { 0 };
			UINT64 entriesBytes;
			std::vector<UINT32> entries;
			if (!npdisp_d3d_dp2CommandRead(&data, cursor, &updatePalette, sizeof(updatePalette)) || !updatePalette.dwPaletteHandle || !context->ddLocalNamespace ||
				(UINT32)updatePalette.wStartIndex + (UINT32)updatePalette.wNumEntries > 256U || !npdisp_d3d_add(cursor, sizeof(updatePalette), &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			entriesBytes = (UINT64)updatePalette.wNumEntries * sizeof(UINT32);
			if (entriesBytes > 0xffffffffULL || (UINT64)cursor + entriesBytes > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			if (updatePalette.wNumEntries) {
				NPDISP_D3D_PALETTE& palette = npdisp_d3d_palettes[npdisp_d3d_surfaceKey(context->ddLocalNamespace, updatePalette.dwPaletteHandle)];
				entries.resize(updatePalette.wNumEntries);
				if (!npdisp_d3d_dp2CommandRead(&data, cursor, &entries[0], (UINT32)entriesBytes)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				for (UINT32 i = 0; i < updatePalette.wNumEntries; ++i) palette.entries[(UINT32)updatePalette.wStartIndex + i] = entries[i];
			}
			cursor += (UINT32)entriesBytes;
			TRACEOUTD3D(("NPDISP11 D3D_DP2_UPDATEPALETTE ctx=%08x ns=%08x palette=%08x start=%u count=%u statecount=%u", data.dwhContext, context->ddLocalNamespace, updatePalette.dwPaletteHandle, updatePalette.wStartIndex, updatePalette.wNumEntries, command.wCount));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_ZRANGE) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2ZRANGE32);
			NPDISP_D3DHAL_DP2ZRANGE32 zrange = { 0 };
			if (bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(zrange), &zrange, sizeof(zrange))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				context->zMin = zrange.dvMinZ;
				context->zMax = zrange.dvMaxZ;
			}
			cursor += (UINT32)bytes64;
			TRACEOUTD3D(("NPDISP11 D3D_DP2_ZRANGE ctx=%08x count=%u min=%g max=%g", data.dwhContext, command.wCount, context->zMin, context->zMax));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_POINTS) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2POINTS32);
			if (!npdisp_d3d_dp2VertexFormat(data.dwVertexType, vertexSize) || bytes64 > 0xffffffffULL ||
				(UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				NPDISP_D3DHAL_DP2POINTS32 points = { 0 };
				std::vector<NPDISP_D3DTLVERTEX32> vertices;
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(points), &points, sizeof(points)) ||
					(points.wCount && (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data.dwhContext), &data, vertexSize, points.wVStart, points.wCount, &vertices) ||
					!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_POINTLIST, &vertices[0], points.wCount, NULL, 0)))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				TRACEOUTD3D(("NPDISP11 D3D_DP2_POINTS ctx=%08x count=%u base=%u", data.dwhContext, points.wCount, points.wVStart));
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_INDEXEDLINELIST || command.bCommand == NPDISP_D3DDP2OP_INDEXEDLINELIST2) {
			if (!npdisp_d3d_dp2IndexedLineList(context, &data, vertexSize, command.bCommand, command.wCount, &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_INDEXEDLINESTRIP) {
			if (!npdisp_d3d_dp2IndexedLineStrip(context, &data, vertexSize, command.wCount, &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_LINELIST || command.bCommand == NPDISP_D3DDP2OP_LINESTRIP) {
			NPDISP_D3DHAL_DP2STARTVERTEX32 start = { 0 };
			std::vector<NPDISP_D3DTLVERTEX32> vertices;
			UINT32 vertexCount;
			UINT32 primitiveType;
			if (!npdisp_d3d_dp2VertexFormat(data.dwVertexType, vertexSize) ||
				!npdisp_d3d_dp2CommandRead(&data, cursor, &start, sizeof(start)) || !npdisp_d3d_add(cursor, sizeof(start), &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			if (command.bCommand == NPDISP_D3DDP2OP_LINELIST) {
				if (command.wCount > NPDISP_D3D_MAX_VERTICES / 2U) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				vertexCount = (UINT32)command.wCount * 2U;
				primitiveType = NPDISP_D3DPT_LINELIST;
			}
			else {
				if (command.wCount >= NPDISP_D3D_MAX_VERTICES) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				vertexCount = command.wCount ? (UINT32)command.wCount + 1U : 0U;
				primitiveType = NPDISP_D3DPT_LINESTRIP;
			}
			if (vertexCount && (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data.dwhContext), &data, vertexSize, start.wVStart, vertexCount, &vertices) ||
				!npdisp_d3d_drawPrimitive(context, primitiveType, &vertices[0], vertexCount, NULL, 0))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			TRACEOUTD3D(("NPDISP11 D3D_DP2_LINESEQ ctx=%08x op=%u count=%u base=%u", data.dwhContext, command.bCommand, command.wCount, start.wVStart));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_TRIANGLEFAN_IMM) {
			NPDISP_D3DHAL_DP2TRIANGLEFAN_IMM32 fan = { 0 };
			std::vector<NPDISP_D3DTLVERTEX32> vertices;
			UINT32 vertexCount;
			UINT64 bytes64;
			if (!npdisp_d3d_dp2VertexFormat(data.dwVertexType, vertexSize) || command.wCount > NPDISP_D3D_MAX_VERTICES - 2U ||
				!npdisp_d3d_dp2CommandRead(&data, cursor, &fan, sizeof(fan)) || !npdisp_d3d_add(cursor, sizeof(fan), &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			{
				UINT32 alignedCursor;
				if (!npdisp_d3d_dp2AlignImmediateCursor(&data, cursor, &alignedCursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				if (alignedCursor != cursor) TRACEOUTD3D(("NPDISP11 D3D_DP2_IMM_ALIGN ctx=%08x op=%u skip=%u", data.dwhContext, command.bCommand, alignedCursor - cursor));
				cursor = alignedCursor;
			}
			vertexCount = command.wCount ? (UINT32)command.wCount + 2U : 0U;
			bytes64 = (UINT64)vertexCount * vertexSize;
			if (bytes64 > 0xffffffffULL || (vertexCount && (!npdisp_d3d_dp2ImmediateVerticesRead(context, npdisp_d3d_getTnlContext(data.dwhContext), &data, vertexSize, cursor, vertexCount, &vertices) ||
				!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_TRIANGLEFAN, &vertices[0], vertexCount, NULL, 0))) || !npdisp_d3d_add(cursor, (UINT32)bytes64, &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TRIFAN_IMM ctx=%08x count=%u edge=%08x", data.dwhContext, command.wCount, fan.dwEdgeFlags));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_LINELIST_IMM) {
			std::vector<NPDISP_D3DTLVERTEX32> vertices;
			UINT32 vertexCount;
			UINT64 bytes64;
			if (!npdisp_d3d_dp2VertexFormat(data.dwVertexType, vertexSize) || command.wCount > NPDISP_D3D_MAX_VERTICES / 2U) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			{
				UINT32 alignedCursor;
				if (!npdisp_d3d_dp2AlignImmediateCursor(&data, cursor, &alignedCursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				if (alignedCursor != cursor) TRACEOUTD3D(("NPDISP11 D3D_DP2_IMM_ALIGN ctx=%08x op=%u skip=%u", data.dwhContext, command.bCommand, alignedCursor - cursor));
				cursor = alignedCursor;
			}
			vertexCount = (UINT32)command.wCount * 2U;
			bytes64 = (UINT64)vertexCount * vertexSize;
			if (bytes64 > 0xffffffffULL || (vertexCount && (!npdisp_d3d_dp2ImmediateVerticesRead(context, npdisp_d3d_getTnlContext(data.dwhContext), &data, vertexSize, cursor, vertexCount, &vertices) ||
				!npdisp_d3d_drawPrimitive(context, NPDISP_D3DPT_LINELIST, &vertices[0], vertexCount, NULL, 0))) || !npdisp_d3d_add(cursor, (UINT32)bytes64, &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			TRACEOUTD3D(("NPDISP11 D3D_DP2_LINELIST_IMM ctx=%08x count=%u", data.dwhContext, command.wCount));
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_INDEXEDTRIANGLELIST || command.bCommand == NPDISP_D3DDP2OP_INDEXEDTRIANGLELIST2) {
			if (!npdisp_d3d_dp2IndexedTriangleList(context, &data, vertexSize, command.bCommand, command.wCount, &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_INDEXEDTRIANGLESTRIP || command.bCommand == NPDISP_D3DDP2OP_INDEXEDTRIANGLEFAN) {
			if (!npdisp_d3d_dp2IndexedTriangleSequence(context, &data, vertexSize, command.bCommand, command.wCount, &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_TRIANGLELIST || command.bCommand == NPDISP_D3DDP2OP_TRIANGLESTRIP || command.bCommand == NPDISP_D3DDP2OP_TRIANGLEFAN) {
			NPDISP_D3DHAL_DP2STARTVERTEX32 start = { 0 };
			std::vector<NPDISP_D3DTLVERTEX32> vertices;
			UINT32 vertexCount;
			UINT32 primitiveType;
			if (!npdisp_d3d_dp2VertexFormat(data.dwVertexType, vertexSize) || !npdisp_d3d_dp2CommandRead(&data, cursor, &start, sizeof(start)) ||
				!npdisp_d3d_add(cursor, sizeof(start), &cursor)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			if (command.bCommand == NPDISP_D3DDP2OP_TRIANGLELIST) {
				if ((UINT32)command.wCount > NPDISP_D3D_MAX_VERTICES / 3U) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				vertexCount = (UINT32)command.wCount * 3U;
				primitiveType = NPDISP_D3DPT_TRIANGLELIST;
			}
			else {
				if (command.wCount > NPDISP_D3D_MAX_VERTICES - 2U) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				vertexCount = command.wCount ? (UINT32)command.wCount + 2U : 0U;
				primitiveType = command.bCommand == NPDISP_D3DDP2OP_TRIANGLESTRIP ? NPDISP_D3DPT_TRIANGLESTRIP : NPDISP_D3DPT_TRIANGLEFAN;
			}
			if (vertexCount && (!npdisp_d3d_dp2VerticesRead(context, npdisp_d3d_getTnlContext(data.dwhContext), &data, vertexSize, start.wVStart, vertexCount, &vertices) ||
				!npdisp_d3d_drawPrimitive(context, primitiveType, &vertices[0], vertexCount, NULL, 0))) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_TEXBLT) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2TEXBLT32);
			if (!command.wCount || bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				NPDISP_D3DHAL_DP2TEXBLT32 blt = { 0 };
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(blt), &blt, sizeof(blt)) || !npdisp_d3d_dp2TexBlt(context, &blt)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_SETRENDERTARGET) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2SETRENDERTARGET32);
			if (!command.wCount || bytes64 > 0xffffffffULL || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			for (UINT32 i = 0; i < command.wCount; ++i) {
				NPDISP_D3DHAL_DP2SETRENDERTARGET32 setTarget = { 0 };
				if (!npdisp_d3d_dp2CommandRead(&data, cursor + i * (UINT32)sizeof(setTarget), &setTarget, sizeof(setTarget)) || !npdisp_d3d_dp2SetRenderTarget(context, &setTarget)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			}
			cursor += (UINT32)bytes64;
		}
		else if (command.bCommand == NPDISP_D3DDP2OP_CLEAR) {
			NPDISP_D3DHAL_DP2CLEAR32 clear = { 0 };
			std::vector<NPDISP_DDRECTL> rects;
			UINT64 rectBytes = (UINT64)command.wCount * sizeof(NPDISP_DDRECTL);
			if (command.wCount > NPDISP_D3D_MAX_RECTS || rectBytes > 0xffffffffULL || !npdisp_d3d_dp2CommandRead(&data, cursor, &clear, sizeof(clear)) ||
				(UINT64)cursor + sizeof(clear) + rectBytes > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
			cursor += sizeof(clear);
			if (command.wCount) {
				rects.resize(command.wCount);
				if (!npdisp_d3d_dp2CommandRead(&data, cursor, &rects[0], (UINT32)rectBytes)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
				cursor += (UINT32)rectBytes;
			}
			if (!npdisp_d3d_dp2Clear(context, &clear, rects)) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
		else {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_UNKNOWN ctx=%08x op=%u count=%u offset=%u", data.dwhContext, command.bCommand, command.wCount, data.dwCommandOffset + commandStart));
			return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
		}
	}
	return npdisp_d3d_dp2Finish(lpDataAddr, &data, 0, 0);
}

static bool npdisp_d3d_swapSurfaceHandles(UINT32 ddLocalNamespace, UINT32 surfaceHandle)
{
	std::map<UINT32, UINT32>::iterator pending;
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator first;
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator second;
	NPDISP_D3D_SURFACE firstSurface;
	NPDISP_D3D_SURFACE secondSurface;
	UINT32 firstHandle;
	if (!ddLocalNamespace || !surfaceHandle) return false;
	pending = npdisp_d3d_surfaceSwapPending.find(ddLocalNamespace);
	if (pending == npdisp_d3d_surfaceSwapPending.end()) {
		npdisp_d3d_surfaceSwapPending[ddLocalNamespace] = surfaceHandle;
		TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_SWAP_FIRST ns=%08x handle=%08x", ddLocalNamespace, surfaceHandle));
		return true;
	}
	firstHandle = pending->second;
	npdisp_d3d_surfaceSwapPending.erase(pending);
	if (firstHandle == surfaceHandle) return false;
	first = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(ddLocalNamespace, firstHandle));
	second = npdisp_d3d_surfaces.find(npdisp_d3d_surfaceKey(ddLocalNamespace, surfaceHandle));
	if (first == npdisp_d3d_surfaces.end() || second == npdisp_d3d_surfaces.end()) return false;
	firstSurface = first->second;
	secondSurface = second->second;
	firstSurface.handle = surfaceHandle;
	secondSurface.handle = firstHandle;
	first->second = secondSurface;
	second->second = firstSurface;
	TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_SWAP ns=%08x first=%08x second=%08x", ddLocalNamespace, firstHandle, surfaceHandle));
	return true;
}

static bool npdisp_d3d_registerSurfaceEx(UINT32 ddLocalNamespace, UINT32 lpSurface, UINT32 callbackFlags)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_MORE_HEAD32 more = { 0 };
	NPDISP_D3D_SURFACE surface = { 0 };
	UINT64 key;
	UINT32 handleAddr;
	UINT32 surfaceHandle;

	if (!ddLocalNamespace || !lpSurface ||
		!npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpSurfMore ||
		!npdisp_d3d_read(&more, lcl.lpSurfMore, sizeof(more)) ||
		more.dwSize < NPDISP_DDRAWSURFACE_MORE_SIZE_DX7 ||
		!npdisp_d3d_add(lcl.lpSurfMore, NPDISP_DDRAWSURFACE_HANDLE_OFS_DX7, &handleAddr) ||
		!npdisp_d3d_read(&surfaceHandle, handleAddr, sizeof(surfaceHandle)) || !surfaceHandle) return false;

	key = npdisp_d3d_surfaceKey(ddLocalNamespace, surfaceHandle);
	if (lcl.ddsCaps.dwCaps & 0x00800000UL) {
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD traceGbl = { 0 };
		const bool haveGbl = lcl.lpGbl && npdisp_d3d_read(&traceGbl, lcl.lpGbl, sizeof(traceGbl));
		TRACEOUTD3D(("NPDISP11 D3D_EXECBUF_SURFACEEX ns=%08x handle=%08x surf=%08x flags=%08x gbl=%08x fp=%08x pitch=%d size=%ux%u",
			ddLocalNamespace, surfaceHandle, lpSurface, callbackFlags, lcl.lpGbl, haveGbl ? traceGbl.fpVidMem : 0,
			haveGbl ? traceGbl.lPitch : 0, haveGbl ? traceGbl.wWidth : 0, haveGbl ? traceGbl.wHeight : 0));
	}
	if ((lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_SYSTEMMEMORY) && lcl.lpGbl &&
		npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl)) && !gbl.fpVidMem) {
		npdisp_d3d_surfaces.erase(key);
		TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_REMOVE ns=%08x handle=%08x surf=%08x caps=%08x flags=%08x",
			ddLocalNamespace, surfaceHandle, lpSurface, lcl.ddsCaps.dwCaps, callbackFlags));
		return true;
	}

	surface.ddLocalNamespace = ddLocalNamespace;
	surface.handle = surfaceHandle;
	surface.lpSurface = lpSurface;
	npdisp_d3d_surfaceCaps2(surface.lpSurface, &surface.caps2);
	npdisp_d3d_captureTargetSnapshot(surface.lpSurface, &surface.targetSnapshot);
	npdisp_d3d_captureTextureSnapshot(surface.lpSurface, &surface.textureSnapshot);
	npdisp_d3d_surfaces[key] = surface;
	TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_ADD ns=%08x handle=%08x surf=%08x caps=%08x caps2=%08x flags=%08x target=%u off=%08x targetsys=%u texture=%u texfp=%08x texoff=%08x texsys=%u texsize=%ux%u",
		ddLocalNamespace, surfaceHandle, lpSurface, lcl.ddsCaps.dwCaps, surface.caps2, callbackFlags,
		surface.targetSnapshot.valid ? 1U : 0U, surface.targetSnapshot.apertureOffset, surface.targetSnapshot.systemMemory ? 1U : 0U,
		surface.textureSnapshot.valid ? 1U : 0U, surface.textureSnapshot.linearBase, surface.textureSnapshot.apertureOffset,
		surface.textureSnapshot.systemMemory ? 1U : 0U, surface.textureSnapshot.width, surface.textureSnapshot.height));
	return true;
}

static bool npdisp_d3d_registerSurfaceExTree(UINT32 ddLocalNamespace, UINT32 lpSurface, UINT32 callbackFlags)
{
	std::vector<UINT32> surfaces;
	surfaces.push_back(lpSurface);

	for (UINT32 index = 0; index < surfaces.size(); ++index) {
		NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
		UINT32 listAddr;

		if (surfaces.size() > 256U || !npdisp_d3d_registerSurfaceEx(ddLocalNamespace, surfaces[index], callbackFlags) ||
			!npdisp_d3d_read(&lcl, surfaces[index], sizeof(lcl))) return false;

		listAddr = lcl.lpAttachList;
		for (UINT32 count = 0; listAddr && count < 256U; ++count) {
			NPDISP_DDATTACHLIST list = { 0 };
			UINT32 attached = 0;
			if (!npdisp_d3d_read(&list, listAddr, sizeof(list))) return false;
			if (list.lpAttached) attached = list.lpAttached;
			else if (list.lpIAttached) {
				NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD surfaceInt = { 0 };
				if (!npdisp_d3d_read(&surfaceInt, list.lpIAttached, sizeof(surfaceInt))) return false;
				attached = surfaceInt.lpLcl;
			}
			if (attached) {
				bool known = false;
				for (UINT32 i = 0; i < surfaces.size(); ++i) {
					if (surfaces[i] == attached) {
						known = true;
						break;
					}
				}
				if (!known) surfaces.push_back(attached);
			}
			listAddr = list.lpLink;
		}
		if (listAddr) return false;
	}
	return true;
}

static UINT32 npdisp_d3d_createSurfaceEx(UINT32 lpDataAddr)
{
	NPDISP_DD_CREATESURFACEEXDATA32 data = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_MORE_HEAD32 more = { 0 };
	UINT32 ddLocalNamespace;
	UINT32 handleAddr;
	UINT32 surfaceHandle;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !data.lpDDLcl || !data.lpDDSLcl ||
		!npdisp_d3d_read(&lcl, data.lpDDSLcl, sizeof(lcl)) || !lcl.lpSurfMore || !npdisp_d3d_read(&more, lcl.lpSurfMore, sizeof(more))) {
		TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_REJECT stage=header data=%08x ddlcl=%08x surf=%08x", lpDataAddr, data.lpDDLcl, data.lpDDSLcl));
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	if (more.dwSize < NPDISP_DDRAWSURFACE_MORE_SIZE_DX7) {
		TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_REJECT stage=more-size more=%08x size=%u", lcl.lpSurfMore, more.dwSize));
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	if (!npdisp_d3d_add(lcl.lpSurfMore, NPDISP_DDRAWSURFACE_HANDLE_OFS_DX7, &handleAddr) || !npdisp_d3d_read(&surfaceHandle, handleAddr, sizeof(surfaceHandle)) || !surfaceHandle) {
		TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_REJECT stage=handle more=%08x size=%u offset=%u", lcl.lpSurfMore, more.dwSize, NPDISP_DDRAWSURFACE_HANDLE_OFS_DX7));
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	ddLocalNamespace = npdisp_d3d_getDDLocalNamespace(data.lpDDLcl, true);
	if (!ddLocalNamespace) {
		TRACEOUTD3D(("NPDISP11 D3D_SURFACEEX_REJECT stage=ddlocal ddlcl=%08x handle=%08x", data.lpDDLcl, surfaceHandle));
		data.ddRVal = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	if (data.dwFlags & NPDISP_DDHAL_CREATESURFACEEX_SWAPHANDLES) {
		data.ddRVal = npdisp_d3d_swapSurfaceHandles(ddLocalNamespace, surfaceHandle) ? 0 : NPDISP_DDERR_CURRENTLYNOTAVAIL;
		if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}

	{
		UINT32 caps2 = 0;
		const bool cubeMap = npdisp_d3d_surfaceCaps2(data.lpDDSLcl, &caps2) && (caps2 & NPDISP_DDSCAPS2_CUBEMAP);
		data.ddRVal = (cubeMap ? npdisp_d3d_registerSurfaceExTree(ddLocalNamespace, data.lpDDSLcl, data.dwFlags) :
			npdisp_d3d_registerSurfaceEx(ddLocalNamespace, data.lpDDSLcl, data.dwFlags)) ? 0 : NPDISP_DDERR_CURRENTLYNOTAVAIL;
	}
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

typedef struct {
	UINT32 dwhContext;
	UINT32 dwFlags;
	UINT32 dwReserved;
	UINT32 dwNumPasses;
	UINT32 ddrval;
} NPDISP_D3DHAL_VALIDATETEXTURESTAGESTATEDATA32;

static UINT32 npdisp_d3d_validateTextureStageState(UINT32 lpDataAddr)
{
	NPDISP_D3DHAL_VALIDATETEXTURESTAGESTATEDATA32 data = { 0 };
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	data.dwNumPasses = 1;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	TRACEOUTD3D(("NPDISP11 D3D_VALIDATE_TSS ctx=%08x flags=%08x passes=%u", data.dwhContext, data.dwFlags, data.dwNumPasses));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_destroyDDLocal(UINT32 lpDataAddr)
{
	NPDISP_DDHAL_DESTROYDDLOCALDATA32 data = { 0 };
	npdisp_d3d_flush();
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator sit;
	std::map<UINT64, NPDISP_D3D_PALETTE>::iterator pit;
	std::map<UINT64, UINT32>::iterator spit;
	std::map<UINT32, NPDISP_D3D_CONTEXT>::iterator cit;
	UINT32 ddLocalNamespace;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	ddLocalNamespace = npdisp_d3d_getDDLocalNamespace(data.pDDLcl, false);
	if (ddLocalNamespace) {
		sit = npdisp_d3d_surfaces.begin();
		while (sit != npdisp_d3d_surfaces.end()) {
			if (sit->second.ddLocalNamespace == ddLocalNamespace) npdisp_d3d_surfaces.erase(sit++);
			else ++sit;
		}
		for (cit = npdisp_d3d_contexts.begin(); cit != npdisp_d3d_contexts.end(); ++cit) {
			if (cit->second.ddLocalNamespace == ddLocalNamespace) {
				cit->second.ddLocalNamespace = 0;
				cit->second.lpTarget = 0;
				cit->second.lpDepth = 0;
				memset(&cit->second.targetSnapshot, 0, sizeof(cit->second.targetSnapshot));
			}
		}
		pit = npdisp_d3d_palettes.begin();
		while (pit != npdisp_d3d_palettes.end()) {
			if ((UINT32)(pit->first >> 32) == ddLocalNamespace) npdisp_d3d_palettes.erase(pit++);
			else ++pit;
		}
		spit = npdisp_d3d_surfacePalettes.begin();
		while (spit != npdisp_d3d_surfacePalettes.end()) {
			if ((UINT32)(spit->first >> 32) == ddLocalNamespace) npdisp_d3d_surfacePalettes.erase(spit++);
			else ++spit;
		}
		npdisp_d3d_surfaceSwapPending.erase(ddLocalNamespace);
		npdisp_d3d_ddLocalNamespaces.erase(data.pDDLcl);
	}
	data.ddRVal = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	return NPDISP_DDHAL_DRIVER_HANDLED;
}


#if defined(SUPPORT_NPDISP_W2K)
static bool npdisp_d3d_w2kSurfaceOffset(UINT32 fpVidMem, UINT64 bytes, UINT32* offset)
{
	UINT32 value;
	if (!offset || !bytes || bytes > NPDISP_DD_APERTURE_SIZE) return false;
	if (fpVidMem < NPDISP_DD_APERTURE_SIZE) value = fpVidMem;
	else {
		if (!npdisp.mm_vramLinearAddr || fpVidMem < npdisp.mm_vramLinearAddr) return false;
		value = fpVidMem - npdisp.mm_vramLinearAddr;
	}
	if ((UINT64)value + bytes > NPDISP_DD_APERTURE_SIZE) return false;
	*offset = value;
	return true;
}

static bool npdisp_d3d_w2kTargetSnapshot(const NPDISP_W2K_D3D_SURFACE* surface, NPDISP_D3D_TARGET_SNAPSHOT* snapshot)
{
	UINT32 bpp;
	UINT32 bytesPerPixel;
	UINT64 bytes;
	UINT32 offset;

	if (!surface || !snapshot || !surface->SurfaceKey || !surface->Width || !surface->Height || surface->Pitch <= 0) return false;
	memset(snapshot, 0, sizeof(*snapshot));
	bpp = surface->RGBBitCount;
	if (bpp != 15U && bpp != 16U && bpp != 24U && bpp != 32U) bpp = npdisp.bpp;
	bytesPerPixel = (bpp == 15U || bpp == 16U) ? 2U : (bpp == 24U ? 3U : (bpp == 32U ? 4U : 0U));
	if (!bytesPerPixel || (UINT64)surface->Width * bytesPerPixel > (UINT32)surface->Pitch) return false;
	bytes = (UINT64)(UINT32)surface->Pitch * surface->Height;
	if (!bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES || bytes > 0x7fffffffUL) return false;

	snapshot->width = surface->Width;
	snapshot->height = surface->Height;
	snapshot->pitch = surface->Pitch;
	snapshot->bpp = bpp;
	if (surface->Caps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		if (!surface->FpVidMem || (UINT64)surface->FpVidMem + bytes > ((UINT64)1 << 32)) return false;
		snapshot->linearBase = surface->FpVidMem;
		snapshot->systemMemory = true;
	}
	else {
		if (surface->Caps & NPDISP_DDSCAPS_PRIMARYSURFACE) offset = 0U;
		else if (!npdisp_d3d_w2kSurfaceOffset(surface->FpVidMem, bytes, &offset)) return false;
		snapshot->apertureOffset = offset;
		snapshot->linearBase = 0;
	}
	snapshot->valid = true;
	return true;
}

static bool npdisp_d3d_w2kTextureSnapshot(const NPDISP_W2K_D3D_SURFACE* surface, NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot)
{
	UINT32 bytesPerPixel = 2U;
	UINT64 bytes;
	UINT32 offset;

	if (!surface || !snapshot || !surface->SurfaceKey || !(surface->Caps & NPDISP_DDSCAPS_TEXTURE) ||
		!surface->Width || !surface->Height || surface->Pitch <= 0) return false;
	memset(snapshot, 0, sizeof(*snapshot));

	if (surface->Flags & NPDISP_DDRAWISURF_HASPIXELFORMAT) {
		if (surface->PixelFlags & NPDISP_DDPF_FOURCC) return false;
		if (surface->PixelFlags & NPDISP_DDPF_PALETTEINDEXED8) {
			if (surface->RGBBitCount != 8U) return false;
			snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_P8;
			bytesPerPixel = 1U;
		}
		else if (surface->PixelFlags & NPDISP_DDPF_RGB) {
			if (surface->RGBBitCount == 16U &&
				((surface->RBitMask == 0x0000f800UL && surface->GBitMask == 0x000007e0UL &&
				  surface->BBitMask == 0x0000001fUL && !surface->RGBAlphaBitMask) ||
				 (surface->RBitMask == 0x00007c00UL && surface->GBitMask == 0x000003e0UL &&
				  surface->BBitMask == 0x0000001fUL && surface->RGBAlphaBitMask == 0x00008000UL) ||
				 (surface->RBitMask == 0x00000f00UL && surface->GBitMask == 0x000000f0UL &&
				  surface->BBitMask == 0x0000000fUL && surface->RGBAlphaBitMask == 0x0000f000UL))) {
				snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_RGB16;
			}
			else if (surface->RGBBitCount == 32U &&
				surface->RBitMask == 0x00ff0000UL && surface->GBitMask == 0x0000ff00UL &&
				surface->BBitMask == 0x000000ffUL &&
				(!surface->RGBAlphaBitMask || surface->RGBAlphaBitMask == 0xff000000UL)) {
				snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_RGB32;
				bytesPerPixel = 4U;
			}
			else return false;
		}
		else if ((surface->PixelFlags & (NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE)) ==
			(NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE) &&
			surface->RGBBitCount == 16U && surface->RBitMask == 0x0000001fUL &&
			surface->GBitMask == 0x000003e0UL && surface->BBitMask == 0x0000fc00UL) {
			snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_U5V5L6;
		}
		else if ((surface->PixelFlags & NPDISP_DDPF_BUMPDUDV) &&
			!(surface->PixelFlags & NPDISP_DDPF_BUMPLUMINANCE) &&
			surface->RGBBitCount == 16U && surface->RBitMask == 0x000000ffUL &&
			surface->GBitMask == 0x0000ff00UL && !surface->BBitMask) {
			snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_U8V8;
		}
		else return false;
	}
	else {
		snapshot->format = NPDISP_D3D_TEXTURE_FORMAT_RGB16;
	}

	bytes = (UINT64)(UINT32)surface->Pitch * surface->Height;
	if ((UINT64)surface->Width * bytesPerPixel > (UINT32)surface->Pitch ||
		!bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES || bytes > 0x7fffffffUL) return false;

	if (surface->Caps & NPDISP_DDSCAPS_SYSTEMMEMORY) {
		if (!surface->FpVidMem || (UINT64)surface->FpVidMem + bytes > ((UINT64)1 << 32)) return false;
		snapshot->linearBase = surface->FpVidMem;
		snapshot->systemMemory = true;
	}
	else {
		if (!npdisp_d3d_w2kSurfaceOffset(surface->FpVidMem, bytes, &offset)) return false;
		snapshot->apertureOffset = offset;
		snapshot->linearBase = 0;
	}

	snapshot->width = surface->Width;
	snapshot->height = surface->Height;
	snapshot->pitch = surface->Pitch;
	snapshot->bpp = bytesPerPixel * 8U;
	if (surface->Flags & NPDISP_DDRAWISURF_HASPIXELFORMAT) {
		if (surface->PixelFlags & (NPDISP_DDPF_BUMPDUDV | NPDISP_DDPF_BUMPLUMINANCE)) {
			snapshot->duMask = surface->RBitMask;
			snapshot->dvMask = surface->GBitMask;
			snapshot->luminanceMask = surface->BBitMask;
		}
		else {
			snapshot->rMask = surface->RBitMask;
			snapshot->gMask = surface->GBitMask;
			snapshot->bMask = surface->BBitMask;
			snapshot->aMask = surface->RGBAlphaBitMask;
		}
	}
	else {
		snapshot->rMask = 0x0000f800UL;
		snapshot->gMask = 0x000007e0UL;
		snapshot->bMask = 0x0000001fUL;
	}
	snapshot->valid = true;
	return true;
}

static bool npdisp_d3d_w2kDepthSnapshot(const NPDISP_W2K_D3D_SURFACE* surface, NPDISP_D3D_W2K_DEPTH_SURFACE* depth)
{
	UINT64 bytes;
	UINT32 offset;
	UINT32 mask;
	UINT32 stencil;
	if (!surface || !depth || !surface->SurfaceKey || !(surface->Caps & NPDISP_DDSCAPS_ZBUFFER) ||
		(surface->Caps & NPDISP_DDSCAPS_SYSTEMMEMORY) || !surface->Width || !surface->Height || surface->Pitch <= 0 ||
		(UINT64)surface->Width * 2U > (UINT32)surface->Pitch || !(surface->PixelFlags & NPDISP_DDPF_ZBUFFER) ||
		surface->ZBufferBitDepth != 16U) return false;
	if (surface->PixelFlags & NPDISP_DDPF_STENCILBUFFER) {
		stencil = surface->StencilBitDepth;
		if (stencil == 1U) {
			if (surface->ZBitMask != 0x00007fffUL || surface->StencilBitMask != 0x00008000UL) return false;
			mask = 0x00007fffUL;
		}
		else if (stencil == 4U) {
			if (surface->ZBitMask != 0x00000fffUL || surface->StencilBitMask != 0x0000f000UL) return false;
			mask = 0x00000fffUL;
		}
		else return false;
	}
	else {
		if (surface->StencilBitDepth || surface->StencilBitMask ||
			(surface->ZBitMask && surface->ZBitMask != 0x0000ffffUL)) return false;
		mask = 0x0000ffffUL;
		stencil = 0U;
	}
	bytes = (UINT64)(UINT32)surface->Pitch * surface->Height;
	if (!bytes || bytes > NPDISP_D3D_MAX_TEXTURE_BYTES || bytes > 0x7fffffffUL) return false;
	if (!npdisp_d3d_w2kSurfaceOffset(surface->FpVidMem, bytes, &offset)) return false;
	memset(depth, 0, sizeof(*depth));
	depth->lpSurface = surface->SurfaceKey;
	depth->snapshot.linearBase = 0;
	depth->snapshot.apertureOffset = offset;
	depth->snapshot.width = surface->Width;
	depth->snapshot.height = surface->Height;
	depth->snapshot.pitch = surface->Pitch;
	depth->snapshot.bpp = 16U;
	depth->snapshot.valid = true;
	depth->depthMask = mask;
	depth->stencilBits = stencil;
	return true;
}

static bool npdisp_d3d_w2kRegisterSurface(UINT32 ddLocalNamespace, const NPDISP_W2K_D3D_SURFACE* source)
{
	NPDISP_D3D_SURFACE surface = { 0 };
	UINT64 key;

	if (!ddLocalNamespace || !source || !source->SurfaceKey) return false;
	npdisp_d3d_w2kDepthSurfaces.erase(source->SurfaceKey);
	if (source->Caps & NPDISP_DDSCAPS_ZBUFFER) {
		NPDISP_D3D_W2K_DEPTH_SURFACE depth = { 0 };
		if (!npdisp_d3d_w2kDepthSnapshot(source, &depth)) {
			TRACEOUTD3D(("NPDISP11 D3D_W2K_DEPTH_REJECT ns=%08x handle=%08x surf=%08x fp=%08x size=%ux%u pitch=%d caps=%08x pf=%08x zbits=%u sbits=%u zmask=%08x smask=%08x vram=%08x", ddLocalNamespace, source->SurfaceHandle, source->SurfaceKey, source->FpVidMem, source->Width, source->Height, source->Pitch, source->Caps, source->PixelFlags, source->ZBufferBitDepth, source->StencilBitDepth, source->ZBitMask, source->StencilBitMask, npdisp.mm_vramLinearAddr));
			return false;
		}
		depth.ddLocalNamespace = ddLocalNamespace;
		npdisp_d3d_w2kDepthSurfaces[source->SurfaceKey] = depth;
		TRACEOUTD3D(("NPDISP11 D3D_W2K_DEPTH_ADD ns=%08x handle=%08x surf=%08x off=%08x size=%ux%u pitch=%d mask=%08x stencil=%u", ddLocalNamespace, source->SurfaceHandle, source->SurfaceKey, depth.snapshot.apertureOffset, depth.snapshot.width, depth.snapshot.height, depth.snapshot.pitch, depth.depthMask, depth.stencilBits));
	}
	if (!source->SurfaceHandle) return true;
	key = npdisp_d3d_surfaceKey(ddLocalNamespace, source->SurfaceHandle);
	surface.ddLocalNamespace = ddLocalNamespace;
	surface.handle = source->SurfaceHandle;
	surface.lpSurface = source->SurfaceKey;
	surface.caps2 = source->Caps2;
	if (!(source->Caps & NPDISP_DDSCAPS_ZBUFFER)) {
		npdisp_d3d_w2kTargetSnapshot(source, &surface.targetSnapshot);
		if (source->Caps & NPDISP_DDSCAPS_TEXTURE) {
			if (npdisp_d3d_w2kTextureSnapshot(source, &surface.textureSnapshot)) {
				TRACEOUTD3D(("NPDISP11 D3D_W2K_TEXTURE_ADD ns=%08x handle=%08x surf=%08x fp=%08x off=%08x size=%ux%u pitch=%d pf=%08x bpp=%u format=%u sys=%u", ddLocalNamespace, source->SurfaceHandle, source->SurfaceKey, source->FpVidMem, surface.textureSnapshot.apertureOffset, surface.textureSnapshot.width, surface.textureSnapshot.height, surface.textureSnapshot.pitch, source->PixelFlags, surface.textureSnapshot.bpp, surface.textureSnapshot.format, surface.textureSnapshot.systemMemory ? 1U : 0U));
			}
			else {
				TRACEOUTD3D(("NPDISP11 D3D_W2K_TEXTURE_REJECT ns=%08x handle=%08x surf=%08x fp=%08x size=%ux%u pitch=%d caps=%08x flags=%08x pf=%08x rgb=%u mask=%08x/%08x/%08x/%08x", ddLocalNamespace, source->SurfaceHandle, source->SurfaceKey, source->FpVidMem, source->Width, source->Height, source->Pitch, source->Caps, source->Flags, source->PixelFlags, source->RGBBitCount, source->RBitMask, source->GBitMask, source->BBitMask, source->RGBAlphaBitMask));
			}
		}
	}
	npdisp_d3d_surfaces[key] = surface;
	return true;
}

static void npdisp_d3d_w2kInitContext(NPDISP_D3D_CONTEXT* context)
{
	UINT32 stage;
	if (!context) return;
	context->renderStates[NPDISP_D3DRENDERSTATE_FILLMODE] = NPDISP_D3DFILL_SOLID;
	context->renderStates[NPDISP_D3DRENDERSTATE_SHADEMODE] = NPDISP_D3DSHADE_GOURAUD;
	context->renderStates[NPDISP_D3DRENDERSTATE_CULLMODE] = NPDISP_D3DCULL_CCW;
	context->renderStates[NPDISP_D3DRENDERSTATE_LASTPIXEL] = 1U;
	context->renderStates[NPDISP_D3DRENDERSTATE_ALPHAFUNC] = NPDISP_D3DCMP_ALWAYS;
	context->renderStates[NPDISP_D3DRENDERSTATE_SRCBLEND] = NPDISP_D3DBLEND_ONE;
	context->renderStates[NPDISP_D3DRENDERSTATE_DESTBLEND] = NPDISP_D3DBLEND_ZERO;
	context->renderStates[NPDISP_D3DRENDERSTATE_TEXTUREMAPBLEND] = NPDISP_D3DTBLEND_MODULATE;
	context->renderStates[NPDISP_D3DRENDERSTATE_ZWRITEENABLE] = 1U;
	context->renderStates[NPDISP_D3DRENDERSTATE_ZFUNC] = NPDISP_D3DCMP_LESSEQUAL;
	context->renderStates[NPDISP_D3DRENDERSTATE_STENCILFAIL] = NPDISP_D3DSTENCILOP_KEEP;
	context->renderStates[NPDISP_D3DRENDERSTATE_STENCILZFAIL] = NPDISP_D3DSTENCILOP_KEEP;
	context->renderStates[NPDISP_D3DRENDERSTATE_STENCILPASS] = NPDISP_D3DSTENCILOP_KEEP;
	context->renderStates[NPDISP_D3DRENDERSTATE_STENCILFUNC] = NPDISP_D3DCMP_ALWAYS;
	context->renderStates[NPDISP_D3DRENDERSTATE_STENCILMASK] = 0xffffffffUL;
	context->renderStates[NPDISP_D3DRENDERSTATE_STENCILWRITEMASK] = 0xffffffffUL;
	context->renderStates[NPDISP_D3DRENDERSTATE_CLIPPING] = 1U;
	context->textureStageStates[0][NPDISP_D3DTSS_COLOROP] = NPDISP_D3DTOP_MODULATE;
	context->textureStageStates[0][NPDISP_D3DTSS_COLORARG1] = NPDISP_D3DTA_TEXTURE;
	context->textureStageStates[0][NPDISP_D3DTSS_COLORARG2] = NPDISP_D3DTA_DIFFUSE;
	context->textureStageStates[0][NPDISP_D3DTSS_ALPHAOP] = NPDISP_D3DTOP_SELECTARG1;
	context->textureStageStates[0][NPDISP_D3DTSS_ALPHAARG1] = NPDISP_D3DTA_TEXTURE;
	context->textureStageStates[0][NPDISP_D3DTSS_ALPHAARG2] = NPDISP_D3DTA_DIFFUSE;
	for (stage = 0; stage < NPDISP_D3D_MAX_TEXTURE_STAGES; ++stage) {
		context->textureStageStates[stage][NPDISP_D3DTSS_ADDRESSU] = NPDISP_D3DTADDRESS_WRAP;
		context->textureStageStates[stage][NPDISP_D3DTSS_ADDRESSV] = NPDISP_D3DTADDRESS_WRAP;
		context->textureStageStates[stage][NPDISP_D3DTSS_MAGFILTER] = NPDISP_D3DTFG_POINT;
		context->textureStageStates[stage][NPDISP_D3DTSS_MINFILTER] = NPDISP_D3DTFN_POINT;
		if (stage) context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP] = NPDISP_D3DTOP_DISABLE;
	}
}

static UINT32 npdisp_d3d_w2kContextCreate(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_CONTEXT_CREATE data = { 0 };
	NPDISP_D3D_CONTEXT context = { 0 };
	UINT32 handle;

	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !data.DDLocalKey || !data.Target.SurfaceKey) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_d3d_w2kTargetSnapshot(&data.Target, &context.targetSnapshot)) {
		data.Result = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	context.ddLocalNamespace = npdisp_d3d_getDDLocalNamespace(data.DDLocalKey, true);
	if (!context.ddLocalNamespace) {
		data.Result = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	if (!npdisp_d3d_w2kRegisterSurface(context.ddLocalNamespace, &data.Target) ||
		(data.Depth.SurfaceKey && !npdisp_d3d_w2kRegisterSurface(context.ddLocalNamespace, &data.Depth))) {
		data.Result = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	context.dwPID = data.ProcessId;
	context.lpTarget = data.Target.SurfaceKey;
	context.lpDepth = data.Depth.SurfaceKey;
	handle = npdisp_d3d_allocContext();
	if (!handle) {
		data.Result = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	npdisp_d3d_w2kInitContext(&context);
	npdisp_d3d_contexts[handle] = context;
	{
		NPDISP_D3D_TNL_CONTEXT tnl;
		npdisp_d3d_tnlInitialize(&tnl);
		npdisp_d3d_tnlContexts[handle] = tnl;
	}
	data.Context = handle;
	data.Result = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) {
		npdisp_d3d_contexts.erase(handle);
		npdisp_d3d_tnlContexts.erase(handle);
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_W2K_CTX_CREATE handle=%08x ns=%08x target=%08x depth=%08x runtime=%u", handle, context.ddLocalNamespace, context.lpTarget, context.lpDepth, data.RuntimeVersion));
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

static UINT32 npdisp_d3d_w2kContextDestroy(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_CONTEXT_DESTROY data = { 0 };
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !npdisp_d3d_getContext(data.Context)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	npdisp_d3d_flush();
	npdisp_d3d_destroyLegacyTextures(data.Context);
	npdisp_d3d_contexts.erase(data.Context);
	npdisp_d3d_tnlContexts.erase(data.Context);
	TRACEOUTD3D(("NPDISP11 D3D_W2K_CTX_DESTROY handle=%08x", data.Context));
	data.Result = 0;
	return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_w2kContextDestroyAll(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_CONTEXT_DESTROY_ALL data = { 0 };
	std::map<UINT32, NPDISP_D3D_CONTEXT>::iterator it;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	npdisp_d3d_flush();
	it = npdisp_d3d_contexts.begin();
	while (it != npdisp_d3d_contexts.end()) {
		if (it->second.dwPID == data.ProcessId) {
			npdisp_d3d_destroyLegacyTextures(it->first);
			npdisp_d3d_tnlContexts.erase(it->first);
			npdisp_d3d_contexts.erase(it++);
		}
		else ++it;
	}
	TRACEOUTD3D(("NPDISP11 D3D_W2K_CTX_DESTROY_ALL pid=%08x", data.ProcessId));
	data.Result = 0;
	return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_w2kSceneCapture(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_SCENE_CAPTURE data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.Context)) ||
		(data.Flag != NPDISP_D3DHAL_SCENE_CAPTURE_START && data.Flag != NPDISP_D3DHAL_SCENE_CAPTURE_END)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	context->sceneActive = (data.Flag == NPDISP_D3DHAL_SCENE_CAPTURE_START);
	data.Result = 0;
	TRACEOUTD3D(("NPDISP11 D3D_W2K_SCENE ctx=%08x flag=%u active=%u", data.Context, data.Flag, context->sceneActive ? 1U : 0U));
	return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_w2kSetRenderTarget(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_SET_RENDERTARGET data = { 0 };
	NPDISP_D3D_CONTEXT* context;
	NPDISP_D3D_TARGET_SNAPSHOT snapshot;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.Context)) || !data.Target.SurfaceKey) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (!npdisp_d3d_w2kTargetSnapshot(&data.Target, &snapshot) ||
		!npdisp_d3d_w2kRegisterSurface(context->ddLocalNamespace, &data.Target) ||
		(data.Depth.SurfaceKey && !npdisp_d3d_w2kRegisterSurface(context->ddLocalNamespace, &data.Depth))) {
		data.Result = NPDISP_DDERR_CURRENTLYNOTAVAIL;
		npdisp_d3d_write(&data, lpDataAddr, sizeof(data));
		return NPDISP_DDHAL_DRIVER_HANDLED;
	}
	npdisp_d3d_flush();
	context->lpTarget = data.Target.SurfaceKey;
	context->lpDepth = data.Depth.SurfaceKey;
	context->targetSnapshot = snapshot;
	data.Result = 0;
	TRACEOUTD3D(("NPDISP11 D3D_W2K_SET_TARGET ctx=%08x target=%08x depth=%08x size=%ux%u pitch=%d", data.Context, context->lpTarget, context->lpDepth, snapshot.width, snapshot.height, snapshot.pitch));
	return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_w2kCreateSurfaceEx(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_CREATE_SURFACE_EX data = { 0 };
	UINT32 ddLocalNamespace;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !data.DDLocalKey || !data.Surface.SurfaceKey) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	ddLocalNamespace = npdisp_d3d_getDDLocalNamespace(data.DDLocalKey, true);
	data.Result = (ddLocalNamespace && data.Surface.SurfaceHandle && npdisp_d3d_w2kRegisterSurface(ddLocalNamespace, &data.Surface)) ? 0 : NPDISP_DDERR_CURRENTLYNOTAVAIL;
	TRACEOUTD3D(("NPDISP11 D3D_W2K_SURFACEEX ns=%08x handle=%08x surf=%08x caps=%08x flags=%08x result=%08x", ddLocalNamespace, data.Surface.SurfaceHandle, data.Surface.SurfaceKey, data.Surface.Caps, data.Flags, data.Result));
	return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_w2kDestroyDDLocal(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_DESTROY_DDLOCAL data = { 0 };
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator sit;
	std::map<UINT64, NPDISP_D3D_PALETTE>::iterator pit;
	std::map<UINT64, UINT32>::iterator spit;
	std::map<UINT32, NPDISP_D3D_CONTEXT>::iterator cit;
	UINT32 ddLocalNamespace;

	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !data.DDLocalKey) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	npdisp_d3d_flush();
	ddLocalNamespace = npdisp_d3d_getDDLocalNamespace(data.DDLocalKey, false);
	if (ddLocalNamespace) {
		sit = npdisp_d3d_surfaces.begin();
		while (sit != npdisp_d3d_surfaces.end()) {
			if (sit->second.ddLocalNamespace == ddLocalNamespace) npdisp_d3d_surfaces.erase(sit++); else ++sit;
		}
		{
			std::map<UINT32, NPDISP_D3D_W2K_DEPTH_SURFACE>::iterator dit = npdisp_d3d_w2kDepthSurfaces.begin();
			while (dit != npdisp_d3d_w2kDepthSurfaces.end()) {
				if (dit->second.ddLocalNamespace == ddLocalNamespace) npdisp_d3d_w2kDepthSurfaces.erase(dit++); else ++dit;
			}
		}
		for (cit = npdisp_d3d_contexts.begin(); cit != npdisp_d3d_contexts.end(); ++cit) {
			if (cit->second.ddLocalNamespace == ddLocalNamespace) {
				cit->second.ddLocalNamespace = 0;
				cit->second.lpTarget = 0;
				cit->second.lpDepth = 0;
				memset(&cit->second.targetSnapshot, 0, sizeof(cit->second.targetSnapshot));
			}
		}
		pit = npdisp_d3d_palettes.begin();
		while (pit != npdisp_d3d_palettes.end()) {
			if ((UINT32)(pit->first >> 32) == ddLocalNamespace) npdisp_d3d_palettes.erase(pit++); else ++pit;
		}
		spit = npdisp_d3d_surfacePalettes.begin();
		while (spit != npdisp_d3d_surfacePalettes.end()) {
			if ((UINT32)(spit->first >> 32) == ddLocalNamespace) npdisp_d3d_surfacePalettes.erase(spit++); else ++spit;
		}
		npdisp_d3d_surfaceSwapPending.erase(ddLocalNamespace);
		npdisp_d3d_ddLocalNamespaces.erase(data.DDLocalKey);
	}
	TRACEOUTD3D(("NPDISP11 D3D_W2K_DDLOCAL_DESTROY key=%08x ns=%08x", data.DDLocalKey, ddLocalNamespace));
	data.Result = 0;
	return npdisp_d3d_write(&data, lpDataAddr, sizeof(data)) ? NPDISP_DDHAL_DRIVER_HANDLED : NPDISP_DDHAL_DRIVER_NOTHANDLED;
}

static UINT32 npdisp_d3d_w2kTrace(UINT32 lpDataAddr)
{
	NPDISP_W2K_D3D_TRACE trace = { 0 };
	if (!npdisp_d3d_read(&trace, lpDataAddr, sizeof(trace))) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	if (trace.Event == NPDISP_W2K_D3D_TRACE_SETUP) {
		TRACEOUTD3D(("NPDISP11 D3D_W2K_GUEST_SETUP version=%08x size=%u hwsize=%u extsize=%u setup=%08x callbacks=%08x getinfo=%08x formats=%u",
			trace.Values[0], trace.Values[1], trace.Values[2], trace.Values[3], trace.Values[4], trace.Values[5], trace.Values[6], trace.Values[7]));
	}
	else if (trace.Event == NPDISP_W2K_D3D_TRACE_INIT) {
		TRACEOUTD3D(("NPDISP11 D3D_W2K_GUEST_INIT halflags=%08x global=%08x callbacks=%08x getinfo=%08x globalsize=%u callbacksize=%u formats=%u cb3flags=%08x basecb=%08x hwflags=%08x",
			trace.Values[0], trace.Values[1], trace.Values[2], trace.Values[3], trace.Values[4], trace.Values[5], trace.Values[6], trace.Values[7], trace.Values[8], trace.Values[9]));
	}
	else if (trace.Event == NPDISP_W2K_D3D_TRACE_GETDRIVERINFO) {
		TRACEOUTD3D(("NPDISP11 D3D_W2K_GETINFO tag=%u guid=%08x-%08x-%08x-%08x size=%u data=%08x expected=%u actual=%u result=%08x",
			trace.Values[7], trace.Values[0], trace.Values[1], trace.Values[2], trace.Values[3], trace.Values[8], trace.Values[9],
			trace.Values[4], trace.Values[5], trace.Values[6]));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	else {
		TRACEOUTD3D(("NPDISP11 D3D_W2K_TRACE_UNKNOWN event=%08x", trace.Event));
	}
	return NPDISP_DDHAL_DRIVER_HANDLED;
}

UINT32 npdisp_d3d_dispatchW2k(UINT32 callbackId, UINT32 lpDataAddr, UINT32 dataSize)
{
	switch (callbackId) {
	case NPDISP_W2K_D3D_DISPATCH_CONTEXTCREATE:
		if (dataSize < sizeof(NPDISP_W2K_D3D_CONTEXT_CREATE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kContextCreate(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_CONTEXTDESTROY:
		if (dataSize < sizeof(NPDISP_W2K_D3D_CONTEXT_DESTROY)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kContextDestroy(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_CONTEXTDESTROYALL:
		if (dataSize < sizeof(NPDISP_W2K_D3D_CONTEXT_DESTROY_ALL)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kContextDestroyAll(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_SCENECAPTURE:
		if (dataSize < sizeof(NPDISP_W2K_D3D_SCENE_CAPTURE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kSceneCapture(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_DRAWPRIMITIVES2:
		if (dataSize < sizeof(NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_drawPrimitives2(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_VALIDATETEXTURESTAGE:
		if (dataSize < sizeof(NPDISP_D3DHAL_VALIDATETEXTURESTAGESTATEDATA32)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_validateTextureStageState(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_SETRENDERTARGET:
		if (dataSize < sizeof(NPDISP_W2K_D3D_SET_RENDERTARGET)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kSetRenderTarget(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_CREATESURFACEEX:
		if (dataSize < sizeof(NPDISP_W2K_D3D_CREATE_SURFACE_EX)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kCreateSurfaceEx(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_GETDRIVERSTATE:
		if (dataSize < sizeof(NPDISP_DD_GETDRIVERSTATEDATA32)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_getDriverState(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_DESTROYDDLOCAL:
		if (dataSize < sizeof(NPDISP_W2K_D3D_DESTROY_DDLOCAL)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kDestroyDDLocal(lpDataAddr);
	case NPDISP_W2K_D3D_DISPATCH_TRACE:
		if (dataSize < sizeof(NPDISP_W2K_D3D_TRACE)) return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		return npdisp_d3d_w2kTrace(lpDataAddr);
	default:
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
}
#endif

UINT32 npdisp_d3d_dispatch(UINT32 callbackId, UINT32 lpDataAddr)
{
	switch (callbackId) {
	case NPDISP_DDBRIDGE_CB_D3D_CONTEXTCREATE: return npdisp_d3d_contextCreate(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_CONTEXTDESTROY: return npdisp_d3d_contextDestroy(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_CONTEXTDESTROYALL: return npdisp_d3d_contextDestroyAll(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_SCENECAPTURE: return npdisp_d3d_sceneCapture(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_RENDERSTATE: return npdisp_d3d_renderState(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_RENDERPRIMITIVE: return npdisp_d3d_renderPrimitive(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_GETSTATE: return npdisp_d3d_getState(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_TEXTURECREATE: return npdisp_d3d_textureCreate(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_TEXTUREDESTROY: return npdisp_d3d_textureDestroy(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_TEXTURESWAP: return npdisp_d3d_textureSwap(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_TEXTUREGETSURF: return npdisp_d3d_textureGetSurf(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_SETRENDERTARGET: return npdisp_d3d_setRenderTarget(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_CLEAR: return npdisp_d3d_clear(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_CLEAR2: return npdisp_d3d_clear2(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_DRAWONEPRIMITIVE: return npdisp_d3d_drawOnePrimitive(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_DRAWONEINDEXEDPRIMITIVE: return npdisp_d3d_drawOneIndexedPrimitive(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_DRAWPRIMITIVES: return npdisp_d3d_drawPrimitives(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_DRAWPRIMITIVES2: return npdisp_d3d_drawPrimitives2(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_VALIDATESTAGE: return npdisp_d3d_validateTextureStageState(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_CREATESURFACEEX: return npdisp_d3d_createSurfaceEx(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_DESTROYDDLOCAL: return npdisp_d3d_destroyDDLocal(lpDataAddr);
	case NPDISP_DDBRIDGE_CB_D3D_GETDRIVERSTATE: return npdisp_d3d_getDriverState(lpDataAddr);
	default: return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
}

#endif
