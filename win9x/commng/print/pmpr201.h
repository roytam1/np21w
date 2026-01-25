/**
 * @file	pmpr201.h
 * @brief	PC-PR201系印刷クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "pmbase.h"
#include "cmdparser.h"

typedef enum {
	PRINT_PR201_CODEMODE_8BIT = 0,
	PRINT_PR201_CODEMODE_7BIT = 1,
} PRINT_PR201_CODEMODE;

typedef enum {
	PRINT_PR201_PRINTMODE_N = 'N', // HSパイカ
	PRINT_PR201_PRINTMODE_H = 'H', // HDパイカ
	PRINT_PR201_PRINTMODE_Q = 'Q', // コンデンス
	PRINT_PR201_PRINTMODE_E = 'E', // エリート
	PRINT_PR201_PRINTMODE_P = 'P', // プロポーショナル
	PRINT_PR201_PRINTMODE_K = 'K', // 漢字横
	PRINT_PR201_PRINTMODE_t = 't', // 漢字縦
} PRINT_PR201_PRINTMODE;

typedef enum {
	PRINT_PR201_HSPMODE_NHS = '0',
	PRINT_PR201_HSPMODE_SHS = '1',
} PRINT_PR201_HSPMODE;

typedef enum {
	PRINT_PR201_CHARMODE_8BIT_KATAKANA = '$',
	PRINT_PR201_CHARMODE_8BIT_HIRAGANA = '&',
	PRINT_PR201_CHARMODE_7BIT_ASCII = '$',
	PRINT_PR201_CHARMODE_7BIT_HIRAGANA = '&',
	PRINT_PR201_CHARMODE_7BIT_CG = '#',
	// 以下、エミュレーションの都合上の値
	PRINT_PR201_CHARMODE_7BIT_KATAKANA = '%',
} PRINT_PR201_CHARMODE;

typedef enum {
	PRINT_PR201_SCRIPTMODE_DISABLE = '0',
	PRINT_PR201_SCRIPTMODE_SUPER = '1',
	PRINT_PR201_SCRIPTMODE_SUB = '2',
} PRINT_PR201_SCRIPTMODE;

typedef struct {
	float posX; // 描画位置X pixel
	float posY; // 描画位置Y pixel
	float actualLineHeight; // 現在の行の実際の高さ

	float leftMargin; // 左マージン幅 インチ単位
	float rightMargin;  // 右マージン幅 インチ単位
	float topMargin; // 上マージン幅 インチ単位

	bool isKanji; // 漢字(2byte)モードかどうか

	PRINT_PR201_CODEMODE codemode; // 8bit/7bitコードモード（PRINT_PR201_CHARMODEの解釈が変わる）

	bool isSelect; // SELECT/DESELECT状態
	PRINT_PR201_PRINTMODE mode; // 印字モード
	PRINT_PR201_HSPMODE hspMode; // HSパイカモード
	PRINT_PR201_CHARMODE charMode; // キャラクタモード
	PRINT_PR201_SCRIPTMODE scriptMode; // スクリプトモード
	bool downloadCharMode; // ダウンロード文字印字モード
	int charScaleX; // 文字スケールX
	int charScaleY; // 文字スケールY
	float lpi; // lines per inch
	bool bold; // 太字
	int lineselect; // 下線・上線選択
	int linep1; // param1 S=実線
	int linep2; // param2 1=一重線, 1=二重線
	int linep3; // param3 線の太さ 2=細線, 4=中線
	int linecolor; // 線色
	bool lineenable; // 下線・上線有効
	int dotsp_left; // 左ドットスペース
	int dotsp_right; // 右ドットスペース
	bool copymode; // コピーモード
	int color; // 色

	bool hasGraphic; // グラフィック印字があるかどうか
	float graphicPosY; // グラフィックの描画位置Y pixel

	void SetDefault()
	{
		posX = 0;
		posY = 0;
		actualLineHeight = 0;

		leftMargin = 0; // 左マージン位置 インチ単位
		rightMargin = 13.6;  // 右マージン位置 インチ単位
		topMargin = 0; // 上マージン幅 インチ単位

		isKanji = false;

		codemode = PRINT_PR201_CODEMODE_8BIT;

		isSelect = true;
		mode = PRINT_PR201_PRINTMODE_N;
		hspMode = PRINT_PR201_HSPMODE_NHS;
		charMode = PRINT_PR201_CHARMODE_8BIT_KATAKANA;
		scriptMode = PRINT_PR201_SCRIPTMODE_DISABLE;
		downloadCharMode = false;
		charScaleX = 1;
		charScaleY = 1;
		lpi = 1.0 / 6;
		bold = false;
		lineselect = 1;
		linep1 = 'S';
		linep2 = 1;
		linep3 = 2;
		linecolor = 0;
		lineenable = false;
		dotsp_left = 0;
		dotsp_right = 0;
		copymode = false;
		color = 0;

		hasGraphic = false;
		graphicPosY = 0;
	}
} PRINT_PR201_STATE;

typedef struct {
	HFONT oldfont;			/*!< Old Font */
	HFONT fontbase;			/*!< Font Base */
	HFONT fontrot90;		/*!< Font Rotation */
	HFONT fontbold;			/*!< Bold Font Base */
	HFONT fontboldrot90;	/*!< Bold Font Rotation */
	HPEN penline; // ライン用ペン
	HBRUSH brsDot[8]; // ドット描画用ブラシ8色分

	UINT8 lastlinecolor;
	UINT8 lastlinep1;
	UINT8 lastlinep2;
	UINT8 lastlinep3;
} PRINT_PR201_GDIOBJ;

