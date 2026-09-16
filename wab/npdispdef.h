/**
 * @file	npdispdef.h
 * @brief	Definition of the Neko Project II Display Adapter
 */

#pragma once

// NewFontSeg対応
#define SUPPORT_NPDISP_NEWFONTSEG

#if defined(SUPPORT_WAB_NPDISP)

#define NPDISP_DEVTYPE_DIBENG	0x5250
#define NPDISP_DEVTYPE			NPDISP_DEVTYPE_DIBENG
#define NPDISP_DEVTYPE_DDB		0x2222

// 特殊DDBで使用するdeFlags互換値　0以外ならなんでもよいが普通のDIBSectionと思われない値がよい
#define NPDISP_WING_DDB_DEFLAGS	0x8800

#define NPDISP_EXEC_MAGIC	0x3132504e

#define NPDISP_RETCODE_NONE		0
#define NPDISP_RETCODE_SUCCESS	1
#define NPDISP_RETCODE_FAILED	2

// 関数番号　特に意味は無いがSDK記載の序数と合わせておく
#define NPDISP_FUNCORDER_NP2INITIALIZE			0 // 初期化用
#define NPDISP_FUNCORDER_Enable					5
#define NPDISP_FUNCORDER_Disable				4
#define NPDISP_FUNCORDER_RealizeObject			10
#define NPDISP_FUNCORDER_BitBlt					1
#define NPDISP_FUNCORDER_BitmapBits				30
#define NPDISP_FUNCORDER_ColorInfo				2
#define NPDISP_FUNCORDER_Control				3
#define NPDISP_FUNCORDER_CreateDIBitmap			20
#define NPDISP_FUNCORDER_DeviceBitmap			16
#define NPDISP_FUNCORDER_DeviceBitmapBits		19
#define NPDISP_FUNCORDER_DeviceMode				13
#define NPDISP_FUNCORDER_EnumDFonts				6
#define NPDISP_FUNCORDER_EnumObj				7
#define NPDISP_FUNCORDER_ExtDeviceMode			90
#define NPDISP_FUNCORDER_ExtTextOut				14
#define NPDISP_FUNCORDER_GetCharWidth			15
#define NPDISP_FUNCORDER_GetDriverResourceID	450
#define NPDISP_FUNCORDER_GetPalette				23
#define NPDISP_FUNCORDER_GetPalTrans			25
#define NPDISP_FUNCORDER_Output					8
#define NPDISP_FUNCORDER_Pixel					9
#define NPDISP_FUNCORDER_ScanLR					12
#define NPDISP_FUNCORDER_SelectBitmap			29
#define NPDISP_FUNCORDER_SetAttribute			18
#define NPDISP_FUNCORDER_SetDIBitsToDevice		21
#define NPDISP_FUNCORDER_SetPalette				22
#define NPDISP_FUNCORDER_SetPalTrans			24
#define NPDISP_FUNCORDER_StrBlt					11
#define NPDISP_FUNCORDER_StretchBlt				27
#define NPDISP_FUNCORDER_StretchDIBits			28
#define NPDISP_FUNCORDER_UpdateColors			26
#define NPDISP_FUNCORDER_CheckCursor			104
#define NPDISP_FUNCORDER_FastBorder				17
#define NPDISP_FUNCORDER_Inquire				101
#define NPDISP_FUNCORDER_MoveCursor				103
#define NPDISP_FUNCORDER_SaveScreenBitmap		92
#define NPDISP_FUNCORDER_SetCursor				102
#define NPDISP_FUNCORDER_UserRepaintDisable		500 // DDK HELPにないがこれがないとプログラム終了時に例外 
#define NPDISP_FUNCORDER_DCI_BEGINACCESS		0xfe00
#define NPDISP_FUNCORDER_DCI_ENDACCESS			0xfe01
#define NPDISP_FUNCORDER_DCI_DESTROYSURFACE		0xfe02
#define NPDISP_FUNCORDER_DD32_DISPATCH			0xfe30

// 32bit HAL DLLから受け取るDirectDraw 2D callback ID。
#define NPDISP_DDBRIDGE_CB_DD_CREATESURFACE		0x0001UL
#define NPDISP_DDBRIDGE_CB_DD_SETCOLORKEY		0x0002UL
#define NPDISP_DDBRIDGE_CB_DD_SETMODE			0x0003UL
#define NPDISP_DDBRIDGE_CB_DD_WAITVB			0x0004UL
#define NPDISP_DDBRIDGE_CB_DD_CANCREATESURFACE	0x0005UL
#define NPDISP_DDBRIDGE_CB_DD_CREATEPALETTE		0x0006UL
#define NPDISP_DDBRIDGE_CB_DD_GETSCANLINE		0x0007UL
#define NPDISP_DDBRIDGE_CB_DD_SETEXCLUSIVEMODE	0x0008UL
#define NPDISP_DDBRIDGE_CB_DD_FLIPTOGDI			0x0009UL
#define NPDISP_DDBRIDGE_CB_DD_GETDRIVERINFO		0x000aUL
#define NPDISP_DDBRIDGE_CB_SURF_DESTROY			0x0100UL
#define NPDISP_DDBRIDGE_CB_SURF_FLIP			0x0101UL
#define NPDISP_DDBRIDGE_CB_SURF_SETCLIPLIST		0x0102UL
#define NPDISP_DDBRIDGE_CB_SURF_LOCK			0x0103UL
#define NPDISP_DDBRIDGE_CB_SURF_UNLOCK			0x0104UL
#define NPDISP_DDBRIDGE_CB_SURF_BLT				0x0105UL
#define NPDISP_DDBRIDGE_CB_SURF_SETCOLORKEY		0x0106UL
#define NPDISP_DDBRIDGE_CB_SURF_ADDATTACHED		0x0107UL
#define NPDISP_DDBRIDGE_CB_SURF_GETBLTSTATUS	0x0108UL
#define NPDISP_DDBRIDGE_CB_SURF_GETFLIPSTATUS	0x0109UL
#define NPDISP_DDBRIDGE_CB_SURF_UPDATEOVERLAY	0x010aUL
#define NPDISP_DDBRIDGE_CB_SURF_SETOVERLAYPOS	0x010bUL
#define NPDISP_DDBRIDGE_CB_SURF_SETPALETTE		0x010dUL
#define NPDISP_DDBRIDGE_CB_PAL_DESTROY			0x0200UL
#define NPDISP_DDBRIDGE_CB_PAL_SETENTRIES		0x0201UL

#if defined(SUPPORT_NPDISP_D3D)
#define NPDISP_DDBRIDGE_CB_D3D_CONTEXTCREATE			0x0300UL
#define NPDISP_DDBRIDGE_CB_D3D_CONTEXTDESTROY			0x0301UL
#define NPDISP_DDBRIDGE_CB_D3D_CONTEXTDESTROYALL		0x0302UL
#define NPDISP_DDBRIDGE_CB_D3D_SCENECAPTURE				0x0306UL
#define NPDISP_DDBRIDGE_CB_D3D_RENDERSTATE				0x0303UL
#define NPDISP_DDBRIDGE_CB_D3D_RENDERPRIMITIVE			0x0304UL
#define NPDISP_DDBRIDGE_CB_D3D_GETSTATE					0x0305UL
#define NPDISP_DDBRIDGE_CB_D3D_TEXTURECREATE			0x0307UL
#define NPDISP_DDBRIDGE_CB_D3D_TEXTUREDESTROY			0x0308UL
#define NPDISP_DDBRIDGE_CB_D3D_TEXTURESWAP				0x0309UL
#define NPDISP_DDBRIDGE_CB_D3D_TEXTUREGETSURF			0x030aUL
#define NPDISP_DDBRIDGE_CB_D3D_SETRENDERTARGET			0x0320UL
#define NPDISP_DDBRIDGE_CB_D3D_CLEAR					0x0321UL
#define NPDISP_DDBRIDGE_CB_D3D_DRAWONEPRIMITIVE			0x0322UL
#define NPDISP_DDBRIDGE_CB_D3D_DRAWONEINDEXEDPRIMITIVE	0x0323UL
#define NPDISP_DDBRIDGE_CB_D3D_CREATESURFACEEX			0x0310UL
#define NPDISP_DDBRIDGE_CB_D3D_DESTROYDDLOCAL			0x0311UL
#define NPDISP_DDBRIDGE_CB_D3D_GETDRIVERSTATE			0x0312UL
#define NPDISP_DDBRIDGE_CB_D3D_ALPHABLT					0x0313UL
#define NPDISP_DDBRIDGE_CB_D3D_DRAWPRIMITIVES			0x0324UL
#define NPDISP_DDBRIDGE_CB_D3D_DRAWPRIMITIVES2			0x0330UL
#define NPDISP_DDBRIDGE_CB_D3D_VALIDATESTAGE			0x0331UL
#define NPDISP_DDBRIDGE_CB_D3D_CLEAR2					0x0332UL
#endif
#define NPDISP_FUNCORDER_INT2Fh					0xff2f // 序数がないので0xff2fとしておく
#define NPDISP_FUNCORDER_MEMORYMAP				0xfffc // 序数がないので0xfffcとしておく
#define NPDISP_FUNCORDER_WEP					0xffff // 序数がないので0xffffとしておく
// 以降 Win9x用
#define NPDISP_FUNCORDER_ReEnable				31
#define NPDISP_FUNCORDER_ValidateMode			700
#define NPDISP_FUNCORDER_SelectBitmap			29
#define NPDISP_FUNCORDER_BitmapBits				30

#define NPDISP_DT_RASDISPLAY	1

#define NPDISP_CC_CIRCLES		1
#define NPDISP_CC_PIE			2
#define NPDISP_CC_CHORD			4
#define NPDISP_CC_ELLIPSES		8
#define NPDISP_CC_WIDE			16
#define NPDISP_CC_STYLED		32
#define NPDISP_CC_WIDESTYLED	64
#define NPDISP_CC_INTERIORS		128
#define NPDISP_CC_ROUNDRECT		256
#define NPDISP_CC_POLYBEZIER	512

#define NPDISP_LC_POLYLINE		2
#define NPDISP_LC_MARKER		4
#define NPDISP_LC_POLYMARKER	8
#define NPDISP_LC_WIDE			16
#define NPDISP_LC_STYLED		32
#define NPDISP_LC_WIDESTYLED	64
#define NPDISP_LC_INTERIORS		128

#define NPDISP_PC_POLYGON		1
#define NPDISP_PC_RECTANGLE		2
#define NPDISP_PC_WINDPOLYGON	4
#define NPDISP_PC_SCANLINE		8
#define NPDISP_PC_WIDE			16
#define NPDISP_PC_STYLED		32
#define NPDISP_PC_WIDESTYLED	64
#define NPDISP_PC_INTERIORS		128
#define NPDISP_PC_POLYPOLYGON	256
#define NPDISP_PC_PATHS			512

#define NPDISP_CP_RECTANGLE		1

#define NPDISP_TC_OP_CHARACTER	0x0001
#define NPDISP_TC_OP_STROKE		0x0002
#define NPDISP_TC_CP_STROKE		0x0004
#define NPDISP_TC_CR_90			0x0008
#define NPDISP_TC_CR_ANY		0x0010
#define NPDISP_TC_SF_X_YINDEP	0x0020
#define NPDISP_TC_SA_DOUBLE		0x0040
#define NPDISP_TC_SA_INTEGER	0x0080
#define NPDISP_TC_SA_CONTIN		0x0100
#define NPDISP_TC_EA_DOUBLE		0x0200
#define NPDISP_TC_IA_ABLE		0x0400
#define NPDISP_TC_UA_ABLE		0x0800
#define NPDISP_TC_SO_ABLE		0x1000
#define NPDISP_TC_RA_ABLE		0x2000
#define NPDISP_TC_VA_ABLE		0x4000

#define NPDISP_RC_BITBLT		0x0001
#define NPDISP_RC_BANDING		0x0002
#define NPDISP_RC_SCALING		0x0004
#define NPDISP_RC_BITMAP64		0x0008
#define NPDISP_RC_GDI20_OUTPUT	0x0010
#define NPDISP_RC_GDI20_STATE	0x0020
#define NPDISP_RC_SAVEBITMAP	0x0040
#define NPDISP_RC_DI_BITMAP		0x0080
#define NPDISP_RC_PALETTE		0x0100
#define NPDISP_RC_DIBTODEV		0x0200
#define NPDISP_RC_BIGFONT		0x0400
#define NPDISP_RC_STRETCHBLT	0x0800
#define NPDISP_RC_FLOODFILL		0x1000
#define NPDISP_RC_STRETCHDIB	0x2000
#define NPDISP_RC_OP_DX_OUTPUT	0x4000
#define NPDISP_RC_DEVBITS		0x8000

#define NPDISP_C1_TRANSPARENT	0x0001
#define NPDISP_C1_DIBENGINE 	0x0010
#define NPDISP_C1_REINIT_ABLE	0x0080
#define NPDISP_C1_GLYPH_INDEX	0x0100
#define NPDISP_C1_BIT_PACKED	0x0200
#define NPDISP_C1_BYTE_PACKED	0x0400
#define NPDISP_C1_COLORCURSOR	0x0800
#define NPDISP_C1_SLOW_CARD		0x2000

