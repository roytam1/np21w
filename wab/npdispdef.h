/**
 * @file	npdispdef.h
 * @brief	Definition of the Neko Project II Display Adapter
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP)

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
#define NPDISP_FUNCORDER_INT2Fh					0xff2f // 序数がないので0xff2fとしておく
#define NPDISP_FUNCORDER_WEP					0xffff // 序数がないので0xffffとしておく

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
			//struct
			//{
			//	UINT32 lpRetValueAddr;
			//	UINT32 lpDestDevAddr;
			//	UINT16 wDestXOrg;
			//	UINT16 wDestYOrg;
			//	UINT32 lpClipRectAddr;
			//	UINT32 lpStringAddr;
			//	SINT16 wCount;
			//	UINT32 lpFontInfoAddr;
			//	UINT32 lpDrawModeAddr;
			//	UINT32 lpTextXFormAddr;
			//} strBlt;
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
				UINT16 wDestXext;
				UINT16 wDestYext;
				UINT32 lpSrcDevAddr;
				SINT16 wSrcX;
				SINT16 wSrcY;
				UINT16 wSrcXext;
				UINT16 wSrcYext;
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
				UINT16 wStartX;
				UINT16 wStartY;
				UINT16 wExtX;
				UINT16 wExtY;
				UINT32 lpTranslateAddr;
			} updateColors;
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
		char dummy[3];
		NPDISP_LPEN lpen; // NPDISP_PENの先頭はLPENとする
		int key; // np2側のキー 
	} NPDISP_PEN;
	typedef struct {
		char dummy[54];
		NPDISP_LBRUSH lbrush; // NPDISP_BRUSHの先頭はLBRUSHとする
		int key; // np2側のキー 
	} NPDISP_BRUSH;
	typedef struct {
		NPDISP_LFONT lfont; // NPDISP_FONTの先頭はNPDISP_LFONTとする
		int key; // np2側のキー 
	} NPDISP_FONT;

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
		SINT16 reserved1;
		SINT16 reserved2;
	} NPDISP_PBITMAP;

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
		NPDISP_PBITMAP bmp;
		char dummy[4];
	} NPDISP_PDEVICE;

	typedef struct {
		UINT16 csHotX;
		UINT16 csHotY;
		UINT16 csWidth;
		UINT16 csHeight;
		UINT16 csWidthBytes;
		UINT16 csColor;
	} NPDISP_CURSORSHAPE;
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
		SINT8 dfDefaultChar;
		SINT8 dfBreakChar;

		SINT16 dfWidthBytes;
		SINT32 dfDevice;
		SINT32 dfFace;
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
		SINT16 bmType;
		SINT16 bmWidth;
		SINT16 bmHeight;
		SINT16 bmWidthBytes;
		UINT8 bmPlanes;
		UINT8 bmBitsPixel;
		char bmBits[4 * 8 * 8]; // Win3.1は8x8px上限
	} NPDISP_HOSTPATTERNBITMAP;

	typedef struct {
		NPDISP_LBRUSH lbrush;
		NPDISP_HOSTPATTERNBITMAP pattern;
		HBRUSH brs; // Windows向け
		UINT32 refCount; // 参照数
	} NPDISP_HOSTBRUSH;

	typedef struct {
		NPDISP_LPEN lpen;
		HPEN pen; // Windows向け
		UINT32 refCount; // 参照数
	} NPDISP_HOSTPEN;



	// Windows向けコード群

	typedef struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD          bmiColors[256];
	} BITMAPINFO_8BPP;

	typedef struct {
		BITMAPINFO_8BPP bi;
		HDC hdc;
		void* pBits;
		HBITMAP hBmp;
		HPALETTE hPalette;
		HGDIOBJ hOldBmp;
		HGDIOBJ hOldPen;
		HGDIOBJ hOldBrush;
		HPALETTE hOldPalette;
		UINT32 stride;
		HFONT hFont;

		HDC hdcShadow;
		void* pBitsShadow;
		HBITMAP hBmpShadow;
		HGDIOBJ hOldBmpShadow;
		HPALETTE hOldPaletteShadow;
		RECT rectShadow;

		HDC hdcBltBuf;
		void* pBitsBltBuf;
		HBITMAP hBmpBltBuf;
		HGDIOBJ hOldBmpBltBuf;
		HPALETTE hOldPaletteBltBuf;

		HDC hdcCursor;
		HBITMAP hBmpCursor;
		HBITMAP hOldBmpCursor;
		HDC hdcCursorMask;
		HBITMAP hBmpCursorMask;
		HBITMAP hOldBmpCursorMask;
		HBRUSH scanlineBrush;

		HDC hdcCache[2];

		UINT32 pensIdx;
		std::map<UINT32, NPDISP_HOSTPEN> pens;
		UINT32 brushesIdx;
		std::map<UINT32, NPDISP_HOSTBRUSH> brushes;
	} NPDISP_WINDOWS;

	typedef struct {
		HDC hdc;
		void* pBits;
		HBITMAP hBmp;
		HGDIOBJ hOldBmp;
		UINT32 stride;
		BITMAPINFO* lpbi;
	} NPDISP_WINDOWS_BMPHDC;

#pragma pack(pop)


#ifdef __cplusplus
}
#endif



#endif