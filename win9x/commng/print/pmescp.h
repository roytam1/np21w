/**
 * @file	pmescp.h
 * @brief	ESC/P系印刷クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "pmbase.h"
#include "cmdparser.h"

#define HTAB_SETS		33
#define VTAB_CHANNELS	8
#define VTAB_SETS		17

typedef struct {
	float posX; // 描画位置X pixel
	float posY; // 描画位置Y pixel

	float defUnit; // 単位
	float charPitchX; // 現在の1文字の幅
	float charPitchKanjiX; // 現在の1文字の幅（漢字）
	float charPoint; // 現在の文字のポイント
	float linespacing; // 現在の行の高さ
	float pagelength; // ページの長さ（インチ）
	float leftMargin; // 左マージン（インチ）
	float rightMargin; // 右マージン（インチ）
	float topMargin; // 上マージン（インチ）
	float bottomMargin; // 下マージン（インチ）
	int color; // 色

	float hTabPositions[HTAB_SETS]; // 水平タブ位置（文字単位）
	float vTabPositions[VTAB_CHANNELS][VTAB_SETS]; // 垂直タブ位置（行単位）
	int vTabCh; // 垂直タブチャネル

	float exSpc; // 1文字あたりの追加スペース（LQ 1/180単位、Draft 1/120単位）

	float graphicsPitchX; // グラフィックのピッチX
	float graphicsPitchY; // グラフィックのピッチY
	float graphicPosY; // グラフィックの描画位置Y pixel

	bool isCustomDefUnit; // defUnitが指定されているかどうか
	bool isKanji; // 漢字(2byte)モードかどうか
	bool isHalfKanji; // 半角漢字モードかどうか
	bool isRotKanji; // 縦漢字モードかどうか
	bool isGraphMode; // グラフィックモードかどうか
	bool isSansSerif; // サンセリフ書体かどうか
	bool isBold; // 太字かどうか
	bool isItalic; // 斜体かどうか
	bool isCondensed; // コンデンスモードかどうか
	bool isDoubleWidth; // 倍角モードかどうか
	bool isDoubleWidthSingleLine; // 単一行倍角モードかどうか
	bool isDoubleHeight; // 縦倍角モードかどうか
	bool hasGraphic; // グラフィック印字があるかどうか

	void SetDefault()
	{
		posX = 0;
		posY = 0;

		defUnit = 1.0 / 180;

		charPitchX = 0.15;
		charPitchKanjiX = 0.14;
		charPoint = 10.8;
		linespacing = 0.15;
		pagelength = 11.69; // XXX: A4縦
		leftMargin = 0; // 左マージン（インチ）
		rightMargin = 0; // 右マージン（インチ）
		topMargin = 0; // 上マージン（インチ）
		topMargin = 0; // 下マージン（インチ）

		vTabCh = 0;
		for (int i = 0; i < HTAB_SETS; i++) {
			hTabPositions[i] = i * 8 * charPitchX;
		}
		for (int ch = 0; ch < VTAB_CHANNELS; ch++) {
			for (int i = 0; i < VTAB_SETS; i++) {
				vTabPositions[ch][i] = 0 * linespacing;
			}
		}

		exSpc = 0;

		graphicsPitchX = 1;
		graphicsPitchY = 1;
		graphicPosY = 0;

		isCustomDefUnit = false;
		isKanji = false;
		isHalfKanji = false;
		isRotKanji = false;
		isGraphMode = false;
		isSansSerif = false;
		isBold = false;
		isItalic = false;
		isCondensed = false;
		isDoubleWidth = false;
		isDoubleWidthSingleLine = false;
		isDoubleHeight = false;
	}

	double CalcDotPitchX() {
		return 1 / (float)160; // 1画素あたりが1/160インチ
	}
	double CalcDotPitchY() {
		return 1 / (float)160; // 1画素あたりが1/160インチ
	}
	double CalcCharPitchX() {
		return isKanji ? charPitchKanjiX : charPitchX;
	}
} PRINT_ESCP_STATE;

typedef struct {
	HFONT oldfont;				/*!< Old Font */
	HFONT fontbase;				/*!< Font Base */
	HFONT fontSansSerif;		/*!< Font Sans-serif */
	HFONT fontBoldbase;			/*!< Bold Font Base */
	HFONT fontBoldSansSerif;	/*!< Bold Font Sans-serif */
	HFONT fontItalicbase;		/*!< Italic Font Base */
	HFONT fontItalicSansSerif;	/*!< Italic Font Sans-serif */
	HFONT fontItalicBoldbase;		/*!< Italic Bold Font Base */
	HFONT fontItalicBoldSansSerif;	/*!< Italic Bold Font Sans-serif */
	HBRUSH brsDot[8]; // ドット描画用ブラシ8色分
} PRINT_ESCP_GDIOBJ;

/**
 * @brief ESC/P系印刷クラス
 */
class CPrintESCP : public CPrintBase
{
public:
	CPrintESCP();
	virtual ~CPrintESCP();

	virtual void StartPrint(HDC hdc, int offsetXPixel, int offsetYPixel, int widthPixel, int heightPixel, float dpiX, float dpiY, float dotscale, bool rectdot);
	
	virtual void EndPrint();

	virtual bool Write(UINT8 data);

	bool CheckOverflowLine(float addCharWidth);

	bool CheckOverflowPage(float addLineHeight);

	virtual PRINT_COMMAND_RESULT DoCommand();

	virtual bool HasRenderingCommand();

	void UpdateFont();
	void UpdateFontSize();

	PRINT_ESCP_STATE m_state; // ESC/P状態

	PRINT_ESCP_GDIOBJ m_gdiobj; // GDI描画用オブジェクト

	UINT8* m_colorbuf;	// カラーバッファ
	int m_colorbuf_w;	// カラーバッファ幅
	int m_colorbuf_h;	// カラーバッファ高さ
private:
	void CPrintESCP::RenderGraphic();
	void Render(int count);

	PrinterCommandParser* m_parser;

	int m_cmdIndex; // 実行中コマンドのインデックス
	bool m_lastNewPage; // 前回ページ送りしたかどうか

	PRINT_ESCP_STATE m_renderstate; // ESC/P状態 描画用

	void ReleaseFont();
};