#define NPDISP_PEN_STYLE_SOLID			0
#define NPDISP_PEN_STYLE_DASHED			1
#define NPDISP_PEN_STYLE_DOTTED			2
#define NPDISP_PEN_STYLE_DOTDASHED		3
#define NPDISP_PEN_STYLE_DASHDOTDOT 	4
#define NPDISP_PEN_STYLE_NOLINE			5
#define NPDISP_PEN_STYLE_INSIDEFRAME	6

#define NPDISP_BRUSH_STYLE_SOLID		0
#define NPDISP_BRUSH_STYLE_HOLLOW		1
#define NPDISP_BRUSH_STYLE_HATCHED		2
#define NPDISP_BRUSH_STYLE_PATTERN		3

#define NPDISP_BRUSH_HATCH_HORIZONTAL	0
#define NPDISP_BRUSH_HATCH_VERTICAL		1
#define NPDISP_BRUSH_HATCH_FDIAGONAL	2
#define NPDISP_BRUSH_HATCH_BDIAGONAL	3
#define NPDISP_BRUSH_HATCH_CROSS		4
#define NPDISP_BRUSH_HATCH_DIAGCROSS	5

#define NPDISP_DBB_SET 1
#define NPDISP_DBB_GET 2
#define NPDISP_DBB_COPY 4
#define NPDISP_DBB_SETWITHFILLER 8

#define NPDISP_QDI_SETDIBITS                1
#define NPDISP_QDI_GETDIBITS                2
#define NPDISP_QDI_DIBTOSCREEN              4
#define NPDISP_QDI_STRETCHDIB               8

#define NPDISP_VALMODE_YES			0
#define NPDISP_VALMODE_NO_WRONGDRV	1
#define NPDISP_VALMODE_NO_NOMEM		2
#define NPDISP_VALMODE_NO_NODAC		3
#define NPDISP_VALMODE_NO_UNKNOWN	4

#define NPDISP_CONTROL_SETCOLORTABLE        4
#define NPDISP_CONTROL_GETCOLORTABLE        5
#define NPDISP_CONTROL_QUERYESCSUPPORT		8
#define NPDISP_CONTROL_QUERYDIBSUPPORT		3073
#define NPDISP_CONTROL_DCICOMMAND			3075

#define NPDISP_CONTROL_OPENGL_CMD			4352
#define NPDISP_CONTROL_OPENGL_GETINFO		4353
#define NPDISP_CONTROL_WNDOBJ_SETUP			4354

#define NPDISP_CONTROL_NP2DCIENABLE			0x7222
#define NPDISP_CONTROL_NP2DCIDISABLE		0x7223

#define NPDISP_CONTROL_DCI_DCICREATEPRIMARYSURFACE		1
#define NPDISP_CONTROL_DCI_DCICREATEOFFSCREENSURFACE	2
#define NPDISP_CONTROL_DCI_DCICREATEOVERLAYSURFACE		3
#define NPDISP_CONTROL_DCI_DCIENUMSURFACE				4
#define NPDISP_CONTROL_DCI_DCIESCAPE					5

#define NPDISP_CONTROL_DCI_DDCREATEDRIVEROBJECT	10
#define NPDISP_CONTROL_DCI_DDGET32BITDRIVERNAME	11
#define NPDISP_CONTROL_DCI_DDNEWCALLBACKFNS		12
#define NPDISP_CONTROL_DCI_DDVERSIONINFO			13

#define NPDISP_FONT_CACHE_MAX	16

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

	typedef struct _tagNPDISP_REQUEST
	{
		UINT16 version;
		UINT16 funcOrder;
		UINT16 returnCode;
		UINT16 reserved;
		union
		{
			struct
			{
				UINT16 dpiX;
				UINT16 dpiY;
				UINT16 width;
				UINT16 height;
				UINT16 bpp;
				UINT16 isWin9x;
				UINT32 bmpinfoAddr;
				UINT32 beginAccessAddr;
				UINT32 endAccessAddr;
				UINT32 dcibufAddr;
				UINT32 dciBeginAccessAddr;
				UINT32 dciEndAccessAddr;
				UINT32 dciDestroySurfaceAddr;
				UINT32 vramLinearAddr;
				UINT32 vramPhysicalAddr;
				UINT32 ddCallbacksAddr;
				UINT32 ddSurfaceCallbacksAddr;
				UINT32 ddHalInfoAddr;
				UINT32 ddModeInfoAddr;
				UINT32 ddPaletteCallbacksAddr;
				UINT32 ddVidMemAddr;
				UINT32 ddBridgeInfoAddr;
				UINT32 d3dGlobalDriverDataAddr;
				UINT32 d3dHalCallbacksAddr;
			} init;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDevInfoAddr;
				UINT16 wStyle;
				UINT32 lpDestDevTypeAddr;
				UINT32 lpOutputFileAddr;
				UINT32 lpDataAddr;
			} enable;
			struct
			{
				UINT32 lpDestDevAddr;
			} disable;
			struct
			{
				UINT32 lpRetValueAddr;
				SINT16 iResId;
				UINT32 lpResTypeAddr;
			} GetDriverResourceID;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT32 dwColorin;
				UINT32 lpPColorAddr;
			} ColorInfo;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT16 wStyle;
				UINT32 lpInObjAddr;
				UINT32 lpOutObjAddr;
				UINT32 lpTextXFormAddr;
			} RealizeObject;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT16 wFunction;
				UINT32 lpInDataAddr;
				UINT32 lpOutDataAddr;
			} Control;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				SINT16 wDestX;
				SINT16 wDestY;
				UINT32 lpSrcDevAddr;
				SINT16 wSrcX;
				SINT16 wSrcY;
				UINT16 wXext;
				UINT16 wYext;
				UINT32 Rop3;
				UINT32 lpPBrushAddr;
				UINT32 lpDrawModeAddr;
			} BitBlt;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpBitmapAddr;
				UINT16 fGet;
				UINT16 iStart;
				UINT16 cScans;
				UINT32 lpDIBitsAddr;
				UINT32 lpBitmapInfoAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpTranslateAddr;
			} DeviceBitmapBits;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				SINT16 X;
				SINT16 Y;
				UINT16 iScan;
				UINT16 cScans;
				UINT32 lpClipRectAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpDIBitsAddr;
				UINT32 lpBitmapInfoAddr;
				UINT32 lpTranslateAddr;
			} SetDIBitsToDevice;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpRect;
				UINT16 wCommand;
			} SaveScreenBitmap;
			struct
			{
				UINT16 wAbsX;
				UINT16 wAbsY;
			} MoveCursor;
			struct
			{
				UINT32 lpCursorShapeAddr;
			} SetCursor;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				SINT16 wDestXOrg;
				SINT16 wDestYOrg;
				UINT32 lpClipRectAddr;
				UINT32 lpStringAddr;
				SINT16 wCount;
				UINT32 lpFontInfoAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpTextXFormAddr;
				UINT32 lpCharWidthsAddr;
				UINT32 lpOpaqueRectAddr;
				UINT16 wOptions;
			} extTextOut;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				SINT16 wDestXOrg;
				SINT16 wDestYOrg;
				UINT32 lpClipRectAddr;
				UINT32 lpStringAddr;
				SINT16 wCount;
				UINT32 lpFontInfoAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpTextXFormAddr;
			} strBlt;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT16 wStyle;
				UINT16 wCount;
				UINT32 lpPointsAddr;
				UINT32 lpPPenAddr;
				UINT32 lpPBrushAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpClipRectAddr;
			} output;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpRectAddr;
				UINT16 wHorizBorderThick;
				UINT16 wVertBorderThick;
				UINT32 dwRasterOp;
				UINT32 lpDestDevAddr;
				UINT32 lpPBrushAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpClipRectAddr;
			} fastBorder;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT16 X;
				UINT16 Y;
				UINT32 dwPhysColor;
				UINT32 lpDrawModeAddr;
			} pixel;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT16 X;
				UINT16 Y;
				UINT32 dwPhysColor;
				UINT16 Style;
			} scanLR;
			struct
			{
				UINT32 lpRetValueAddr; // 0=Complete, 1=hasData
				UINT32 lpDestDevAddr;
				UINT16 wStyle; // 1=pen, 2=brush
				UINT16 enumIdx; // 返すオブジェクトの要素番号
				UINT32 lpLogObjAddr; // オブジェクトの内容書き込み先
			} enumObj;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				SINT16 wDestX;
				SINT16 wDestY;
				SINT16 wDestXext;
				SINT16 wDestYext;
				UINT32 lpSrcDevAddr;
				SINT16 wSrcX;
				SINT16 wSrcY;
				SINT16 wSrcXext;
				SINT16 wSrcYext;
				UINT32 Rop3;
				UINT32 lpPBrushAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpClipAddr;
			} stretchBlt;
			struct
			{
				UINT16 nStartIndex;
				UINT16 nNumEntries;
				UINT32 lpPaletteAddr;
			} getPalette;
			struct
			{
				UINT16 nStartIndex;
				UINT16 nNumEntries;
				UINT32 lpPaletteAddr;
			} setPalette;
			struct
			{
				UINT32 lpIndexesAddr;
			} getPalTrans;
			struct
			{
				UINT32 lpIndexesAddr;
			} setPalTrans;
			struct
			{
				SINT16 wStartX;
				SINT16 wStartY;
				UINT16 wExtX;
				UINT16 wExtY;
				UINT32 lpTranslateAddr;
			} updateColors;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDestDevAddr;
				UINT32 lpBufferAddr;
				UINT16 wFirstChar;
				UINT16 wLastChar;
				UINT32 lpFontInfoAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpFontTransAddr;
			} getCharWidth;
			struct
			{
				UINT16 ax;
			} INT2Fh;
			struct
			{
				UINT32 bSystemExit;
			} WEP;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpPDeviceAddr;
				UINT32 lpGDIInfoAddr;
			} reEnable;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpValModeAddr;
			} validateMode;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDeviceAddr;
				UINT32 lpPrevBitmapAddr;
				UINT32 lpBitmapAddr;
				UINT32 fFlags;
			} selectBitmap;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpDeviceAddr;
				UINT32 fFlags;
				UINT32 dwCount;
				UINT32 lpBitsAddr;
			} bitmapBits;
			struct
			{
				UINT32 lpRetValueAddr;
				UINT32 lpPDevice;
				UINT16 fGet;
				SINT16 DestX;
				SINT16 DestY;
				SINT16 DestXE;
				SINT16 DestYE;
				UINT16 SrcX;
				UINT16 SrcY;
				UINT16 SrcXE;
				UINT16 SrcYE;
				UINT32 lpBitsAddr;
				UINT32 lpBitmapInfoAddr;
				UINT32 lpTranslateAddr;
				UINT32 dwROP;
				UINT32 lpPBrushAddr;
				UINT32 lpDrawModeAddr;
				UINT32 lpClipRecAddr;
			} stretchDIBits;
			struct
			{
				UINT32 physicalAddr;
				UINT32 linearAddr;
				UINT16 farSelector;
				UINT32 farOffset;
			} MEMORYMAP;
			struct
			{
				UINT32 lpDeviceAddr;
				UINT32 lpRectAddr;
			} DCI_BeginAccess;
			struct
			{
				UINT32 lpDeviceAddr;
			} DCI_EndAccess;
			struct
			{
				UINT32 lpDeviceAddr;
			} DCI_DestroySurface;
			struct
			{
				UINT16 arguments[20];
			} others;
		} parameters;
	} NPDISP_REQUEST;

#pragma pack(pop)