/**
 * @brief PC-PR201系印刷クラス
 */
class CPrintPR201 : public CPrintBase
{
public:
	CPrintPR201();
	virtual ~CPrintPR201();

	virtual void StartPrint(HDC hdc, int offsetXPixel, int offsetYPixel, int widthPixel, int heightPixel, float dpiX, float dpiY, float dotscale, bool rectdot);
	
	virtual void EndPrint();

	virtual bool Write(UINT8 data);
	
	virtual PRINT_COMMAND_RESULT DoCommand();

	virtual bool HasRenderingCommand();

	bool CheckOverflowLine(float addCharWidth);
	bool CheckOverflowPage(float addLineHeight);

	void UpdateFont();
	void UpdateLinePen();

	double CalcDotPitchX() {
		return m_dpiX / (float)160; // PC-PR201 1画素あたりが1/160インチ
	}
	double CalcDotPitchY() {
		return m_dpiY / (float)160; // PC-PR201 1画素あたりが1/160インチ
	}
	double CalcVFULineHeight() {
		return m_dpiY / 6; // 1/6 inch で1行
	}
	double CalcLineHeight() {
		return m_dpiY / m_state.lpi; // 設定されている行の高さ
	}
	double CalcActualLineHeight() {
		return max(m_state.actualLineHeight, CalcLineHeight()); // 設定されている行の高さ
	}
	double CalcCPI() {
		return 14;
	}
	double CalcCurrentLetterWidth() {
		float charWidth = (float)m_dpiX / CalcCPI();
		if (m_state.mode == PRINT_PR201_PRINTMODE_Q) { // コンデンス
			charWidth *= 0.6;
		}
		else if (m_state.mode == PRINT_PR201_PRINTMODE_E) { // エリート
			charWidth *= 0.8;
		}
		else if (m_state.mode == PRINT_PR201_PRINTMODE_P) { // プロポーショナル XXX; 本当は字の幅が可変
			charWidth *= 0.9;
		}
		return charWidth;
	}

	PRINT_PR201_STATE m_state; // PC-PR201状態

	PRINT_PR201_GDIOBJ m_gdiobj; // GDI描画用オブジェクト

	UINT8* m_colorbuf;	// カラーバッファ
	int m_colorbuf_w;	// カラーバッファ幅
	int m_colorbuf_h;	// カラーバッファ高さ
	
private:
	void RenderGraphic();
	void Render(int count);

	PrinterCommandParser* m_parser;

	int m_cmdIndex; // 実行中コマンドのインデックス
	bool m_lastNewPage; // 前回ページ送りしたかどうか

	PRINT_PR201_STATE m_renderstate; // PC-PR201状態（描画用バックアップ）

};
