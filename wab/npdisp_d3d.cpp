/**
 * @file	npdisp_d3d.cpp
 * @brief	NPDISP Direct3D HAL frontend
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_D3D)

#include	<deque>
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

#define NPDISP_D3D_HAL_GLOBAL_SIZE				192U
#define NPDISP_D3D_HAL_CALLBACKS_SIZE			140U
#define NPDISP_D3D_TEXTURE_FORMAT_COUNT			5U

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
#define NPDISP_D3DDEVCAPS_HWRASTERIZATION		0x00080000UL
#define NPDISP_D3DCOLOR_RGB						0x00000002UL
#define NPDISP_D3DPMISCCAPS_CULLNONE			0x00000010UL
#define NPDISP_D3DPMISCCAPS_CULLCW				0x00000020UL
#define NPDISP_D3DPMISCCAPS_CULLCCW				0x00000040UL
#define NPDISP_D3DPRASTERCAPS_ZTEST				0x00000010UL
#define NPDISP_D3DPRASTERCAPS_FOGVERTEX			0x00000080UL
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
#define NPDISP_D3DVTXPCAPS_DIRECTIONALLIGHTS	0x00000008UL
#define NPDISP_D3DVTXPCAPS_POSITIONALLIGHTS		0x00000010UL
#define NPDISP_D3DTEXOPCAPS_DISABLE				0x00000001UL
#define NPDISP_D3DTEXOPCAPS_SELECTARG1			0x00000002UL
#define NPDISP_D3DTEXOPCAPS_SELECTARG2			0x00000004UL
#define NPDISP_D3DTEXOPCAPS_MODULATE			0x00000008UL
#define NPDISP_D3DTEXOPCAPS_ADD					0x00000040UL
#define NPDISP_D3DTEXOPCAPS_BUMPENVMAP			0x00200000UL
#define NPDISP_D3DTEXOPCAPS_BUMPENVMAPLUMINANCE 0x00400000UL
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
	UINT32 ddLocalNamespace;
	UINT32 handle;
	UINT32 lpSurface;
	UINT32 caps2;
	NPDISP_D3D_TARGET_SNAPSHOT targetSnapshot;
	NPDISP_D3D_TEXTURE_SNAPSHOT textureSnapshot;
} NPDISP_D3D_SURFACE;

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
static std::map<UINT32, UINT32> npdisp_d3d_ddLocalNamespaces;
static std::map<UINT64, NPDISP_D3D_SURFACE> npdisp_d3d_surfaces;
static std::map<UINT32, UINT32> npdisp_d3d_surfaceSwapPending;
static std::map<UINT32, NPDISP_D3D_LEGACY_TEXTURE> npdisp_d3d_legacyTextures;
static std::map<UINT64, NPDISP_D3D_PALETTE> npdisp_d3d_palettes;
static std::map<UINT64, UINT32> npdisp_d3d_surfacePalettes;
static UINT32 npdisp_d3d_nextContext = 1;
static UINT32 npdisp_d3d_nextDDLocalNamespace = 1;
static UINT32 npdisp_d3d_nextLegacyTexture = 1;

#define NPDISP_D3D_STATE_MAGIC 0x31533344UL
#define NPDISP_D3D_STATE_VERSION 1U

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
static void npdisp_d3d_asyncStopWorker(void);

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
	global.hwCaps.dtcTransformCaps.dwSize = sizeof(global.hwCaps.dtcTransformCaps);
	global.hwCaps.dlcLightingCaps.dwSize = sizeof(global.hwCaps.dlcLightingCaps);
	global.hwCaps.dpcLineCaps.dwSize = sizeof(global.hwCaps.dpcLineCaps);
	global.hwCaps.dpcLineCaps.dwRasterCaps = NPDISP_D3DPRASTERCAPS_ZTEST | NPDISP_D3DPRASTERCAPS_FOGVERTEX;
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
	global.hwCaps.dpcTriCaps.dwRasterCaps = NPDISP_D3DPRASTERCAPS_ZTEST | NPDISP_D3DPRASTERCAPS_FOGVERTEX;
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
		NPDISP_D3DPTFILTERCAPS_LINEAR | NPDISP_D3DPTFILTERCAPS_LINEARMIPLINEAR |
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

	if (npdisp_d3d_guidEqual(&data.guidInfo, &parseUnknownCommandGuid)) {
		data.ddRVal = 0;
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
		extendedCaps.dwFVFCaps = 2;
		extendedCaps.dwTextureOpCaps = NPDISP_D3DTEXOPCAPS_DISABLE | NPDISP_D3DTEXOPCAPS_SELECTARG1 |
			NPDISP_D3DTEXOPCAPS_SELECTARG2 | NPDISP_D3DTEXOPCAPS_MODULATE |
			NPDISP_D3DTEXOPCAPS_ADD | NPDISP_D3DTEXOPCAPS_BUMPENVMAP |
			NPDISP_D3DTEXOPCAPS_BUMPENVMAPLUMINANCE;
		extendedCaps.wMaxTextureBlendStages = 3;
		extendedCaps.wMaxSimultaneousTextures = 3;
		extendedCaps.dwVertexProcessingCaps = NPDISP_D3DVTXPCAPS_DIRECTIONALLIGHTS | NPDISP_D3DVTXPCAPS_POSITIONALLIGHTS;
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
			}
			else {
				ResetEvent(npdisp_d3d_asyncWake);
				npdisp_d3d_asyncActiveTarget = 0;
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

static bool npdisp_d3d_resolveAperture(UINT32 fpVidMem, SINT32 pitch, UINT32 height, UINT8** hostBase, UINT32* apertureOffset)
{
	UINT32 offset;
	UINT64 bytes;
	if (!hostBase || !apertureOffset || !npdisp.mm_vramLinearAddr || pitch <= 0 || !height) return false;
	bytes = (UINT64)(UINT32)pitch * height;
	if (fpVidMem == 0 || fpVidMem == npdisp.mm_vramLinearAddr) {
		if (!npdisp.mm_screenPtr || bytes > npdisp.mm_screenSize || bytes > NPDISP_DD_PRIMARY_REGION_SIZE) return false;
		*hostBase = npdisp.mm_screenPtr;
		*apertureOffset = 0;
		return true;
	}
	if (fpVidMem < npdisp.mm_vramLinearAddr) return false;
	offset = fpVidMem - npdisp.mm_vramLinearAddr;
	if (offset < NPDISP_DD_OFFSCREEN_OFFSET || offset >= NPDISP_DD_APERTURE_SIZE ||
		!npdisp.mm_ddOffscreenPtr || npdisp.mm_ddOffscreenSize != NPDISP_DD_OFFSCREEN_SIZE) return false;
	if (bytes > NPDISP_DD_OFFSCREEN_SIZE ||
		(UINT64)(offset - NPDISP_DD_OFFSCREEN_OFFSET) + bytes > NPDISP_DD_OFFSCREEN_SIZE) return false;
	*hostBase = npdisp.mm_ddOffscreenPtr + (offset - NPDISP_DD_OFFSCREEN_OFFSET);
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
	UINT32 apertureOffset = 0;
	UINT32 fpVidMem;
	if (!snapshot || !snapshot->valid || !target || !npdisp.mm_vramLinearAddr) return false;
	if (snapshot->apertureOffset > 0xffffffffUL - npdisp.mm_vramLinearAddr) return false;
	fpVidMem = npdisp.mm_vramLinearAddr + snapshot->apertureOffset;
	if (!npdisp_d3d_resolveAperture(fpVidMem, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
		apertureOffset != snapshot->apertureOffset) return false;
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
	if (!lpSurface || !target || !npdisp_d3d_read(&lcl, lpSurface, sizeof(lcl)) || !lcl.lpGbl ||
		!(lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_ZBUFFER) || (lcl.ddsCaps.dwCaps & NPDISP_DDSCAPS_SYSTEMMEMORY) ||
		!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl))) return false;
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

static bool npdisp_d3d_lclBufferRead(UINT32 lpSurfaceLcl, UINT32 offset, void* dst, UINT32 size)
{
	NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD lcl = { 0 };
	NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD gbl = { 0 };
	UINT32 addr;
	UINT32 apertureOffset;
	UINT64 end;
	if (!lpSurfaceLcl || !dst || !size || !npdisp_d3d_read(&lcl, lpSurfaceLcl, sizeof(lcl)) || !lcl.lpGbl ||
		!npdisp_d3d_read(&gbl, lcl.lpGbl, sizeof(gbl)) || !gbl.fpVidMem || !npdisp_d3d_add(gbl.fpVidMem, offset, &addr)) return false;

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
	return npdisp_d3d_read(dst, addr, size);
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
		if (type == NPDISP_D3DRENDERSTATE_SCENECAPTURE) context->sceneActive = states[i].value ? true : false;
		context->renderStates[type] = states[i].value;
	}
	return true;
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
		context->textureStageStates[states[i].wStage][states[i].TSState] = states[i].dwValue;
		if (states[i].wStage < NPDISP_D3D_RASTER_TEXTURE_STAGES) {
			TRACEOUTD3D(("NPDISP11 D3D_DP2_TSS_STATE stage=%u state=%u value=%08x", states[i].wStage, states[i].TSState, states[i].dwValue));
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
	if (state->zEnable > 1U) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=wbuffer value=%08x", state->zEnable));
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
	state->specularEnable = context->renderStates[NPDISP_D3DRENDERSTATE_SPECULARENABLE] ? 1U : 0U;
	state->alphaTestEnable = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHATESTENABLE] ? 1U : 0U;
	state->alphaRef = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHAREF] & 0xffU;
	state->alphaFunc = context->renderStates[NPDISP_D3DRENDERSTATE_ALPHAFUNC];
	state->fogEnable = context->renderStates[NPDISP_D3DRENDERSTATE_FOGENABLE] ? 1U : 0U;
	state->fogColor = context->renderStates[NPDISP_D3DRENDERSTATE_FOGCOLOR];
	state->coordWrap[0] = context->renderStates[NPDISP_D3DRENDERSTATE_WRAP0];
	state->coordWrap[1] = context->renderStates[NPDISP_D3DRENDERSTATE_WRAP0 + 1U];
	if (state->shadeMode != NPDISP_D3DSHADE_FLAT && state->shadeMode != NPDISP_D3DSHADE_GOURAUD) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=shade value=%08x", state->shadeMode));
		return false;
	}
	if (state->cullMode != NPDISP_D3DCULL_NONE && state->cullMode != NPDISP_D3DCULL_CW && state->cullMode != NPDISP_D3DCULL_CCW) {
		TRACEOUTD3D(("NPDISP11 D3D_DRAWSTATE_REJECT reason=cull value=%08x", state->cullMode));
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
	if (!npdisp_d3d_resolveAperture(snapshot->linearBase, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
		apertureOffset != snapshot->apertureOffset) return false;
	memcpy(dst, hostBase, size);
	return true;
}

static bool npdisp_d3d_textureSnapshotPointer(const NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot, const UINT8** pixels)
{
	UINT8* hostBase = NULL;
	UINT32 apertureOffset = 0;
	if (!snapshot || !snapshot->valid || !pixels || snapshot->systemMemory) return false;
	if (!npdisp_d3d_resolveAperture(snapshot->linearBase, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
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
	if (!npdisp_d3d_resolveAperture(snapshot->linearBase, snapshot->pitch, snapshot->height, &hostBase, &apertureOffset) ||
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

static bool npdisp_d3d_prepareTextureStage(NPDISP_D3D_CONTEXT* context, UINT32 stage, NPDISP_D3D_RASTERSTATE* state, std::vector<UINT8>* storage, NPDISP_D3D_TEXTURE* texture, bool directVideo)
{
	std::map<UINT64, NPDISP_D3D_SURFACE>::iterator it;
	const NPDISP_D3D_TEXTURE_SNAPSHOT* snapshot;
	NPDISP_D3D_TEXTURE_SNAPSHOT cubeSnapshots[NPDISP_D3D_CUBE_FACES];
	NPDISP_D3D_TEXTURE_SNAPSHOT mipSnapshots[NPDISP_D3D_MAX_MIP_LEVELS];
	UINT32 handle, colorOp, colorArg1, colorArg2, alphaOp, alphaArg1, alphaArg2, addressU, addressV, magFilter, minFilter, mipFilter, texCoordIndex;
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
	texCoordIndex = context->textureStageStates[stage][NPDISP_D3DTSS_TEXCOORDINDEX];
	if (texCoordIndex > 1U ||
		(mipFilter != NPDISP_D3DTFP_NONE && mipFilter != NPDISP_D3DTFP_POINT && mipFilter != NPDISP_D3DTFP_LINEAR) ||
		context->textureStageStates[stage][NPDISP_D3DTSS_TEXTURETRANSFORMFLAGS] ||
		(addressU != NPDISP_D3DTADDRESS_WRAP && addressU != NPDISP_D3DTADDRESS_MIRROR && addressU != NPDISP_D3DTADDRESS_CLAMP) ||
		(addressV != NPDISP_D3DTADDRESS_WRAP && addressV != NPDISP_D3DTADDRESS_MIRROR && addressV != NPDISP_D3DTADDRESS_CLAMP) ||
		(magFilter != NPDISP_D3DTFG_POINT && magFilter != NPDISP_D3DTFG_LINEAR) ||
		(minFilter != NPDISP_D3DTFN_POINT && minFilter != NPDISP_D3DTFN_LINEAR)) {
		TRACEOUTD3D(("NPDISP11 D3D_TEXTURE_REJECT reason=state stage=%u handle=%08x op=%u tc=%u addr=%u/%u filter=%u/%u mip=%u tf=%u",
			stage, handle, colorOp, texCoordIndex, addressU, addressV, minFilter, magFilter, mipFilter,
			context->textureStageStates[stage][NPDISP_D3DTSS_TEXTURETRANSFORMFLAGS]));
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
			colorOp != NPDISP_D3DTOP_SELECTARG2 && colorOp != NPDISP_D3DTOP_ADD) return false;
		if (snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_RGB16 && snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_RGB32 &&
			snapshot->format != NPDISP_D3D_TEXTURE_FORMAT_P8) return false;
		if ((colorArg1 & ~NPDISP_D3DTA_SELECTMASK) || (colorArg2 & ~NPDISP_D3DTA_SELECTMASK) ||
			((colorArg1 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE) ||
			((colorArg2 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE)) return false;
	}
	if (alphaOp && alphaOp != NPDISP_D3DTOP_DISABLE &&
		alphaOp != NPDISP_D3DTOP_MODULATE && alphaOp != NPDISP_D3DTOP_SELECTARG1 && alphaOp != NPDISP_D3DTOP_SELECTARG2) return false;
	if (alphaOp && alphaOp != NPDISP_D3DTOP_DISABLE &&
		((alphaArg1 & ~NPDISP_D3DTA_SELECTMASK) || (alphaArg2 & ~NPDISP_D3DTA_SELECTMASK) ||
		 ((alphaArg1 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE) ||
		 ((alphaArg2 & NPDISP_D3DTA_SELECTMASK) > NPDISP_D3DTA_TEXTURE))) return false;
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

static bool npdisp_d3d_prepareTextures(NPDISP_D3D_CONTEXT* context, NPDISP_D3D_RASTERSTATE* state, std::vector<UINT8>* storage, NPDISP_D3D_TEXTURE* textures, bool directVideo)
{
	if (!context || !state || !storage || !textures) return false;
	for (UINT32 stage = 0; stage < NPDISP_D3D_RASTER_TEXTURE_STAGES; ++stage) {
		UINT32 colorOp = context->textureStageStates[stage][NPDISP_D3DTSS_COLOROP];
		if (stage && (!colorOp || colorOp == NPDISP_D3DTOP_DISABLE ||
			!context->textureStageStates[stage][NPDISP_D3DTSS_TEXTUREMAP])) break;
		if (!npdisp_d3d_prepareTextureStage(context, stage, state, &storage[stage], &textures[stage], directVideo)) return false;
		if (colorOp == NPDISP_D3DTOP_DISABLE) break;
	}
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
	return v;
}

static bool npdisp_d3d_point(NPDISP_D3D_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, UINT32 index, const NPDISP_D3D_RASTERSTATE* state)
{
	NPDISP_D3D_VERTEX vertex;
	if (!target || !vertices || !state || index >= vertexCount) return false;
	vertex = npdisp_d3d_vertex(&vertices[index]);
	return npdisp_d3d_sw_point(&target->sw, depthTarget, &vertex, state);
}

static bool npdisp_d3d_line(NPDISP_D3D_TARGET* target, NPDISP_D3D_SW_DEPTH_TARGET* depthTarget, const NPDISP_D3DTLVERTEX32* vertices, UINT32 vertexCount, UINT32 i0, UINT32 i1, const NPDISP_D3D_RASTERSTATE* state)
{
	NPDISP_D3D_VERTEX v0;
	NPDISP_D3D_VERTEX v1;
	if (!target || !vertices || !state || i0 >= vertexCount || i1 >= vertexCount) return false;
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
		!npdisp_d3d_drawState(context, &state) || !npdisp_d3d_prepareTextures(context, &state, textureStorage, textures, false)) return false;
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
	UINT32 count = indices ? indexCount : vertexCount;
	if (!context || !vertices || !context->lpTarget || count > NPDISP_D3D_MAX_VERTICES) return false;
	if (context->targetSnapshot.valid && context->targetSnapshot.systemMemory) {
		npdisp_d3d_flush();
		return npdisp_d3d_drawPrimitiveImmediate(context, primitiveType, vertices, vertexCount, indices, indexCount);
	}
	command = new NPDISP_D3D_ASYNC_COMMAND;
	if (!command) return false;
	memset(&command->target, 0, sizeof(command->target));
	memset(&command->depth, 0, sizeof(command->depth));
	memset(&command->state, 0, sizeof(command->state));
	memset(command->textures, 0, sizeof(command->textures));
	command->type = NPDISP_D3D_ASYNC_DRAW;
	command->targetSurface = context->lpTarget;
	command->primitiveType = primitiveType;
	command->hasDepth = false;
	command->clearFlags = 0;
	command->clearColor = 0;
	command->clearDepth = 0;
	command->clearStencil = 0;
	command->bytes = 0;
	if (!(context->targetSnapshot.valid ? npdisp_d3d_targetFromSnapshot(&context->targetSnapshot, &command->target) : npdisp_d3d_target(context->lpTarget, &command->target)) ||
		!npdisp_d3d_drawState(context, &command->state) || !npdisp_d3d_prepareTextures(context, &command->state, command->textureStorage, command->textures, true)) {
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
	command->vertices.assign(vertices, vertices + vertexCount);
	if (indices && indexCount) command->indices.assign(indices, indices + indexCount);
	if (!npdisp_d3d_asyncEnqueue(command)) {
		delete command;
		return npdisp_d3d_drawPrimitiveImmediate(context, primitiveType, vertices, vertexCount, indices, indexCount);
	}
	return true;
}

UINT32 npdisp_d3d_stateSize(void)
{
	UINT64 size = sizeof(NPDISP_D3D_STATE_HEADER);
	size += (UINT64)npdisp_d3d_contexts.size() * (sizeof(UINT32) + sizeof(NPDISP_D3D_CONTEXT));
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
	if (header.magic != NPDISP_D3D_STATE_MAGIC || header.version != NPDISP_D3D_STATE_VERSION ||
		!header.nextContext || !header.nextDDLocalNamespace || !header.nextLegacyTexture) return false;
	in = src + sizeof(header);
	remain = size - sizeof(header);
	if (!npdisp_d3d_stateReadMap(&in, &remain, header.contextCount, &npdisp_d3d_contexts) ||
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
	return true;
}

void npdisp_d3d_reset(void)
{
	npdisp_d3d_asyncStopWorker();
	npdisp_d3d_contexts.clear();
	npdisp_d3d_ddLocalNamespaces.clear();
	npdisp_d3d_surfaces.clear();
	npdisp_d3d_surfaceSwapPending.clear();
	npdisp_d3d_legacyTextures.clear();
	npdisp_d3d_palettes.clear();
	npdisp_d3d_surfacePalettes.clear();
	npdisp_d3d_nextContext = 1;
	npdisp_d3d_nextDDLocalNamespace = 1;
	npdisp_d3d_nextLegacyTexture = 1;
	InterlockedExchange(&npdisp_d3d_asyncVisibleDirty, 0);
	InterlockedExchange(&npdisp_d3d_asyncError, 0);
}

void npdisp_d3d_destroySurface(UINT32 lpSurfaceAddr)
{
	npdisp_d3d_flush();
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
			return NPDISP_DDHAL_DRIVER_NOTHANDLED;
		}
		context.lpTarget = targetLocal;
	}
	if (data.lpDDSZ) {
		NPDISP_D3D_SW_DEPTH_TARGET depthTarget = { 0 };
		context.lpDepth = data.lpDDSZ;
		if (!npdisp_d3d_depthTarget(context.lpDepth, &depthTarget)) {
			depthLocal = npdisp_d3d_indirectSurfaceLocal(data.lpDDSZ);
			if (!depthLocal || depthLocal == data.lpDDSZ || !npdisp_d3d_depthTarget(depthLocal, &depthTarget)) {
				TRACEOUTD3D(("NPDISP11 D3D_CTX_CREATE_REJECT stage=depth direct=%08x indirect=%08x", data.lpDDSZ, depthLocal));
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
	data.dwhContext = handle;
	data.ddrval = 0;
	if (!npdisp_d3d_write(&data, lpDataAddr, sizeof(data))) {
		npdisp_d3d_contexts.erase(handle);
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
			npdisp_d3d_destroyLegacyTextures(it->first);
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
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (!data.lpDDSZ) depthReady = true;
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
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	if (lpDepth && (depthTarget.width < target.sw.width || depthTarget.height < target.sw.height)) {
		TRACEOUTD3D(("NPDISP11 D3D_SETRENDERTARGET_REJECT stage=size target=%ux%u depth=%ux%u", target.sw.width, target.sw.height, depthTarget.width, depthTarget.height));
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
	return npdisp_d3d_lclBufferRead(data->lpDDCommands, surfaceOffset, dst, size);
}

static bool npdisp_d3d_dp2VertexSize(UINT32 fvf, UINT32* vertexSize)
{
	UINT32 size = 16U;
	UINT32 texCount;
	if (!vertexSize || (fvf & NPDISP_D3DFVF_POSITION_MASK) != NPDISP_D3DFVF_XYZRHW ||
		(fvf & (NPDISP_D3DFVF_RESERVED0 | NPDISP_D3DFVF_NORMAL | NPDISP_D3DFVF_RESERVED1 | NPDISP_D3DFVF_RESERVED2))) return false;
	texCount = (fvf & NPDISP_D3DFVF_TEXCOUNT_MASK) >> 8;
	if (texCount > NPDISP_D3D_MAX_TEXTURE_STAGES) return false;
	for (UINT32 i = texCount; i < NPDISP_D3D_MAX_TEXTURE_STAGES; ++i) {
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
	return vertexSize >= 16U && vertexSize <= NPDISP_D3D_MAX_VERTEX_SIZE && npdisp_d3d_dp2VertexSize(fvf, &minSize) && vertexSize >= minSize;
}

static bool npdisp_d3d_dp2VertexRead(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 index, NPDISP_D3DTLVERTEX32* vertex)
{
	UINT64 byteOffset;
	UINT32 offset;
	UINT32 attributeOffset = 16U;
	UINT32 texCount;
	if (!data || !vertex || index >= data->dwVertexLength) return false;
	byteOffset = (UINT64)data->dwVertexOffset + (UINT64)index * vertexSize;
	if (byteOffset > 0xffffffffULL) return false;
	offset = (UINT32)byteOffset;
	*vertex = NPDISP_D3DTLVERTEX32();
	vertex->rhw = 1.0f;
	vertex->color = 0xffffffffUL;
	vertex->specular = 0xff000000UL;
	texCount = (data->dwVertexType & NPDISP_D3DFVF_TEXCOUNT_MASK) >> 8;
	if (data->dwFlags & NPDISP_D3DHALDP2_USERMEMVERTICES) {
		UINT32 addr;
		UINT32 attrAddr;
		if (!npdisp_d3d_add(data->lpDDVertexOrVertices, offset, &addr) || !npdisp_d3d_read(vertex, addr, 16U)) return false;
		if (data->dwVertexType & NPDISP_D3DFVF_DIFFUSE) {
			if (!npdisp_d3d_add(addr, attributeOffset, &attrAddr) || !npdisp_d3d_read(&vertex->color, attrAddr, sizeof(vertex->color))) return false;
			attributeOffset += 4U;
		}
		if (data->dwVertexType & NPDISP_D3DFVF_SPECULAR) {
			if (!npdisp_d3d_add(addr, attributeOffset, &attrAddr) || !npdisp_d3d_read(&vertex->specular, attrAddr, sizeof(vertex->specular))) return false;
			attributeOffset += 4U;
		}
		for (UINT32 i = 0; i < texCount; ++i) {
			UINT32 format = (data->dwVertexType >> (16U + i * 2U)) & 3U;
			UINT32 dimensions = format == 3U ? 1U : format + 2U;
			if (i < 2U) {
				float* tu = i ? &vertex->tu2 : &vertex->tu;
				float* tv = i ? &vertex->tv2 : &vertex->tv;
				float* tw = i ? &vertex->tw2 : &vertex->tw;
				if (!npdisp_d3d_add(addr, attributeOffset, &attrAddr) || !npdisp_d3d_read(tu, attrAddr, sizeof(*tu))) return false;
				if (dimensions >= 2U) {
					if (!npdisp_d3d_add(attrAddr, 4U, &attrAddr) || !npdisp_d3d_read(tv, attrAddr, sizeof(*tv))) return false;
				}
				if (dimensions >= 3U) {
					if (!npdisp_d3d_add(addr, attributeOffset + 8U, &attrAddr) || !npdisp_d3d_read(tw, attrAddr, sizeof(*tw))) return false;
				}
			}
			attributeOffset += dimensions * 4U;
		}
	}
	else {
		UINT32 attrOffset;
		if (!npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, offset, vertex, 16U)) return false;
		if (data->dwVertexType & NPDISP_D3DFVF_DIFFUSE) {
			if (!npdisp_d3d_add(offset, attributeOffset, &attrOffset) || !npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, attrOffset, &vertex->color, sizeof(vertex->color))) return false;
			attributeOffset += 4U;
		}
		if (data->dwVertexType & NPDISP_D3DFVF_SPECULAR) {
			if (!npdisp_d3d_add(offset, attributeOffset, &attrOffset) || !npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, attrOffset, &vertex->specular, sizeof(vertex->specular))) return false;
			attributeOffset += 4U;
		}
		for (UINT32 i = 0; i < texCount; ++i) {
			UINT32 format = (data->dwVertexType >> (16U + i * 2U)) & 3U;
			UINT32 dimensions = format == 3U ? 1U : format + 2U;
			if (i < 2U) {
				float* tu = i ? &vertex->tu2 : &vertex->tu;
				float* tv = i ? &vertex->tv2 : &vertex->tv;
				float* tw = i ? &vertex->tw2 : &vertex->tw;
				if (!npdisp_d3d_add(offset, attributeOffset, &attrOffset) || !npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, attrOffset, tu, sizeof(*tu))) return false;
				if (dimensions >= 2U) {
					if (!npdisp_d3d_add(attrOffset, 4U, &attrOffset) || !npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, attrOffset, tv, sizeof(*tv))) return false;
				}
				if (dimensions >= 3U) {
					if (!npdisp_d3d_add(offset, attributeOffset + 8U, &attrOffset) || !npdisp_d3d_lclBufferRead(data->lpDDVertexOrVertices, attrOffset, tw, sizeof(*tw))) return false;
				}
			}
			attributeOffset += dimensions * 4U;
		}
	}
	return true;
}

static bool npdisp_d3d_dp2VerticesRead(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 first, UINT32 count, std::vector<NPDISP_D3DTLVERTEX32>* vertices)
{
	UINT64 end;
	if (!data || !vertices || count > NPDISP_D3D_MAX_VERTICES) return false;
	end = (UINT64)first + count;
	if (end > data->dwVertexLength) return false;
	vertices->resize(count);
	for (UINT32 i = 0; i < count; ++i) if (!npdisp_d3d_dp2VertexRead(data, vertexSize, first + i, &(*vertices)[i])) return false;
	return true;
}

static bool npdisp_d3d_dp2CommandVertexRead(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 offset, NPDISP_D3DTLVERTEX32* vertex)
{
	UINT32 attributeOffset = 16U;
	UINT32 addr;
	UINT32 texCount;
	if (!data || !vertex) return false;
	*vertex = NPDISP_D3DTLVERTEX32();
	vertex->rhw = 1.0f;
	vertex->color = 0xffffffffUL;
	vertex->specular = 0xff000000UL;
	texCount = (data->dwVertexType & NPDISP_D3DFVF_TEXCOUNT_MASK) >> 8;
	if (!npdisp_d3d_dp2CommandRead(data, offset, vertex, 16U)) return false;
	if (data->dwVertexType & NPDISP_D3DFVF_DIFFUSE) {
		if (!npdisp_d3d_add(offset, attributeOffset, &addr) || !npdisp_d3d_dp2CommandRead(data, addr, &vertex->color, sizeof(vertex->color))) return false;
		attributeOffset += 4U;
	}
	if (data->dwVertexType & NPDISP_D3DFVF_SPECULAR) {
		if (!npdisp_d3d_add(offset, attributeOffset, &addr) || !npdisp_d3d_dp2CommandRead(data, addr, &vertex->specular, sizeof(vertex->specular))) return false;
		attributeOffset += 4U;
	}
	for (UINT32 i = 0; i < texCount; ++i) {
		UINT32 format = (data->dwVertexType >> (16U + i * 2U)) & 3U;
		UINT32 dimensions = format == 3U ? 1U : format + 2U;
		if (i < 2U) {
			float* tu = i ? &vertex->tu2 : &vertex->tu;
			float* tv = i ? &vertex->tv2 : &vertex->tv;
			float* tw = i ? &vertex->tw2 : &vertex->tw;
			if (!npdisp_d3d_add(offset, attributeOffset, &addr) || !npdisp_d3d_dp2CommandRead(data, addr, tu, sizeof(*tu))) return false;
			if (dimensions >= 2U) {
				if (!npdisp_d3d_add(addr, 4U, &addr) || !npdisp_d3d_dp2CommandRead(data, addr, tv, sizeof(*tv))) return false;
			}
			if (dimensions >= 3U) {
				if (!npdisp_d3d_add(offset, attributeOffset + 8U, &addr) || !npdisp_d3d_dp2CommandRead(data, addr, tw, sizeof(*tw))) return false;
			}
		}
		attributeOffset += dimensions * 4U;
	}
	return true;
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

static bool npdisp_d3d_dp2ImmediateVerticesRead(const NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32* data, UINT32 vertexSize, UINT32 offset, UINT32 count, std::vector<NPDISP_D3DTLVERTEX32>* vertices)
{
	UINT64 bytes64;
	if (!data || !vertices || count > NPDISP_D3D_MAX_VERTICES) return false;
	bytes64 = (UINT64)vertexSize * count;
	if (bytes64 > 0xffffffffULL || (UINT64)offset + bytes64 > data->dwCommandLength) return false;
	vertices->resize(count);
	for (UINT32 i = 0; i < count; ++i) {
		UINT64 vertexOffset = (UINT64)offset + (UINT64)i * vertexSize;
		if (vertexOffset > 0xffffffffULL || !npdisp_d3d_dp2CommandVertexRead(data, (UINT32)vertexOffset, &(*vertices)[i])) return false;
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
	if (!npdisp_d3d_dp2VerticesRead(data, vertexSize, start.wVStart, maxIndex + 1U, &vertices)) {
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
	if (!npdisp_d3d_dp2VerticesRead(data, vertexSize, start.wVStart, maxIndex + 1U, &vertices)) {
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
	if (!npdisp_d3d_dp2VerticesRead(data, vertexSize, start.wVStart, maxIndex + 1U, &vertices) ||
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
	if (!npdisp_d3d_dp2VerticesRead(data, vertexSize, start.wVStart, maxIndex + 1U, &vertices) ||
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
	npdisp_d3d_flush();
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
	UINT32 vertexSize;
	UINT32 cursor = 0;
	if (!npdisp_d3d_read(&data, lpDataAddr, sizeof(data)) || !(context = npdisp_d3d_getContext(data.dwhContext)) ||
		data.dwCommandLength > NPDISP_D3D_MAX_DP2_COMMAND_BYTES) {
		TRACEOUTD3D(("NPDISP11 D3D_DP2_REJECT stage=header data=%08x", lpDataAddr));
		return NPDISP_DDHAL_DRIVER_NOTHANDLED;
	}
	TRACEOUTD3D(("NPDISP11 D3D_DP2_IN ctx=%08x flags=%08x fvf=%08x vsize=%u cmd=%08x off=%u len=%u vert=%08x voff=%u vlen=%u", data.dwhContext, data.dwFlags, data.dwVertexType, data.dwVertexSizeOrDdrval, data.lpDDCommands, data.dwCommandOffset, data.dwCommandLength, data.lpDDVertexOrVertices, data.dwVertexOffset, data.dwVertexLength));
	if (data.dwFlags & NPDISP_D3DHALDP2_EXECUTEBUFFER) TRACEOUTD3D(("NPDISP11 D3D_DP2_EXECUTEBUFFER ctx=%08x cmd=%08x vert=%08x rstates=%08x ns=%08x", data.dwhContext, data.lpDDCommands, data.lpDDVertexOrVertices, data.lpdwRStates, context->ddLocalNamespace));
	if (data.dwFlags & ~NPDISP_D3DHALDP2_VALID_FLAGS) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset);
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

		if ((data.dwFlags & NPDISP_D3DHALDP2_EXECUTEBUFFER) && command.bCommand == NPDISP_D3DOP_EXIT) {
			TRACEOUTD3D(("NPDISP11 D3D_EXECBUF_EXIT ctx=%08x offset=%u", data.dwhContext, data.dwCommandOffset + commandStart));
			return npdisp_d3d_dp2Finish(lpDataAddr, &data, 0, 0);
		}
		if (command.bCommand == NPDISP_D3DDP2OP_RENDERSTATE) {
			UINT64 bytes64 = (UINT64)command.wCount * sizeof(NPDISP_D3DHAL_DP2RENDERSTATE32);
			std::vector<NPDISP_D3DHAL_DP2RENDERSTATE32> renderStates;
			std::vector<NPDISP_D3DSTATE32> states;
			if (bytes64 > 0xffffffffULL || command.wCount > NPDISP_D3D_MAX_RENDERSTATES || (UINT64)cursor + bytes64 > data.dwCommandLength) return npdisp_d3d_dp2Finish(lpDataAddr, &data, NPDISP_D3DERR_COMMAND_UNPARSED, data.dwCommandOffset + commandStart);
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
					(points.wCount && (!npdisp_d3d_dp2VerticesRead(&data, vertexSize, points.wVStart, points.wCount, &vertices) ||
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
			if (vertexCount && (!npdisp_d3d_dp2VerticesRead(&data, vertexSize, start.wVStart, vertexCount, &vertices) ||
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
			if (bytes64 > 0xffffffffULL || (vertexCount && (!npdisp_d3d_dp2ImmediateVerticesRead(&data, vertexSize, cursor, vertexCount, &vertices) ||
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
			if (bytes64 > 0xffffffffULL || (vertexCount && (!npdisp_d3d_dp2ImmediateVerticesRead(&data, vertexSize, cursor, vertexCount, &vertices) ||
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
			if (vertexCount && (!npdisp_d3d_dp2VerticesRead(&data, vertexSize, start.wVStart, vertexCount, &vertices) ||
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