#pragma pack(push, 2)
	typedef struct {
		SINT16  x;
		SINT16  y;
	} NPDISP_POINT;
	typedef struct {
		SINT16 left;
		SINT16 top;
		SINT16 right;
		SINT16 bottom;
	} NPDISP_RECT;
	typedef struct {
		SINT16 dpVersion;
		SINT16 dpTechnology;
		SINT16 dpHorzSize;
		SINT16 dpVertSize;
		SINT16 dpHorzRes;
		SINT16 dpVertRes;
		SINT16 dpBitsPixel;
		SINT16 dpPlanes;
		SINT16 dpNumBrushes;
		SINT16 dpNumPens;
		SINT16 futureuse;
		SINT16 dpNumFonts;
		SINT16 dpNumColors;
		UINT16 dpDEVICEsize;
		UINT16 dpCurves;
		UINT16 dpLines;
		UINT16 dpPolygonals;
		UINT16 dpText;
		UINT16 dpClip;
		UINT16 dpRaster;
		SINT16 dpAspectX;
		SINT16 dpAspectY;
		SINT16 dpAspectXY;
		SINT16 dpStyleLen;
		NPDISP_POINT dpMLoWin;
		NPDISP_POINT dpMLoVpt;
		NPDISP_POINT dpMHiWin;
		NPDISP_POINT dpMHiVpt;
		NPDISP_POINT dpELoWin;
		NPDISP_POINT dpELoVpt;
		NPDISP_POINT dpEHiWin;
		NPDISP_POINT dpEHiVpt;
		NPDISP_POINT dpTwpWin;
		NPDISP_POINT dpTwpVpt;
		SINT16 dpLogPixelsX;
		SINT16 dpLogPixelsY;
		SINT16 dpDCManage;
		SINT16 dpCaps1;
		SINT32 dpSpotSizeX;
		SINT32 dpSpotSizeY;
		SINT16 dpPalColors;
		SINT16 dpPalReserved;
		SINT16 dpPalResolution;
	} NPDISP_GDIINFO;
	typedef struct {
		char dmDeviceName[32];
		UINT16 dmSpecVersion;
		UINT16 dmDriverVersion;
		UINT16 dmSize;
		UINT16 dmDriverExtra;
		UINT32 dmFields;
		SINT16 dmOrientation;
		SINT16 dmPaperSize;
		SINT16 dmPaperLength;
		SINT16 dmPaperWidth;
		SINT16 dmScale;
		SINT16 dmCopies;
		SINT16 dmDefaultSource;
		SINT16 dmPrintQuality;
		SINT16 dmColor;
		SINT16 dmDuplex;
		SINT16 dmYResolution;
		SINT16 dmTTOption;
	} NPDISP_DEVMODE;
	typedef struct {
		SINT16 txfHeight;
		SINT16 txfWidth;
		SINT16 txfEscapement;
		SINT16 txfOrientation;
		SINT16 txfWeight;
		char txfItalic;
		char txfUnderline;
		char txfStrikeOut;
		char txfOutPrecision;
		char txfClipPrecision;
		SINT16 txfAccelerator;
		SINT16 txfOverhang;
	} NPDISP_TEXTXFORM;
	typedef struct {
		SINT16 opnStyle;
		NPDISP_POINT lopnWidth;
		SINT32 lopnColor;
	} NPDISP_LPEN;
	typedef struct {
		SINT16 lbStyle;
		SINT32 lbColor;
		SINT16 lbHatch;
		SINT32 lbBkColor;
	} NPDISP_LBRUSH;
	typedef struct {
		SINT16 lfHeight;
		SINT16 lfWidth;
		SINT16 lfEscapement;
		SINT16 lfOrientation;
		SINT16 lfWeight;
		UINT8 lfItalic;
		UINT8 lfUnderline;
		UINT8 lfStrikeOut;
		UINT8 lfCharSet;
		UINT8 lfOutPrecision;
		UINT8 lfClipPrecision;
		UINT8 lfQuality;
		UINT8 lfPitchAndFamily;
		char lfFaceName[32];
	} NPDISP_LFONT;

	typedef struct {
		SINT16 bmType;
		SINT16 bmWidth;
		SINT16 bmHeight;
		SINT16 bmWidthBytes;
		UINT8 bmPlanes;
		UINT8 bmBitsPixel;
		UINT32 bmBitsAddr;
		SINT32 bmWidthPlanes;
		UINT32 bmlpPDeviceAddr;
		UINT16 bmSegmentIndex;
		UINT16 bmScanSegment;
		UINT16 bmFillBytes;
		UINT32 reserved;
	} NPDISP_PBITMAP;

	typedef struct {
		SINT16 bmType;
		SINT16 bmWidth;
		SINT16 bmHeight;
		SINT16 bmWidthBytes;
		UINT8 bmPlanes;
		UINT8 bmBitsPixel;
		UINT32 bmBitsAddr;
		SINT32 bmWidthPlanes;
		UINT32 bmlpPDeviceAddr;
		UINT16 bmSegmentIndex;
		UINT16 bmScanSegment;
		UINT16 bmFillBytes;    
		UINT16 reserved1;
		UINT16 reserved2;
		UINT32 ddbmpKey; // np2側のキー 
	} NPDISP_PBITMAP_EXT;

	typedef struct {
		UINT16 deType;
		UINT16 deWidth;
		UINT16 deHeight;
		UINT16 deWidthBytes;
		UINT8 dePlanes;
		UINT8 deBitsPixel;
		UINT32 deReserved1;
		SINT32 deDeltaScan;
		UINT32 delpPDeviceAddr;
		UINT32 deBitsOffset;
		UINT16 deBitsSelector;
		UINT16 deFlags;
		UINT16 deVersion;
		UINT32 deBitmapInfoAddr;
		UINT32 deBeginAccessFuncAddr;
		UINT32 deEndAccessFuncAddr;
		UINT32 deDriverReserved;
	} NPDISP_DIBENGINE;

	typedef struct {
		union {
			NPDISP_PBITMAP bmp;
			NPDISP_DIBENGINE dibe;
		};
	} NPDISP_PDEVICE;

	typedef struct {
		NPDISP_LPEN lpen; // NPDISP_PENの先頭はLPENとする
		int key; // np2側のキー 
	} NPDISP_PEN;
	typedef struct {
		NPDISP_LBRUSH lbrush; // NPDISP_BRUSHの先頭はLBRUSHとする
		int key; // np2側のキー 
	} NPDISP_BRUSH;
	typedef struct {
		NPDISP_LFONT lfont; // NPDISP_FONTの先頭はNPDISP_LFONTとする
		int key; // np2側のキー 
	} NPDISP_FONT;

	typedef struct {
		UINT16 Rop2;
		UINT16 bkMode;
		UINT32 bkColor;    
		UINT32 TextColor;  
		UINT16 TBreakExtra;
		UINT16 BreakExtra; 
		UINT16 BreakErr;   
		UINT16 BreakRem;   
		UINT16 BreakCount; 
		UINT16 CharExtra;  
		UINT32 LbkColor;
		UINT32 LTextColor;
	} NPDISP_DRAWMODE;

	typedef struct {
		UINT16 csHotX;
		UINT16 csHotY;
		UINT16 csWidth;
		UINT16 csHeight;
		UINT16 csWidthBytes;
		UINT16 csColor;
	} NPDISP_CURSORSHAPE;

	typedef struct {
		UINT16 diHdrSize;
		UINT16 diInfoFlags;
		UINT32 diDevNodeHandle;
		UINT8 diDriverName[16];
		UINT16 diXRes;
		UINT16 diYRes;
		UINT16 diDPI;
		UINT8 diPlanes;
		UINT8 diBpp;
		UINT16 diRefreshRateMax;
		UINT16 diRefreshRateMin;
		UINT16 diLowHorz;
		UINT16 diHighHorz;
		UINT16 diLowVert;
		UINT16 diHighVert;
		UINT32 diMonitorDevNodeHandle;
		UINT8 diHorzSyncPolarity;
		UINT8 diVertSyncPolarity;
	} NPDISP_DISPLAYINFO;

	typedef struct {
		UINT16 dvmSize;
		UINT16 dvmBpp;
		SINT16 dvmXRes;
		SINT16 dvmYRes;
	} NPDISP_DISPVALMODE;

	typedef struct
	{
		long version;
		long driverVersion;
		char dllName[262];
	} NPDISP_OPENGL_INFO;


	// ***** DirectDraw対応

	typedef struct {
		UINT32 dwCommand;
		UINT32 dwParam1;
		UINT32 dwParam2;
		UINT32 dwVersion;
		UINT32 dwReserved;
	} NPDISP_DCICMD;

	typedef struct {
		UINT32 dwHALVersion;
		UINT32 dwReserved1;
		UINT32 dwReserved2;
	} NPDISP_DDVERSIONDATA;

	typedef struct {
		char szName[260];
		char szEntryPoint[64];
		UINT32 dwContext;
	} NPDISP_DD32BITDRIVERDATA;

	typedef struct {
		NPDISP_DCICMD  cmd;
		UINT32 dwCompression;
		UINT32 dwMask[3];
		UINT32 dwWidth;
		UINT32 dwHeight;
		UINT32 dwDCICaps;
		UINT32 dwBitCount;
		UINT32 lpSurfaceAddr;
	} NPDISP_DCICREATEINPUT;

	typedef struct {
		UINT32 dwSize;
		UINT32 dwDCICaps;
		UINT32 dwCompression;
		UINT32 dwMask[3];

		UINT32 dwWidth;
		UINT32 dwHeight;
		SINT32 lStride;

		UINT32 dwBitCount;
		UINT32 dwOffSurface;
		UINT16 wSelSurface;
		UINT16 wReserved;

		UINT32 dwReserved1;
		UINT32 dwReserved2;
		UINT32 dwReserved3;

		UINT32 BeginAccessAddr;
		UINT32 EndAccessAddr;
		UINT32 DestroySurfaceAddr;
	} NPDISP_DCISURFACEINFO;

	typedef struct {
		NPDISP_DCICMD cmd;
		RECT rSrc;
		RECT rDst;
		UINT32 EnumCallbackAddr;
		UINT32 lpContextAddr;
	} NPDISP_DCIENUMINPUT;

	typedef struct {
		NPDISP_DCISURFACEINFO  dciInfo;
		UINT32 DrawAddr;
		UINT32 SetClipListAddr;
		UINT32 SetDestinationAddr;
	} NPDISP_DCIOFFSCREEN;

	typedef struct {
		NPDISP_DCISURFACEINFO  dciInfo;
		UINT32   dwChromakeyValue;
		UINT32   dwChromakeyMask;
	} NPDISP_DCIOVERLAY;


	// Win9x FullDriverで使用するDirectDraw HAL定義。
#define NPDISP_DD_VERSION                    0x00000200UL
#define NPDISP_DD_RUNTIME_VERSION            0x00000700UL
#define NPDISP_VIDMEM_ISLINEAR               0x00000001UL
#define NPDISP_DDHAL_DRIVER_NOTHANDLED       0x00000000UL
#define NPDISP_DDHAL_DRIVER_HANDLED          0x00000001UL
#define NPDISP_DDHAL_CB32_DESTROYDRIVER       0x00000001UL
#define NPDISP_DDHAL_CB32_CREATESURFACE       0x00000002UL
#define NPDISP_DDHAL_CB32_SETCOLORKEY         0x00000004UL
#define NPDISP_DDHAL_CB32_SETMODE             0x00000008UL
#define NPDISP_DDHAL_CB32_WAITFORVERTICALBLANK 0x00000010UL
#define NPDISP_DDHAL_CB32_CANCREATESURFACE    0x00000020UL
#define NPDISP_DDHAL_CB32_CREATEPALETTE       0x00000040UL
#define NPDISP_DDHAL_CB32_GETSCANLINE         0x00000080UL
#define NPDISP_DDHAL_CB32_SETEXCLUSIVEMODE    0x00000100UL
#define NPDISP_DDHAL_CB32_FLIPTOGDISURFACE    0x00000200UL
#define NPDISP_DDHAL_SURFCB32_DESTROYSURFACE  0x00000001UL
#define NPDISP_DDHAL_SURFCB32_FLIP            0x00000002UL
#define NPDISP_DDHAL_SURFCB32_SETCLIPLIST     0x00000004UL
#define NPDISP_DDHAL_SURFCB32_LOCK            0x00000008UL
#define NPDISP_DDHAL_SURFCB32_UNLOCK          0x00000010UL
#define NPDISP_DDHAL_SURFCB32_BLT             0x00000020UL
#define NPDISP_DDHAL_SURFCB32_SETCOLORKEY     0x00000040UL
#define NPDISP_DDHAL_SURFCB32_ADDATTACHEDSURFACE 0x00000080UL
#define NPDISP_DDHAL_SURFCB32_GETBLTSTATUS    0x00000100UL
#define NPDISP_DDHAL_SURFCB32_GETFLIPSTATUS   0x00000200UL
#define NPDISP_DDHAL_SURFCB32_UPDATEOVERLAY   0x00000400UL
#define NPDISP_DDHAL_SURFCB32_SETOVERLAYPOSITION 0x00000800UL
#define NPDISP_DDHAL_SURFCB32_RESERVED4       0x00001000UL
#define NPDISP_DDHAL_SURFCB32_SETPALETTE      0x00002000UL
#define NPDISP_DDHAL_PALCB32_DESTROYPALETTE   0x00000001UL
#define NPDISP_DDHAL_PALCB32_SETENTRIES       0x00000002UL
#define NPDISP_DDHALINFO_ISPRIMARYDISPLAY     0x00000001UL
#define NPDISP_DDHALINFO_MODEXILLEGAL         0x00000002UL
#define NPDISP_DDHALINFO_GETDRIVERINFOSET     0x00000004UL

// protocol v14以前の32bit HAL DLLが使用するlegacy bridge情報。
#define NPDISP_DDBRIDGE_V1_REQUEST_MAGIC       0x4444504eUL
#define NPDISP_DDBRIDGE_V1_ACK_MAGIC           0x4b4f4444UL
#define NPDISP_DDBRIDGE_ABI_V1                 0x00010000UL
#define NPDISP_DDBRIDGE_FEATURE_GETDRIVERINFO  0x00000001UL
#define NPDISP_DDBRIDGE_FEATURE_D3D_HAL        0x00000002UL
#define NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA 0x00000004UL
#define NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA 0x00000008UL
#define NPDISP_DDBRIDGE_V1_FEATURE_SUPPORTED   NPDISP_DDBRIDGE_FEATURE_GETDRIVERINFO
#define NPDISP_DDBRIDGE_V1_REQUEST_FEATURES    0x00000000UL

