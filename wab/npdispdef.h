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
#define NPDISP_DDBRIDGE_CB_DD_FLIPTOGDI		0x0009UL
#define NPDISP_DDBRIDGE_CB_DD_GETDRIVERINFO		0x000aUL
#define NPDISP_DDBRIDGE_CB_SURF_DESTROY		0x0100UL
#define NPDISP_DDBRIDGE_CB_SURF_FLIP			0x0101UL
#define NPDISP_DDBRIDGE_CB_SURF_SETCLIPLIST		0x0102UL
#define NPDISP_DDBRIDGE_CB_SURF_LOCK			0x0103UL
#define NPDISP_DDBRIDGE_CB_SURF_UNLOCK			0x0104UL
#define NPDISP_DDBRIDGE_CB_SURF_BLT			0x0105UL
#define NPDISP_DDBRIDGE_CB_SURF_SETCOLORKEY		0x0106UL
#define NPDISP_DDBRIDGE_CB_SURF_ADDATTACHED		0x0107UL
#define NPDISP_DDBRIDGE_CB_SURF_GETBLTSTATUS		0x0108UL
#define NPDISP_DDBRIDGE_CB_SURF_GETFLIPSTATUS		0x0109UL
#define NPDISP_DDBRIDGE_CB_SURF_UPDATEOVERLAY		0x010aUL
#define NPDISP_DDBRIDGE_CB_SURF_SETOVERLAYPOS		0x010bUL
#define NPDISP_DDBRIDGE_CB_SURF_SETPALETTE		0x010dUL
#define NPDISP_DDBRIDGE_CB_PAL_DESTROY			0x0200UL
#define NPDISP_DDBRIDGE_CB_PAL_SETENTRIES		0x0201UL
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

// DriverInit呼び出し中はDDHALINFOのD3D用領域を2Dブリッジ初期化情報として使用する。
#define NPDISP_DDBRIDGE_REQUEST_MAGIC          0x4444504eUL
#define NPDISP_DDBRIDGE_ACK_MAGIC              0x4b4f4444UL
#define NPDISP_DDBRIDGE_ABI_VERSION            0x00010000UL
#define NPDISP_DDBRIDGE_FEATURE_GETDRIVERINFO  0x00000001UL
#define NPDISP_DDBRIDGE_FEATURE_SUPPORTED      NPDISP_DDBRIDGE_FEATURE_GETDRIVERINFO

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
#define NPDISP_DDBRIDGE_REQUEST_FEATURES       0x00000000UL
#define NPDISP_DDCAPS_ALPHA                  0x00800000UL
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
#define NPDISP_DDSCAPS_BACKBUFFER             0x00000004UL
#define NPDISP_DDSCAPS_COMPLEX                0x00000008UL
#define NPDISP_DDSCAPS_FLIP                   0x00000010UL
#define NPDISP_DDSCAPS_FRONTBUFFER            0x00000020UL
#define NPDISP_DDSCAPS_OFFSCREENPLAIN         0x00000040UL
#define NPDISP_DDSCAPS_OVERLAY                0x00000080UL
#define NPDISP_DDSCAPS_PRIMARYSURFACE        0x00000200UL
#define NPDISP_DDSCAPS_SYSTEMMEMORY          0x00000800UL
#define NPDISP_DDSCAPS_VIDEOMEMORY           0x00004000UL
#define NPDISP_DDSCAPS_VISIBLE               0x00008000UL
#define NPDISP_DDRAWISURF_HASPIXELFORMAT     0x00002000UL
#define NPDISP_DDPF_ALPHA                    0x00000002UL
#define NPDISP_DDPF_FOURCC                   0x00000004UL
#define NPDISP_DDPF_PALETTEINDEXED8          0x00000020UL
#define NPDISP_DDPF_RGB                      0x00000040UL
#define NPDISP_DD_FOURCC_YUY2                0x32595559UL
#define NPDISP_DD_FOURCC_SLOT_OFFSET         (NPDISP_DD_MAX_MODES * (UINT32)sizeof(NPDISP_DDHALMODEINFO))
#define NPDISP_DDMODEINFO_PALETTIZED          0x0001U
#define NPDISP_DD_MAX_MODES                   128U
#define NPDISP_DD_MAX_CLIP_RECTS              65536U
#define NPDISP_DDSD_PITCH                    0x00000008UL
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
		};
		union
		{
			UINT32 dwRBitMask;
			UINT32 dwYBitMask;
		};
		union
		{
			UINT32 dwGBitMask;
			UINT32 dwUBitMask;
		};
		union
		{
			UINT32 dwBBitMask;
			UINT32 dwVBitMask;
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
		UINT32 dwMipMapCount;
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