// protocol v15以降はDDHALINFOをscratchにせず専用descriptorを使用する。
#define NPDISP_DDBRIDGE_V2_MAGIC               0x3242444eUL
#define NPDISP_DDBRIDGE_ABI_V2                 0x00020000UL
#define NPDISP_DDBRIDGE_ABI_MAJOR_MASK         0xffff0000UL
#define NPDISP_DDBRIDGE_STATUS_EMPTY           0x00000000UL
#define NPDISP_DDBRIDGE_STATUS_HOST_READY      0x00000001UL
#define NPDISP_DDBRIDGE_STATUS_DRIVER_READY    0x00000002UL
#define NPDISP_DDBRIDGE_STATUS_FAILED          0xffffffffUL

// Direct3D対応時はD3D HAL用featureを要求する。
#define NPDISP_D3D_PROFILE_1                    0x00010000UL
#if defined(SUPPORT_NPDISP_D3D)
#define NPDISP_DDBRIDGE_V2_HOST_FEATURES       (NPDISP_DDBRIDGE_FEATURE_GETDRIVERINFO | NPDISP_DDBRIDGE_FEATURE_D3D_HAL | NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA | NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA)
#define NPDISP_DDBRIDGE_V2_D3D_PROFILE         NPDISP_D3D_PROFILE_1
#else
#define NPDISP_DDBRIDGE_V2_HOST_FEATURES       0x00000000UL
#define NPDISP_DDBRIDGE_V2_D3D_PROFILE         0x00000000UL
#endif

// np21/wで処理するcallbackだけを32bit HAL DLLへ要求する。
#define NPDISP_DDBRIDGE_DD_REQUEST_MASK \
	(NPDISP_DDHAL_CB32_CREATESURFACE | NPDISP_DDHAL_CB32_WAITFORVERTICALBLANK | \
	 NPDISP_DDHAL_CB32_CANCREATESURFACE | NPDISP_DDHAL_CB32_GETSCANLINE | \
	 NPDISP_DDHAL_CB32_FLIPTOGDISURFACE)
#define NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK \
	(NPDISP_DDHAL_SURFCB32_DESTROYSURFACE | NPDISP_DDHAL_SURFCB32_FLIP | \
	 NPDISP_DDHAL_SURFCB32_SETCLIPLIST | NPDISP_DDHAL_SURFCB32_LOCK | \
	 NPDISP_DDHAL_SURFCB32_UNLOCK | NPDISP_DDHAL_SURFCB32_BLT | \
	 NPDISP_DDHAL_SURFCB32_SETCOLORKEY | NPDISP_DDHAL_SURFCB32_ADDATTACHEDSURFACE | \
	 NPDISP_DDHAL_SURFCB32_GETBLTSTATUS | NPDISP_DDHAL_SURFCB32_GETFLIPSTATUS | \
	 NPDISP_DDHAL_SURFCB32_UPDATEOVERLAY | NPDISP_DDHAL_SURFCB32_SETOVERLAYPOSITION)
#define NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK   0x00000000UL
#define NPDISP_DDCAPS_ALPHA                  0x00800000UL
#define NPDISP_DDCAPS_3D                      0x00000001UL
#define NPDISP_DDCAPS_BLT                    0x00000040UL
#define NPDISP_DDCAPS_BLTSTRETCH             0x00000200UL
#define NPDISP_DDCAPS_GDI                    0x00000400UL
#define NPDISP_DDCAPS_OVERLAY                0x00000800UL
#define NPDISP_DDCAPS_OVERLAYCANTCLIP        0x00001000UL
#define NPDISP_DDCAPS_OVERLAYFOURCC          0x00002000UL
#define NPDISP_DDCAPS_OVERLAYSTRETCH         0x00004000UL
#define NPDISP_DDCAPS_READSCANLINE           0x00020000UL
#define NPDISP_DDCAPS_COLORKEY               0x00400000UL
#define NPDISP_DDCAPS_BLTCOLORFILL           0x04000000UL
#define NPDISP_DDCAPS_CANCLIP                0x20000000UL
#define NPDISP_DDCAPS_CANCLIPSTRETCHED       0x40000000UL
#define NPDISP_DDCAPS_CANBLTSYSMEM            0x80000000UL
#define NPDISP_DDCKEYCAPS_DESTBLT            0x00000001UL
#define NPDISP_DDCKEYCAPS_DESTBLTCLRSPACE    0x00000002UL
#define NPDISP_DDCKEYCAPS_SRCBLT             0x00000200UL
#define NPDISP_DDCKEYCAPS_DESTOVERLAY        0x00000010UL
#define NPDISP_DDCKEYCAPS_DESTOVERLAYCLRSPACE 0x00000020UL
#define NPDISP_DDCKEYCAPS_SRCOVERLAY          0x00002000UL
#define NPDISP_DDCKEYCAPS_SRCOVERLAYCLRSPACE  0x00004000UL
#define NPDISP_DDCKEYCAPS_SRCBLTCLRSPACE     0x00000400UL
#define NPDISP_DDFXCAPS_BLTALPHA              0x00000001UL
#define NPDISP_DDFXCAPS_BLTMIRRORLEFTRIGHT    0x00000040UL
#define NPDISP_DDFXCAPS_BLTMIRRORUPDOWN        0x00000080UL
#define NPDISP_DDFXCAPS_BLTROTATION90          0x00000200UL
#define NPDISP_DDFXCAPS_BLTSHRINKX            0x00000400UL
#define NPDISP_DDFXCAPS_BLTSHRINKXN           0x00000800UL
#define NPDISP_DDFXCAPS_BLTSHRINKY            0x00001000UL
#define NPDISP_DDFXCAPS_BLTSHRINKYN           0x00002000UL
#define NPDISP_DDFXCAPS_BLTSTRETCHX           0x00004000UL
#define NPDISP_DDFXCAPS_BLTSTRETCHXN          0x00008000UL
#define NPDISP_DDFXCAPS_BLTSTRETCHY           0x00010000UL
#define NPDISP_DDFXCAPS_BLTSTRETCHYN          0x00020000UL
#define NPDISP_DDFXCAPS_OVERLAYSHRINKX        0x00080000UL
#define NPDISP_DDFXCAPS_OVERLAYSHRINKXN       0x00100000UL
#define NPDISP_DDFXCAPS_OVERLAYSHRINKY        0x00200000UL
#define NPDISP_DDFXCAPS_OVERLAYSHRINKYN       0x00400000UL
#define NPDISP_DDFXCAPS_OVERLAYSTRETCHX       0x00800000UL
#define NPDISP_DDFXCAPS_OVERLAYSTRETCHXN      0x01000000UL
#define NPDISP_DDFXCAPS_OVERLAYSTRETCHY       0x02000000UL
#define NPDISP_DDFXCAPS_OVERLAYSTRETCHYN      0x04000000UL
#define NPDISP_DDFXCAPS_OVERLAYMIRRORLEFTRIGHT 0x08000000UL
#define NPDISP_DDFXCAPS_OVERLAYMIRRORUPDOWN    0x10000000UL
#define NPDISP_DDSCAPS_ALPHA                  0x00000002UL
#define NPDISP_DDSCAPS_3DDEVICE               0x00002000UL
#define NPDISP_DDSCAPS_TEXTURE                0x00001000UL
#define NPDISP_DDSCAPS_ZBUFFER                0x00020000UL
#define NPDISP_DDSCAPS_BACKBUFFER             0x00000004UL
#define NPDISP_DDSCAPS_COMPLEX                0x00000008UL
#define NPDISP_DDSCAPS_MIPMAP                 0x00400000UL
#define NPDISP_DDSCAPS_FLIP                   0x00000010UL
#define NPDISP_DDSCAPS_FRONTBUFFER            0x00000020UL
#define NPDISP_DDSCAPS_OFFSCREENPLAIN         0x00000040UL
#define NPDISP_DDSCAPS_OVERLAY                0x00000080UL
#define NPDISP_DDSCAPS_PRIMARYSURFACE        0x00000200UL
#define NPDISP_DDSCAPS_SYSTEMMEMORY          0x00000800UL
#define NPDISP_DDSCAPS_VIDEOMEMORY           0x00004000UL
#define NPDISP_DDSCAPS_VISIBLE               0x00008000UL
#define NPDISP_DDSCAPS2_CUBEMAP              0x00000200UL
#define NPDISP_DDSCAPS2_CUBEMAP_POSITIVEX    0x00000400UL
#define NPDISP_DDSCAPS2_CUBEMAP_NEGATIVEX    0x00000800UL
#define NPDISP_DDSCAPS2_CUBEMAP_POSITIVEY    0x00001000UL
#define NPDISP_DDSCAPS2_CUBEMAP_NEGATIVEY    0x00002000UL
#define NPDISP_DDSCAPS2_CUBEMAP_POSITIVEZ    0x00004000UL
#define NPDISP_DDSCAPS2_CUBEMAP_NEGATIVEZ    0x00008000UL
#define NPDISP_DDSCAPS2_CUBEMAP_ALLFACES     0x0000fc00UL
#define NPDISP_DDSCAPS2_MIPMAPSUBLEVEL        0x00010000UL
#define NPDISP_DDRAWISURF_HASPIXELFORMAT     0x00002000UL
#define NPDISP_DDPF_ALPHA                    0x00000002UL
#define NPDISP_DDPF_ALPHAPIXELS              0x00000001UL
#define NPDISP_DDPF_FOURCC                   0x00000004UL
#define NPDISP_DDPF_PALETTEINDEXED8          0x00000020UL
#define NPDISP_DDPF_RGB                      0x00000040UL
#define NPDISP_DDPF_ZBUFFER                  0x00000400UL
#define NPDISP_DDPF_STENCILBUFFER             0x00004000UL
#define NPDISP_DDPF_BUMPLUMINANCE             0x00040000UL
#define NPDISP_DDPF_BUMPDUDV                  0x00080000UL
#define NPDISP_DD_FOURCC_YUY2                0x32595559UL
#define NPDISP_DD_FOURCC_SLOT_OFFSET         (NPDISP_DD_MAX_MODES * (UINT32)sizeof(NPDISP_DDHALMODEINFO))
#define NPDISP_DDMODEINFO_PALETTIZED          0x0001U
#define NPDISP_DD_MAX_MODES                   128U
#define NPDISP_DD_MAX_CLIP_RECTS              65536U
#define NPDISP_DDSD_PITCH                    0x00000008UL
#define NPDISP_DDSD_ZBUFFERBITDEPTH          0x00000040UL
#define NPDISP_DDSD_ALPHABITDEPTH            0x00000080UL
#define NPDISP_DDSD_PIXELFORMAT              0x00001000UL
#define NPDISP_DDHAL_PLEASEALLOC_BLOCKSIZE   0x00000002UL
#define NPDISP_DDCKEY_COLORSPACE             0x00000001UL
#define NPDISP_DDCKEY_DESTBLT                0x00000002UL
#define NPDISP_DDCKEY_DESTOVERLAY            0x00000004UL
#define NPDISP_DDCKEY_SRCBLT                 0x00000008UL
#define NPDISP_DDCKEY_SRCOVERLAY             0x00000010UL
#define NPDISP_DDBLT_ALPHADEST               0x00000001UL
#define NPDISP_DDBLT_ALPHADESTCONSTOVERRIDE  0x00000002UL
#define NPDISP_DDBLT_ALPHADESTNEG            0x00000004UL
#define NPDISP_DDBLT_ALPHADESTSURFACEOVERRIDE 0x00000008UL
#define NPDISP_DDBLT_ALPHASRC                0x00000020UL
#define NPDISP_DDBLT_ALPHASRCCONSTOVERRIDE   0x00000040UL
#define NPDISP_DDBLT_ALPHASRCNEG             0x00000080UL
#define NPDISP_DDBLT_ALPHASRCSURFACEOVERRIDE 0x00000100UL
#define NPDISP_DDBLT_ASYNC                   0x00000200UL
#define NPDISP_DDBLT_COLORFILL               0x00000400UL
#define NPDISP_DDBLT_DDFX                    0x00000800UL
#define NPDISP_DDBLT_KEYDEST                 0x00002000UL
#define NPDISP_DDBLT_KEYDESTOVERRIDE         0x00004000UL
#define NPDISP_DDBLT_KEYSRC                  0x00008000UL
#define NPDISP_DDBLT_KEYSRCOVERRIDE          0x00010000UL
#define NPDISP_DDBLT_ROP                     0x00020000UL
#define NPDISP_DDBLT_WAIT                    0x01000000UL
#define NPDISP_DDFXALPHACAPS_BLTALPHASURFACES    0x00000008UL
#define NPDISP_DDFXALPHACAPS_BLTALPHASURFACESNEG 0x00000010UL
#define NPDISP_DDBD_2                        0x00002000UL
#define NPDISP_DDBD_4                        0x00001000UL
#define NPDISP_DDBD_8                        0x00000800UL
#define NPDISP_DDBD_16                       0x00000400UL
#define NPDISP_DDBD_24                       0x00000200UL
#define NPDISP_DDBD_32                       0x00000100UL
#define NPDISP_DDBLTFX_MIRRORLEFTRIGHT       0x00000002UL
#define NPDISP_DDBLTFX_MIRRORUPDOWN          0x00000004UL
#define NPDISP_DDBLTFX_ROTATE180             0x00000010UL
#define NPDISP_DDBLTFX_ROTATE270             0x00000020UL
#define NPDISP_DDBLTFX_ROTATE90              0x00000040UL
#define NPDISP_DDGBS_CANBLT                   0x00000001UL
#define NPDISP_DDGBS_ISBLTDONE                0x00000002UL
#define NPDISP_DDGFS_CANFLIP                  0x00000001UL
#define NPDISP_DDGFS_ISFLIPDONE              0x00000002UL
#define NPDISP_DDWAITVB_BLOCKBEGIN            0x00000001UL
#define NPDISP_DDWAITVB_BLOCKBEGINEVENT       0x00000002UL
#define NPDISP_DDWAITVB_BLOCKEND              0x00000004UL
#define NPDISP_DDWAITVB_I_TESTVB              0x80000006UL
#define NPDISP_DDERR_CURRENTLYNOTAVAIL        ((SINT32)0x88760028UL)
#define NPDISP_DDERR_VERTICALBLANKINPROGRESS  ((SINT32)0x88760219UL)
#define NPDISP_DDERR_WASSTILLDRAWING          ((SINT32)0x8876021cUL)

#define NPDISP_DDOVER_HIDE                  0x00000200UL
#define NPDISP_DDOVER_KEYDEST               0x00000400UL
#define NPDISP_DDOVER_KEYDESTOVERRIDE       0x00000800UL
#define NPDISP_DDOVER_KEYSRC                0x00001000UL
#define NPDISP_DDOVER_KEYSRCOVERRIDE        0x00002000UL
#define NPDISP_DDOVER_SHOW                  0x00004000UL
#define NPDISP_DDOVER_DDFX                  0x00080000UL
#define NPDISP_DDOVERFX_MIRRORLEFTRIGHT      0x00000002UL
#define NPDISP_DDOVERFX_MIRRORUPDOWN         0x00000004UL

	typedef struct {
		UINT32 dwSize;
		UINT32 dwFlags;
		UINT32 dwFourCC;
		union
		{
			UINT32 dwRGBBitCount;
			UINT32 dwYUVBitCount;
			UINT32 dwZBufferBitDepth;
			UINT32 dwAlphaBitDepth;
			UINT32 dwBumpBitCount;
		};
		union
		{
			UINT32 dwRBitMask;
			UINT32 dwYBitMask;
			UINT32 dwStencilBitDepth;
			UINT32 dwBumpDuBitMask;
		};
		union
		{
			UINT32 dwGBitMask;
			UINT32 dwUBitMask;
			UINT32 dwZBitMask;
			UINT32 dwBumpDvBitMask;
		};
		union
		{
			UINT32 dwBBitMask;
			UINT32 dwVBitMask;
			UINT32 dwStencilBitMask;
			UINT32 dwBumpLuminanceBitMask;
		};
		union
		{
			UINT32 dwRGBAlphaBitMask;
			UINT32 dwYUVAlphaBitMask;
			UINT32 dwRGBZBitMask;
			UINT32 dwYUVZBitMask;
		};
	} NPDISP_DDPIXELFORMAT;


	typedef struct
	{
		UINT32 fpPrimary;
		UINT32 dwFlags;
		UINT32 dwDisplayWidth;
		UINT32 dwDisplayHeight;
		SINT32 lDisplayPitch;
		NPDISP_DDPIXELFORMAT ddpfDisplay;
		UINT32 dwOffscreenAlign;
		UINT32 dwOverlayAlign;
		UINT32 dwTextureAlign;
		UINT32 dwZBufferAlign;
		UINT32 dwAlphaAlign;
		UINT32 dwNumHeaps;
		UINT32 pvmList;
	} NPDISP_VIDMEMINFO;

	typedef struct {
		UINT32 dwCaps;
	} NPDISP_DDSCAPS;

	typedef struct {
		UINT32 dwColorSpaceLowValue;
		UINT32 dwColorSpaceHighValue;
	} NPDISP_DDCOLORKEY;

	typedef struct {
		UINT32 dwFlags;
		UINT32 fpStart;
		UINT32 fpEnd;
		NPDISP_DDSCAPS ddsCaps;
		NPDISP_DDSCAPS ddsCapsAlt;
		UINT32 lpHeap;
	} NPDISP_VIDMEM;

#define NPDISP_DD_ROP_SPACE	(256 / 32)

	typedef struct
	{
		UINT32 dwWidth;
		UINT32 dwHeight;
		SINT32 lPitch;
		UINT32 dwBPP;
		UINT16 wFlags;
		UINT16 wRefreshRate;
		UINT32 dwRBitMask;
		UINT32 dwGBitMask;
		UINT32 dwBBitMask;
		UINT32 dwAlphaBitMask;
	} NPDISP_DDHALMODEINFO;


	typedef struct
	{
		UINT32 dwSize;
		UINT32 dwCaps;
		UINT32 dwCaps2;
		UINT32 dwCKeyCaps;
		UINT32 dwFXCaps;
		UINT32 dwFXAlphaCaps;
		UINT32 dwPalCaps;
		UINT32 dwSVCaps;
		UINT32 dwAlphaBltConstBitDepths;
		UINT32 dwAlphaBltPixelBitDepths;
		UINT32 dwAlphaBltSurfaceBitDepths;
		UINT32 dwAlphaOverlayConstBitDepths;
		UINT32 dwAlphaOverlayPixelBitDepths;
		UINT32 dwAlphaOverlaySurfaceBitDepths;
		UINT32 dwZBufferBitDepths;
		UINT32 dwVidMemTotal;
		UINT32 dwVidMemFree;
		UINT32 dwMaxVisibleOverlays;
		UINT32 dwCurrVisibleOverlays;
		UINT32 dwNumFourCCCodes;
		UINT32 dwAlignBoundarySrc;
		UINT32 dwAlignSizeSrc;
		UINT32 dwAlignBoundaryDest;
		UINT32 dwAlignSizeDest;
		UINT32 dwAlignStrideAlign;
		UINT32 dwRops[NPDISP_DD_ROP_SPACE];
		NPDISP_DDSCAPS ddsCaps;
		UINT32 dwMinOverlayStretch;
		UINT32 dwMaxOverlayStretch;
		UINT32 dwMinLiveVideoStretch;
		UINT32 dwMaxLiveVideoStretch;
		UINT32 dwMinHwCodecStretch;
		UINT32 dwMaxHwCodecStretch;
		UINT32 dwReserved1;
		UINT32 dwReserved2;
		UINT32 dwReserved3;
		UINT32 dwSVBCaps;
		UINT32 dwSVBCKeyCaps;
		UINT32 dwSVBFXCaps;
		UINT32 dwSVBRops[NPDISP_DD_ROP_SPACE];
		UINT32 dwVSBCaps;
		UINT32 dwVSBCKeyCaps;
		UINT32 dwVSBFXCaps;
		UINT32 dwVSBRops[NPDISP_DD_ROP_SPACE];
		UINT32 dwSSBCaps;
		UINT32 dwSSBCKeyCaps;
		UINT32 dwSSBFXCaps;
		UINT32 dwSSBRops[NPDISP_DD_ROP_SPACE];
		UINT32 dwMaxVideoPorts;
		UINT32 dwCurrVideoPorts;
		UINT32 dwSVBCaps2;
	} NPDISP_DDCORECAPS;

	typedef struct
	{
		UINT32 dwSize;
		UINT32 dwMagic;
		UINT32 dwAbiVersion;
		UINT32 dwStatus;
		UINT32 dwHostFeaturesOffered;
		UINT32 dwDriverFeaturesSupported;
		UINT32 dwNegotiatedFeatures;
		UINT32 dwD3DProfileId;
		UINT32 lpDDHalInfo;
		UINT32 lpDDCallbacks;
		UINT32 lpDDSurfaceCallbacks;
		UINT32 lpDDPaletteCallbacks;
		UINT32 dwDDRequestMask;
		UINT32 dwSurfaceRequestMask;
		UINT32 dwPaletteRequestMask;
		UINT32 lpD3DGlobalDriverData;
		UINT32 lpD3DHALCallbacks;
		UINT32 lpD3DThunkTable;
		UINT32 dwD3DThunkTableSize;
		UINT32 dwD3DThunkTableVersion;
	} NPDISP_DDBRIDGEINFO32;

	typedef char NPDISP_DDBRIDGEINFO32_SIZE_CHECK[(sizeof(NPDISP_DDBRIDGEINFO32) == 80) ? 1 : -1];
	typedef char NPDISP_DDBRIDGEINFO32_HAL_OFFSET_CHECK[(offsetof(NPDISP_DDBRIDGEINFO32, lpDDHalInfo) == 32) ? 1 : -1];
	typedef char NPDISP_DDBRIDGEINFO32_MASK_OFFSET_CHECK[(offsetof(NPDISP_DDBRIDGEINFO32, dwDDRequestMask) == 48) ? 1 : -1];
	typedef char NPDISP_DDBRIDGEINFO32_D3D_OFFSET_CHECK[(offsetof(NPDISP_DDBRIDGEINFO32, lpD3DGlobalDriverData) == 60) ? 1 : -1];

#define NPDISP_D3D_GLOBALDRIVERDATA_SIZE	192U
#define NPDISP_D3D_TEXTURE_FORMAT_SIZE		108U
#define NPDISP_D3D_TEXTURE_FORMAT_CAPACITY	16U
#define NPDISP_D3D_THUNK_TABLE_VERSION		0x00010000UL

	typedef struct
	{
		UINT32 dwSize;
		UINT32 dwVersion;
		UINT32 ContextCreate;
		UINT32 ContextDestroy;
		UINT32 ContextDestroyAll;
		UINT32 SceneCapture;
		UINT32 RenderState;
		UINT32 RenderPrimitive;
		UINT32 GetState;
		UINT32 TextureCreate;
		UINT32 TextureDestroy;
		UINT32 TextureSwap;
		UINT32 TextureGetSurf;
		UINT32 SetRenderTarget;
		UINT32 Clear;
		UINT32 DrawOnePrimitive;
		UINT32 DrawOneIndexedPrimitive;
		UINT32 DrawPrimitives;
		UINT32 ValidateTextureStageState;
		UINT32 DrawPrimitives2;
		UINT32 Clear2;
		UINT32 CreateSurfaceEx;
		UINT32 GetDriverState;
		UINT32 DestroyDDLocal;
		UINT32 AlphaBlt;
	} NPDISP_D3D_THUNK_TABLE32;
	typedef char NPDISP_D3D_THUNK_TABLE32_SIZE_CHECK[(sizeof(NPDISP_D3D_THUNK_TABLE32) == 100) ? 1 : -1];

	typedef struct
	{
		UINT32 dwSize;
		UINT32 lpDDCallbacksAddr;
		UINT32 lpDDSurfaceCallbacksAddr;
		UINT32 lpDDPaletteCallbacksAddr;
		NPDISP_VIDMEMINFO vmiData;
		NPDISP_DDCORECAPS ddCaps;
		UINT32 dwMonitorFrequency;
		UINT32 GetDriverInfoAddr;
		UINT32 dwModeIndex;
		UINT32 lpdwFourCC;
		UINT32 dwNumModes;
		UINT32 lpModeInfo;
		UINT32 dwFlags;
		UINT32 lpPDevice;
		UINT32 hInstance;
		UINT32 lpD3DGlobalDriverData;
		UINT32 lpD3DHALCallbacks;
		UINT32 lpDDExeBufCallbacksAddr;
	} NPDISP_DDHALINFO;

	// 16bit DirectDraw callback table。関数ポインタはUINT32の16:16 far pointerとして保持する。
	typedef struct {
		UINT32 dwSize;
		UINT32 dwFlags;
		UINT32 DestroyDriverAddr;
		UINT32 CreateSurfaceAddr;
		UINT32 SetColorKeyAddr;
		UINT32 SetModeAddr;
		UINT32 WaitForVerticalBlankAddr;
		UINT32 CanCreateSurfaceAddr;
		UINT32 CreatePaletteAddr;
		UINT32 GetScanLineAddr;
		UINT32 SetExclusiveModeAddr;
		UINT32 FlipToGDISurfaceAddr;
	} NPDISP_DDHAL_DDCALLBACKS;

	typedef struct {
		UINT32 dwSize;
		UINT32 dwFlags;
		UINT32 DestroyPaletteAddr;
		UINT32 SetEntriesAddr;
	} NPDISP_DDHAL_DDPALETTECALLBACKS;

	typedef struct {
		UINT32 dwSize;
		UINT32 dwFlags;
		UINT32 DestroySurfaceAddr;
		UINT32 FlipAddr;
		UINT32 SetClipListAddr;
		UINT32 LockAddr;
		UINT32 UnlockAddr;
		UINT32 BltAddr;
		UINT32 SetColorKeyAddr;
		UINT32 AddAttachedSurfaceAddr;
		UINT32 GetBltStatusAddr;
		UINT32 GetFlipStatusAddr;
		UINT32 UpdateOverlayAddr;
		UINT32 SetOverlayPositionAddr;
		UINT32 reserved4Addr;
		UINT32 SetPaletteAddr;
	} NPDISP_DDHAL_DDSURFACECALLBACKS;

	typedef struct {
		SINT32 left;
		SINT32 top;
		SINT32 right;
		SINT32 bottom;
	} NPDISP_DDRECTL;

	// DirectDraw 1のDDSURFACEDESC。ddsCapsまでをHAL判定に使用する。
	typedef struct {
		UINT32 dwSize;
		UINT32 dwFlags;
		UINT32 dwHeight;
		UINT32 dwWidth;
		SINT32 lPitch;
		UINT32 dwBackBufferCount;
		union {
			UINT32 dwMipMapCount;
			UINT32 dwZBufferBitDepth;
		};
		UINT32 dwAlphaBitDepth;
		UINT32 dwReserved;
		UINT32 lpSurface;
		UINT32 ddckCKDestOverlay[2];
		UINT32 ddckCKDestBlt[2];
		UINT32 ddckCKSrcOverlay[2];
		UINT32 ddckCKSrcBlt[2];
		NPDISP_DDPIXELFORMAT ddpfPixelFormat;
		NPDISP_DDSCAPS ddsCaps;
	} NPDISP_DDSURFACEDESC;

	// HALで参照するWin9x DDRAWI surface objectの先頭部分。
	typedef struct {
		UINT32 dwRefCnt;
		UINT32 dwGlobalFlags;
		UINT32 dwBlockSizeY;
		UINT32 dwBlockSizeX;
		UINT32 lpDD;
		UINT32 fpVidMem;
		SINT32 lPitch;
		UINT16 wHeight;
		UINT16 wWidth;
	} NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD;

	// System-memory Bltではoptional pixel formatを確認してdisplay format以外をHELへ戻す。
	typedef struct {
		NPDISP_DDRAWI_DDRAWSURFACE_GBL_HEAD head;
		UINT32 dwUsageCount;
		UINT32 dwReserved1;
		NPDISP_DDPIXELFORMAT ddpfSurface;
	} NPDISP_DDRAWI_DDRAWSURFACE_GBL_BLT;

	// IDirectDrawSurface interface objectの先頭部分。lpLclからHAL用surface objectを取得する。
	typedef struct {
		UINT32 lpVtbl;
		UINT32 lpLcl;
	} NPDISP_DDRAWI_DDRAWSURFACE_INT_HEAD;

	// DDRAWI surface間のattachment list。Alpha surface検索に使用する。
	typedef struct {
		UINT32 dwFlags;
		UINT32 lpLink;
		UINT32 lpAttached;
		UINT32 lpIAttached;
	} NPDISP_DDATTACHLIST;

	typedef struct {
		UINT32 lpSurfMore;
		UINT32 lpGbl;
		UINT32 hDDSurface;
		UINT32 lpAttachList;
		UINT32 lpAttachListFrom;
		UINT32 dwLocalRefCnt;
		UINT32 dwProcessId;
		UINT32 dwFlags;
		NPDISP_DDSCAPS ddsCaps;
	} NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD;

#if defined(SUPPORT_NPDISP_D3D)
	// ContextCreateのdwhContextはdriverが返すcontext handle。
	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDS;
		UINT32 lpDDSZ;
		UINT32 dwPID;
		UINT32 dwhContext;
		SINT32 ddrval;
	} NPDISP_D3DHAL_CONTEXTCREATEDATA32;

	typedef struct {
		UINT32 dwhContext;
		SINT32 ddrval;
	} NPDISP_D3DHAL_CONTEXTDESTROYDATA32;

	typedef struct {
		UINT32 dwPID;
		SINT32 ddrval;
	} NPDISP_D3DHAL_CONTEXTDESTROYALLDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlag;
		SINT32 ddrval;
	} NPDISP_D3DHAL_SCENECAPTUREDATA32;
	typedef char NPDISP_D3DHAL_SCENECAPTUREDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_SCENECAPTUREDATA32) == 12) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwOffset;
		UINT32 dwCount;
		UINT32 lpExeBuf;
		SINT32 ddrval;
	} NPDISP_D3DHAL_RENDERSTATEDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwOffset;
		UINT32 dwStatus;
		UINT32 lpExeBuf;
		UINT32 dwTLOffset;
		UINT32 lpTLBuf;
		UINT32 dwInstruction;
		SINT32 ddrval;
	} NPDISP_D3DHAL_RENDERPRIMITIVEDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwWhich;
		UINT32 dwStateType;
		UINT32 dwStateValue;
		SINT32 ddrval;
	} NPDISP_D3DHAL_GETSTATEDATA32;
	typedef char NPDISP_D3DHAL_GETSTATEDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_GETSTATEDATA32) == 20) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 lpDDS;
		UINT32 dwHandle;
		SINT32 ddrval;
	} NPDISP_D3DHAL_TEXTURECREATEDATA32;
	typedef char NPDISP_D3DHAL_TEXTURECREATEDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_TEXTURECREATEDATA32) == 16) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwHandle;
		SINT32 ddrval;
	} NPDISP_D3DHAL_TEXTUREDESTROYDATA32;
	typedef char NPDISP_D3DHAL_TEXTUREDESTROYDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_TEXTUREDESTROYDATA32) == 12) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwHandle1;
		UINT32 dwHandle2;
		SINT32 ddrval;
	} NPDISP_D3DHAL_TEXTURESWAPDATA32;
	typedef char NPDISP_D3DHAL_TEXTURESWAPDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_TEXTURESWAPDATA32) == 16) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 lpDDS;
		UINT32 dwHandle;
		SINT32 ddrval;
	} NPDISP_D3DHAL_TEXTUREGETSURFDATA32;
	typedef char NPDISP_D3DHAL_TEXTUREGETSURFDATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_TEXTUREGETSURFDATA32) == 16) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 lpDDS;
		UINT32 lpDDSZ;
		SINT32 ddrval;
	} NPDISP_D3DHAL_SETRENDERTARGETDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlags;
		UINT32 dwFillColor;
		UINT32 dwFillDepth;
		UINT32 lpRects;
		UINT32 dwNumRects;
		SINT32 ddrval;
	} NPDISP_D3DHAL_CLEARDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlags;
		UINT32 dwFillColor;
		float dvFillDepth;
		UINT32 dwFillStencil;
		UINT32 lpRects;
		UINT32 dwNumRects;
		SINT32 ddrval;
	} NPDISP_D3DHAL_CLEAR2DATA32;
	typedef char NPDISP_D3DHAL_CLEAR2DATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_CLEAR2DATA32) == 32) ? 1 : -1];

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlags;
		UINT32 primitiveType;
		UINT32 vertexType;
		UINT32 lpVertices;
		UINT32 dwNumVertices;
		UINT32 dwReserved;
		SINT32 ddrval;
	} NPDISP_D3DHAL_DRAWONEPRIMITIVEDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlags;
		UINT32 primitiveType;
		UINT32 vertexType;
		UINT32 lpVertices;
		UINT32 dwNumVertices;
		UINT32 lpIndices;
		UINT32 dwNumIndices;
		SINT32 ddrval;
	} NPDISP_D3DHAL_DRAWONEINDEXEDPRIMITIVEDATA32;

	typedef struct {
		UINT16 wNumStateChanges;
		UINT16 wPrimitiveType;
		UINT16 wVertexType;
		UINT16 wNumVertices;
	} NPDISP_D3DHAL_DRAWPRIMCOUNTS32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlags;
		UINT32 lpData;
		UINT32 dwFVFControl;
		SINT32 ddrval;
	} NPDISP_D3DHAL_DRAWPRIMITIVESDATA32;

	typedef struct {
		UINT32 dwhContext;
		UINT32 dwFlags;
		UINT32 dwVertexType;
		UINT32 lpDDCommands;
		UINT32 dwCommandOffset;
		UINT32 dwCommandLength;
		UINT32 lpDDVertexOrVertices;
		UINT32 dwVertexOffset;
		UINT32 dwVertexLength;
		UINT32 dwReqVertexBufSize;
		UINT32 dwReqCommandBufSize;
		UINT32 lpdwRStates;
		UINT32 dwVertexSizeOrDdrval;
		UINT32 dwErrorOffset;
	} NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32;

	typedef struct {
		UINT8 bCommand;
		UINT8 bReserved;
		UINT16 wCount;
	} NPDISP_D3DHAL_DP2COMMAND32;

	typedef struct {
		UINT32 dwEdgeFlags;
	} NPDISP_D3DHAL_DP2TRIANGLEFAN_IMM32;

	typedef struct {
		UINT32 renderState;
		UINT32 value;
	} NPDISP_D3DHAL_DP2RENDERSTATE32;

	typedef struct {
		UINT16 wStage;
		UINT16 TSState;
		UINT32 dwValue;
	} NPDISP_D3DHAL_DP2TEXTURESTAGESTATE32;

	typedef struct {
		UINT32 dwX;
		UINT32 dwY;
		UINT32 dwWidth;
		UINT32 dwHeight;
	} NPDISP_D3DHAL_DP2VIEWPORTINFO32;

	typedef struct {
		float dvWNear;
		float dvWFar;
	} NPDISP_D3DHAL_DP2WINFO32;

	typedef struct {
		UINT32 dwPaletteHandle;
		UINT32 dwPaletteFlags;
		UINT32 dwSurfaceHandle;
	} NPDISP_D3DHAL_DP2SETPALETTE32;

	typedef struct {
		UINT32 dwPaletteHandle;
		UINT16 wStartIndex;
		UINT16 wNumEntries;
	} NPDISP_D3DHAL_DP2UPDATEPALETTE32;

	typedef struct {
		float dvMinZ;
		float dvMaxZ;
	} NPDISP_D3DHAL_DP2ZRANGE32;

	typedef struct {
		UINT16 wCount;
		UINT16 wVStart;
	} NPDISP_D3DHAL_DP2POINTS32;

	typedef struct {
		UINT16 wVStart;
	} NPDISP_D3DHAL_DP2STARTVERTEX32;

	typedef struct {
		UINT16 wV1;
		UINT16 wV2;
	} NPDISP_D3DHAL_DP2INDEXEDLINELIST32;

	typedef struct {
		UINT16 wV1;
		UINT16 wV2;
		UINT16 wV3;
		UINT16 wFlags;
	} NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST32;

	typedef struct {
		UINT16 wV1;
		UINT16 wV2;
		UINT16 wV3;
	} NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST2_32;

	typedef struct {
		UINT32 hRenderTarget;
		UINT32 hZBuffer;
	} NPDISP_D3DHAL_DP2SETRENDERTARGET32;

	typedef struct {
		UINT32 dwDDDestSurface;
		UINT32 dwDDSrcSurface;
		SINT32 x;
		SINT32 y;
		NPDISP_DDRECTL rSrc;
		UINT32 dwFlags;
	} NPDISP_D3DHAL_DP2TEXBLT32;

	typedef struct {
		UINT32 dwFlags;
		UINT32 dwFillColor;
		float dvFillDepth;
		UINT32 dwFillStencil;
	} NPDISP_D3DHAL_DP2CLEAR32;

	typedef char NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DRAWPRIMITIVES2DATA32) == 56) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2COMMAND32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2COMMAND32) == 4) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2TRIANGLEFAN_IMM32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2TRIANGLEFAN_IMM32) == 4) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2RENDERSTATE32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2RENDERSTATE32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2TEXTURESTAGESTATE32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2TEXTURESTAGESTATE32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2VIEWPORTINFO32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2VIEWPORTINFO32) == 16) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2WINFO32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2WINFO32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2SETPALETTE32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2SETPALETTE32) == 12) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2UPDATEPALETTE32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2UPDATEPALETTE32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2ZRANGE32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2ZRANGE32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2POINTS32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2POINTS32) == 4) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2INDEXEDLINELIST32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2INDEXEDLINELIST32) == 4) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST2_32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2INDEXEDTRIANGLELIST2_32) == 6) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2SETRENDERTARGET32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2SETRENDERTARGET32) == 8) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2TEXBLT32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2TEXBLT32) == 36) ? 1 : -1];
	typedef char NPDISP_D3DHAL_DP2CLEAR32_SIZE_CHECK[(sizeof(NPDISP_D3DHAL_DP2CLEAR32) == 16) ? 1 : -1];

	typedef struct {
		UINT32 type;
		UINT32 value;
	} NPDISP_D3DSTATE32;

	typedef struct {
		float sx;
		float sy;
		float sz;
		float rhw;
		UINT32 color;
		UINT32 specular;
		float tu;
		float tv;
		float tu2;
		float tv2;
		float tw;
		float tw2;
	} NPDISP_D3DTLVERTEX32;

	typedef struct {
		UINT16 v1;
		UINT16 v2;
		UINT16 v3;
		UINT16 wFlags;
	} NPDISP_D3DTRIANGLE32;

#define NPDISP_D3DPT_POINTLIST			1UL
#define NPDISP_D3DPT_LINELIST			2UL
#define NPDISP_D3DPT_LINESTRIP			3UL
#define NPDISP_D3DPT_TRIANGLELIST		4UL
#define NPDISP_D3DPT_TRIANGLESTRIP		5UL
#define NPDISP_D3DPT_TRIANGLEFAN		6UL
#define NPDISP_D3DVT_TLVERTEX			3UL
#define NPDISP_D3DFVF_RESERVED0			0x00000001UL
#define NPDISP_D3DFVF_POSITION_MASK		0x0000000eUL
#define NPDISP_D3DFVF_XYZRHW			0x00000004UL
#define NPDISP_D3DFVF_NORMAL			0x00000010UL
#define NPDISP_D3DFVF_RESERVED1			0x00000020UL
#define NPDISP_D3DFVF_DIFFUSE			0x00000040UL
#define NPDISP_D3DFVF_SPECULAR			0x00000080UL
#define NPDISP_D3DFVF_TEXCOUNT_MASK		0x00000f00UL
#define NPDISP_D3DFVF_RESERVED2			0x0000f000UL
#define NPDISP_D3DHALDP2_USERMEMVERTICES	0x00000001UL
#define NPDISP_D3DHALDP2_EXECUTEBUFFER		0x00000002UL
#define NPDISP_D3DHALDP2_SWAPVERTEXBUFFER	0x00000004UL
#define NPDISP_D3DHALDP2_SWAPCOMMANDBUFFER	0x00000008UL
#define NPDISP_D3DHALDP2_REQVERTEXBUFSIZE	0x00000010UL
#define NPDISP_D3DHALDP2_REQCOMMANDBUFSIZE	0x00000020UL
#define NPDISP_D3DHALDP2_VIDMEMVERTEXBUF	0x00000040UL
#define NPDISP_D3DHALDP2_VIDMEMCOMMANDBUF	0x00000080UL
#define NPDISP_D3DHALDP2_VALID_FLAGS		0x000000ffUL
#define NPDISP_D3DDP2OP_POINTS			1U
#define NPDISP_D3DDP2OP_INDEXEDLINELIST		2U
#define NPDISP_D3DDP2OP_INDEXEDTRIANGLELIST	3U
#define NPDISP_D3DDP2OP_RENDERSTATE		8U
#define NPDISP_D3DDP2OP_LINELIST		15U
#define NPDISP_D3DDP2OP_LINESTRIP		16U
#define NPDISP_D3DDP2OP_INDEXEDLINESTRIP	17U
#define NPDISP_D3DDP2OP_INDEXEDTRIANGLESTRIP	20U
#define NPDISP_D3DDP2OP_INDEXEDTRIANGLEFAN	22U
#define NPDISP_D3DDP2OP_TRIANGLEFAN_IMM	23U
#define NPDISP_D3DDP2OP_LINELIST_IMM		24U
#define NPDISP_D3DDP2OP_TEXTURESTAGESTATE	25U
#define NPDISP_D3DDP2OP_INDEXEDTRIANGLELIST2	26U
#define NPDISP_D3DDP2OP_INDEXEDLINELIST2	27U
#define NPDISP_D3DDP2OP_VIEWPORTINFO		28U
#define NPDISP_D3DDP2OP_WINFO			29U
#define NPDISP_D3DDP2OP_SETPALETTE		30U
#define NPDISP_D3DDP2OP_UPDATEPALETTE		31U
#define NPDISP_D3DDP2OP_ZRANGE			32U
#define NPDISP_D3DDP2OP_TEXBLT			38U
#define NPDISP_D3DDP2OP_TRIANGLELIST		18U
#define NPDISP_D3DDP2OP_TRIANGLESTRIP		19U
#define NPDISP_D3DDP2OP_TRIANGLEFAN		21U
#define NPDISP_D3DDP2OP_SETRENDERTARGET	41U
#define NPDISP_D3DDP2OP_CLEAR			42U
#define NPDISP_D3DERR_COMMAND_UNPARSED		0x88760bb8UL
#define NPDISP_D3DOP_TRIANGLE			3UL
#define NPDISP_D3DOP_EXIT			11UL
#define NPDISP_D3DTRIFLAG_START			0x00000000UL
#define NPDISP_D3DTRIFLAG_ODD			0x0000001eUL
#define NPDISP_D3DTRIFLAG_EVEN			0x0000001fUL
#define NPDISP_D3DTRIFLAG_EDGEENABLEMASK	0x00000700UL
#define NPDISP_D3DCLEAR_TARGET			0x00000001UL
#define NPDISP_D3DCLEAR_ZBUFFER			0x00000002UL
#define NPDISP_D3DCLEAR_STENCIL			0x00000004UL
#define NPDISP_D3DHAL_SCENE_CAPTURE_START		0UL
#define NPDISP_D3DHAL_SCENE_CAPTURE_END		1UL
#define NPDISP_DDRAWIPAL_ALPHA			0x00002000UL
#define NPDISP_D3DHALSTATE_GET_TRANSFORM		0x00000001UL
#define NPDISP_D3DHALSTATE_GET_LIGHT			0x00000002UL
#define NPDISP_D3DHALSTATE_GET_RENDER			0x00000004UL

#define NPDISP_D3DTSS_TEXTUREMAP			0U
#define NPDISP_D3DTSS_COLOROP			1U
#define NPDISP_D3DTSS_COLORARG1			2U
#define NPDISP_D3DTSS_COLORARG2			3U
#define NPDISP_D3DTSS_ALPHAOP			4U
#define NPDISP_D3DTSS_ALPHAARG1			5U
#define NPDISP_D3DTSS_ALPHAARG2			6U
#define NPDISP_D3DTSS_BUMPENVMAT00		7U
#define NPDISP_D3DTSS_BUMPENVMAT01		8U
#define NPDISP_D3DTSS_BUMPENVMAT10		9U
#define NPDISP_D3DTSS_BUMPENVMAT11		10U
#define NPDISP_D3DTSS_TEXCOORDINDEX		11U
#define NPDISP_D3DTSS_ADDRESS			12U
#define NPDISP_D3DTSS_ADDRESSU			13U
#define NPDISP_D3DTSS_ADDRESSV			14U
#define NPDISP_D3DTSS_MAGFILTER			16U
#define NPDISP_D3DTSS_MINFILTER			17U
#define NPDISP_D3DTSS_MIPFILTER			18U
#define NPDISP_D3DTSS_BUMPENVLSCALE		22U
#define NPDISP_D3DTSS_BUMPENVLOFFSET		23U
#define NPDISP_D3DTSS_TEXTURETRANSFORMFLAGS	24U
#define NPDISP_D3DTSS_MAX_STATE			28U

#define NPDISP_D3DTOP_DISABLE			1U
#define NPDISP_D3DTOP_SELECTARG1		2U
#define NPDISP_D3DTOP_SELECTARG2		3U
#define NPDISP_D3DTOP_MODULATE			4U
#define NPDISP_D3DTOP_ADD			7U
#define NPDISP_D3DTOP_BUMPENVMAP		22U
#define NPDISP_D3DTOP_BUMPENVMAPLUMINANCE	23U
#define NPDISP_D3DTA_SELECTMASK			0x0000000fUL
#define NPDISP_D3DTA_DIFFUSE			0x00000000UL
#define NPDISP_D3DTA_CURRENT			0x00000001UL
#define NPDISP_D3DTA_TEXTURE			0x00000002UL
#define NPDISP_D3DTA_TFACTOR			0x00000003UL
#define NPDISP_D3DTA_SPECULAR			0x00000004UL
#define NPDISP_D3DTA_COMPLEMENT			0x00000010UL
#define NPDISP_D3DTA_ALPHAREPLICATE		0x00000020UL
#define NPDISP_D3DTADDRESS_WRAP			1U
#define NPDISP_D3DTADDRESS_MIRROR		2U
#define NPDISP_D3DTADDRESS_CLAMP			3U
#define NPDISP_D3DTADDRESS_BORDER		4U
#define NPDISP_D3DTFG_POINT			1U
#define NPDISP_D3DTFG_LINEAR			2U
#define NPDISP_D3DTFN_POINT			1U
#define NPDISP_D3DTFN_LINEAR			2U
#define NPDISP_D3DTFP_NONE			1U
#define NPDISP_D3DTFP_POINT			2U
#define NPDISP_D3DTFP_LINEAR			3U

#define NPDISP_D3DRENDERSTATE_TEXTUREHANDLE		1UL
#define NPDISP_D3DRENDERSTATE_ZENABLE			7UL
#define NPDISP_D3DRENDERSTATE_FILLMODE			8UL
#define NPDISP_D3DRENDERSTATE_SHADEMODE			9UL
#define NPDISP_D3DRENDERSTATE_ZWRITEENABLE		14UL
#define NPDISP_D3DRENDERSTATE_ALPHATESTENABLE		15UL
#define NPDISP_D3DRENDERSTATE_SRCBLEND			19UL
#define NPDISP_D3DRENDERSTATE_DESTBLEND			20UL
#define NPDISP_D3DRENDERSTATE_TEXTUREMAPBLEND		21UL
#define NPDISP_D3DRENDERSTATE_CULLMODE			22UL
#define NPDISP_D3DRENDERSTATE_ZFUNC			23UL
#define NPDISP_D3DRENDERSTATE_ALPHAREF			24UL
#define NPDISP_D3DRENDERSTATE_ALPHAFUNC			25UL
#define NPDISP_D3DRENDERSTATE_ALPHABLENDENABLE		27UL
#define NPDISP_D3DRENDERSTATE_FOGENABLE			28UL
#define NPDISP_D3DRENDERSTATE_SPECULARENABLE		29UL
#define NPDISP_D3DRENDERSTATE_COLORKEYENABLE		41UL
#define NPDISP_DDRAWISURF_HASCKEYSRCBLT		0x00000800UL
#define NPDISP_D3DRENDERSTATE_STENCILENABLE		52UL
#define NPDISP_D3DRENDERSTATE_STENCILFAIL		53UL
#define NPDISP_D3DRENDERSTATE_STENCILZFAIL		54UL
#define NPDISP_D3DRENDERSTATE_STENCILPASS		55UL
#define NPDISP_D3DRENDERSTATE_STENCILFUNC		56UL
#define NPDISP_D3DRENDERSTATE_STENCILREF		57UL
#define NPDISP_D3DRENDERSTATE_STENCILMASK		58UL
#define NPDISP_D3DRENDERSTATE_STENCILWRITEMASK	59UL
#define NPDISP_D3DRENDERSTATE_SCENECAPTURE		62UL
#define NPDISP_D3DRENDERSTATE_FOGCOLOR			34UL
#define NPDISP_D3DRENDERSTATE_WRAP0			128UL

#define NPDISP_D3DCMP_NEVER				1UL
#define NPDISP_D3DCMP_LESS				2UL
#define NPDISP_D3DCMP_EQUAL				3UL
#define NPDISP_D3DCMP_LESSEQUAL			4UL
#define NPDISP_D3DCMP_GREATER				5UL
#define NPDISP_D3DCMP_NOTEQUAL				6UL
#define NPDISP_D3DCMP_GREATEREQUAL			7UL
#define NPDISP_D3DCMP_ALWAYS				8UL
#define NPDISP_D3DSTENCILOP_KEEP			1UL
#define NPDISP_D3DSTENCILOP_ZERO			2UL
#define NPDISP_D3DSTENCILOP_REPLACE			3UL
#define NPDISP_D3DSTENCILOP_INCRSAT			4UL
#define NPDISP_D3DSTENCILOP_DECRSAT			5UL
#define NPDISP_D3DSTENCILOP_INVERT			6UL
#define NPDISP_D3DSTENCILOP_INCR			7UL
#define NPDISP_D3DSTENCILOP_DECR			8UL
#define NPDISP_D3DTBLEND_MODULATE			2UL

#define NPDISP_D3DBLEND_ZERO				1UL
#define NPDISP_D3DBLEND_ONE				2UL
#define NPDISP_D3DBLEND_SRCCOLOR			3UL
#define NPDISP_D3DBLEND_INVSRCCOLOR			4UL
#define NPDISP_D3DBLEND_SRCALPHA			5UL
#define NPDISP_D3DBLEND_INVSRCALPHA			6UL
#define NPDISP_D3DBLEND_DESTALPHA			7UL
#define NPDISP_D3DBLEND_INVDESTALPHA			8UL
#define NPDISP_D3DBLEND_DESTCOLOR			9UL
#define NPDISP_D3DBLEND_INVDESTCOLOR			10UL
#define NPDISP_D3DBLEND_SRCALPHASAT			11UL
#define NPDISP_D3DBLEND_BOTHSRCALPHA			12UL
#define NPDISP_D3DBLEND_BOTHINVSRCALPHA		13UL
#define NPDISP_D3DFILL_WIREFRAME		2UL
#define NPDISP_D3DFILL_SOLID			3UL
#define NPDISP_D3DSHADE_FLAT			1UL
#define NPDISP_D3DSHADE_GOURAUD			2UL
#define NPDISP_D3DCULL_NONE			1UL
#define NPDISP_D3DCULL_CW			2UL
#define NPDISP_D3DCULL_CCW			3UL

	typedef struct {
		UINT32 dwFlags;
		UINT32 lpDDLcl;
		UINT32 lpDDSLcl;
		SINT32 ddRVal;
	} NPDISP_DD_CREATESURFACEEXDATA32;

	typedef struct {
		UINT32 dwSize;
	} NPDISP_DDRAWI_DDRAWSURFACE_MORE_HEAD32;

#define NPDISP_DDRAWSURFACE_MORE_SIZE_DX7	136U
#define NPDISP_DDRAWSURFACE_CAPS2_OFS_DX7	52U
#define NPDISP_DDRAWSURFACE_HANDLE_OFS_DX7	100U

	typedef struct {
		UINT32 dwFlags;
		UINT32 pDDLcl;
		SINT32 ddRVal;
	} NPDISP_DDHAL_DESTROYDDLOCALDATA32;

#endif

	// surface単位のBlt/Overlay color keyまでを含むDDRAWI surface情報。
	typedef struct {
		NPDISP_DDRAWI_DDRAWSURFACE_LCL_HEAD head;
		UINT32 lpDDPalette;
		UINT32 lpDDClipper;
		UINT32 dwModeCreatedIn;
		UINT32 dwBackBufferCount;
		NPDISP_DDCOLORKEY ddckCKDestBlt;
		NPDISP_DDCOLORKEY ddckCKSrcBlt;
		UINT32 hDC;
		UINT32 dwReserved1;
		NPDISP_DDCOLORKEY ddckCKSrcOverlay;
		NPDISP_DDCOLORKEY ddckCKDestOverlay;
	} NPDISP_DDRAWI_DDRAWSURFACE_LCL_COLORKEYS;

	typedef struct {
		UINT32 dwSize;
		UINT32 dwAlphaEdgeBlendBitDepth;
		UINT32 dwAlphaEdgeBlend;
		UINT32 dwReserved;
		UINT32 dwAlphaDestConstBitDepth;
		UINT32 dwAlphaDestConst;
		UINT32 dwAlphaSrcConstBitDepth;
		UINT32 dwAlphaSrcConst;
		NPDISP_DDCOLORKEY dckDestColorkey;
		NPDISP_DDCOLORKEY dckSrcColorkey;
		UINT32 dwDDFX;
		UINT32 dwFlags;
	} NPDISP_DDOVERLAYFX;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDDestSurface;
		NPDISP_DDRECTL rDest;
		UINT32 lpDDSrcSurface;
		NPDISP_DDRECTL rSrc;
		UINT32 dwFlags;
		NPDISP_DDOVERLAYFX overlayFX;
		SINT32 ddRVal;
		UINT32 UpdateOverlayAddr;
	} NPDISP_DDHAL_UPDATEOVERLAYDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSrcSurface;
		UINT32 lpDDDestSurface;
		SINT32 lXPos;
		SINT32 lYPos;
		SINT32 ddRVal;
		UINT32 SetOverlayPositionAddr;
	} NPDISP_DDHAL_SETOVERLAYPOSITIONDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurfaceDesc;
		UINT32 lplpSList;
		UINT32 dwSCnt;
		SINT32 ddRVal;
		UINT32 CreateSurfaceAddr;
	} NPDISP_DDHAL_CREATESURFACEDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurfaceDesc;
		UINT32 bIsDifferentPixelFormat;
		SINT32 ddRVal;
		UINT32 CanCreateSurfaceAddr;
	} NPDISP_DDHAL_CANCREATESURFACEDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 dwFlags;
		UINT32 bIsInVB;
		UINT32 hEvent;
		SINT32 ddRVal;
		UINT32 WaitForVerticalBlankAddr;
	} NPDISP_DDHAL_WAITFORVERTICALBLANKDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 dwScanLine;
		SINT32 ddRVal;
		UINT32 GetScanLineAddr;
	} NPDISP_DDHAL_GETSCANLINEDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		SINT32 ddRVal;
		UINT32 DestroySurfaceAddr;
	} NPDISP_DDHAL_DESTROYSURFACEDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		SINT32 ddRVal;
		UINT32 SetClipListAddr;
	} NPDISP_DDHAL_SETCLIPLISTDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		UINT32 lpSurfAttached;
		SINT32 ddRVal;
		UINT32 AddAttachedSurfaceAddr;
	} NPDISP_DDHAL_ADDATTACHEDSURFACEDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		UINT32 bHasRect;
		NPDISP_DDRECTL rArea;
		UINT32 lpSurfData;
		SINT32 ddRVal;
		UINT32 LockAddr;
		UINT32 dwFlags;
	} NPDISP_DDHAL_LOCKDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		SINT32 ddRVal;
		UINT32 UnlockAddr;
	} NPDISP_DDHAL_UNLOCKDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		UINT32 dwFlags;
		NPDISP_DDCOLORKEY ckNew;
		SINT32 ddRVal;
		UINT32 SetColorKeyAddr;
	} NPDISP_DDHAL_SETCOLORKEYDATA;

	// DirectDraw DDBLTFXの32bitレイアウト。pointerを含むunionはUINT32で保持する。
	typedef struct {
		UINT32 dwSize;
		UINT32 dwDDFX;
		UINT32 dwROP;
		UINT32 dwDDROP;
		UINT32 dwRotationAngle;
		UINT32 dwZBufferOpCode;
		UINT32 dwZBufferLow;
		UINT32 dwZBufferHigh;
		UINT32 dwZBufferBaseDest;
		UINT32 dwZDestConstBitDepth;
		UINT32 dwZDestConst;
		UINT32 dwZSrcConstBitDepth;
		UINT32 dwZSrcConst;
		UINT32 dwAlphaEdgeBlendBitDepth;
		UINT32 dwAlphaEdgeBlend;
		UINT32 dwReserved;
		UINT32 dwAlphaDestConstBitDepth;
		UINT32 dwAlphaDestConst;
		UINT32 dwAlphaSrcConstBitDepth;
		UINT32 dwAlphaSrcConst;
		UINT32 dwFillColor;
		NPDISP_DDCOLORKEY ddckDestColorkey;
		NPDISP_DDCOLORKEY ddckSrcColorkey;
	} NPDISP_DDBLTFX;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDDestSurface;
		NPDISP_DDRECTL rDest;
		UINT32 lpDDSrcSurface;
		NPDISP_DDRECTL rSrc;
		UINT32 dwFlags;
		UINT32 dwROPFlags;
		NPDISP_DDBLTFX bltFX;
		SINT32 ddRVal;
		UINT32 BltAddr;
		UINT32 IsClipped;
		NPDISP_DDRECTL rOrigDest;
		NPDISP_DDRECTL rOrigSrc;
		UINT32 dwRectCnt;
		UINT32 prDestRects;
	} NPDISP_DDHAL_BLTDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		UINT32 dwFlags;
		SINT32 ddRVal;
		UINT32 GetBltStatusAddr;
	} NPDISP_DDHAL_GETBLTSTATUSDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpSurfCurr;
		UINT32 lpSurfTarg;
		UINT32 dwFlags;
		SINT32 ddRVal;
		UINT32 FlipAddr;
	} NPDISP_DDHAL_FLIPDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 lpDDSurface;
		UINT32 dwFlags;
		SINT32 ddRVal;
		UINT32 GetFlipStatusAddr;
	} NPDISP_DDHAL_GETFLIPSTATUSDATA;

	typedef struct {
		UINT32 lpDD;
		UINT32 dwToGDI;
		UINT32 dwReserved;
		SINT32 ddRVal;
		UINT32 FlipToGDISurfaceAddr;
	} NPDISP_DDHAL_FLIPTOGDISURFACEDATA;

#pragma pack(pop)

#pragma pack(push, 1)
	typedef struct {
		UINT16 width;
		UINT32 offset;
	} NPDISP_FONTCHARINFO3;
	typedef struct {
		SINT16 dfType;
		SINT16 dfPoints;
		SINT16 dfVertRes;
		SINT16 dfHorizRes;
		SINT16 dfAscent;
		SINT16 dfInternalLeading;
		SINT16 dfExternalLeading;
		SINT8 dfItalic;
		SINT8 dfUnderline;
		SINT8 dfStrikeOut;
		SINT16 dfWeight;
		SINT8 dfCharSet;
		SINT16 dfPixWidth;
		SINT16 dfPixHeight;
		SINT8 dfPitchAndFamily;
		SINT16 dfAvgWidth;
		SINT16 dfMaxWidth;
		UINT8 dfFirstChar;
		UINT8 dfLastChar;
		UINT8 dfDefaultChar;
		UINT8 dfBreakChar;

		SINT16 dfWidthBytes;
		UINT32 dfDevice;
		UINT32 dfFace;
		UINT32 dfBitsPointer;
		UINT32 dfBitsOffset;
		SINT8 dfReserved;
		/* The following fields present only for Windows 3.x fonts */
		SINT32 dfFlags;
		SINT16 dfAspace;
		SINT16 dfBspace;
		SINT16 dfCspace;
		UINT32 dfColorPointer;
		SINT32 dfReserved1[4];
	} NPDISP_FONTINFO;


	// np2側で控えておく情報

	typedef struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[2];
	} BITMAPINFO_1BPP;
	typedef struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[16];
	} BITMAPINFO_4BPP;
	typedef struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[256];
	} BITMAPINFO_8BPP;
	typedef struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[3];
	} BITMAPINFO_16BPP;
	typedef struct {
		BITMAPINFOHEADER bmiHeader;
	} BITMAPINFO_24BPP;
	typedef struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[3];
	} BITMAPINFO_32BPP;

	typedef struct {
		BITMAPINFOHEADER biHeader;
		RGBQUAD pal[256];
		char bmBits[4 * 8 * 8]; // Win3.1は8x8px上限
	} NPDISP_HOSTPATTERNBITMAP;

	typedef struct {
		NPDISP_LBRUSH lbrush;
		NPDISP_HOSTPATTERNBITMAP pattern;
		UINT8 actualColorNum; // 実際の色の数 0=無効（計算が必要）, 1=1色, 2=2色
		UINT32 actualColor; // 実際の色
		UINT32 actualColor2; // ディザの場合の第2色目
		double actualColor2Ratio; // ディザの場合の混合比
		HBRUSH brs; // Windows向け
		UINT32 refCount; // 参照数
	} NPDISP_HOSTBRUSH;

	typedef struct {
		NPDISP_LPEN lpen;
		UINT8 actualColorNum; // 実際の色の数 0=無効（計算が必要）, 1=1色
		UINT32 actualColor; // 実際の色
		HPEN pen; // Windows向け
		UINT32 refCount; // 参照数
	} NPDISP_HOSTPEN;


	// Windows向けコード群

	typedef struct {
		HDC hdc;
		void* pBits;
		HBITMAP hBmp;
		HGDIOBJ hOldBmp;
		UINT32 stride;
		BITMAPINFO* lpbi;

		HBITMAP hBmpDDB;

		UINT8 isDevMemBmp;
	} NPDISP_WINDOWS_BMPHDC;

	typedef struct {
		NPDISP_WINDOWS_BMPHDC bmphdc;
	} NPDISP_HOSTBITMAP;

	typedef struct {
		BITMAPINFO_8BPP bi;
		HDC hdc;
		void* pBits;
		HBITMAP hBmp;
		HGDIOBJ hOldBmp;
		HGDIOBJ hOldPen;
		HGDIOBJ hOldBrush;
		UINT32 stride;
		HFONT hFont;
		HGDIOBJ hOldhFont;

		HDC hdcShadow;
		void* pBitsShadow;
		HBITMAP hBmpShadow;
		HGDIOBJ hOldBmpShadow;
		RECT rectShadow;

		HDC hdcBltBuf;
		void* pBitsBltBuf;
		HBITMAP hBmpBltBuf;
		HGDIOBJ hOldBmpBltBuf;

		//HDC hdc16BltBuf;
		//HBITMAP hBmp16BltBuf;
		//HGDIOBJ hOldBmp16BltBuf;

		HDC hdcCursor;
		HBITMAP hBmpCursor;
		HBITMAP hOldBmpCursor;
		void* pBitsCursor;
		HDC hdcCursorMask;
		HBITMAP hBmpCursorMask;
		HBITMAP hOldBmpCursorMask;
		void* pBitsCursorMask;

		HDC hdcCache[3];

		UINT32 pensIdx;
		std::map<UINT32, NPDISP_HOSTPEN> pens;
		UINT32 brushesIdx;
		std::map<UINT32, NPDISP_HOSTBRUSH> brushes;
		UINT32 bitmapsIdx;
		std::map<UINT32, NPDISP_HOSTBITMAP> bitmaps;

		RECT dirtyRect;
		RECT lastCursorRect;
		bool cursorUpdated;

		RECT dciDirtyRect;
		RECT ddrawDirtyRect;

		NPDISP_DRAWMODE lastScreenDrawMode;

		HFONT hFontCache[NPDISP_FONT_CACHE_MAX];
		LOGFONTA logFontCache[NPDISP_FONT_CACHE_MAX];
		char fontFaceCache[NPDISP_FONT_CACHE_MAX][32];
	} NPDISP_WINDOWS;

	typedef struct {
		UINT32 funcId;

		std::vector<UINT8> npdisp_memread_buf; // リクエストされてから読み込み完了しているデータを表す
		UINT32 npdisp_memwrite_bufwpos; // リクエストされてから書き込み完了している位置を表す

		UINT32 npdisp_memread_curpos; // リクエストされてからのデータ読み取りバイト数
		UINT32 npdisp_memread_preloadcount; // データプリロードバイト数
		UINT32 npdisp_memwrite_curpos; // リクエストされてからのデータ書き込みバイト数

		UINT32 last_npdisp_memread_bufsize;
		UINT32 last_npdisp_memwrite_bufwpos;
	} NPDISP_MEMCACHE;
#pragma pack(pop)


#ifdef __cplusplus
}
#endif



#endif