/**
 * @file	cmspooler.h
 * @brief	Windowsスプーラ印刷 クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include <vector>

#include "cmbase.h"

typedef struct {
	HFONT oldfont;			/*!< ESC/P Old Font */
	HFONT fontbase;			/*!< ESC/P Font Base */
	HFONT fontrot90;		/*!< ESC/P Font Rotation */
	HFONT fontbold;			/*!< ESC/P Bold Font Base */
	HFONT fontboldrot90;	/*!< ESC/P Bold Font Rotation */
	HPEN penline; // ライン用ペン
	HBRUSH brsDot[8]; // ドット描画用ブラシ8色分
} ESCP_GDIOBJ;

typedef struct {
	UINT8 cmd;				/*!< 現在のESC/Pコマンド */
	int cpi;				/*!< ESC/P Characters Per Inch */
	float lpi;				/*!< ESC/P Lines Per Inch */
	float posx;				/*!< ESC/P Position X */
	float posy;				/*!< ESC/P Position Y */
	float renderofsy;		/*!< ESC/P Render Offset Y */
	int scalex;				/*!< ESC/P Scale X */
	int scaley;				/*!< ESC/P Scale Y */
	int scalex_graph;		/*!< ESC/P Scale X graph */
	int scaley_graph;		/*!< ESC/P Scale Y graph */
	int maxscaley;			/*!< ESC/P Maximum Scale Y */
	int maxlineheight;		/*!< ESC/P 行内最大高さ */
	bool so;				/*!< ESC/P 横倍角 */
	bool deselect;			/*!< ESC/P 印字不可能 */
	int escmode;			/*!< ESC/P 印字モード指定 */
	bool bold;				/*!< ESC/P 強調印字 */
	int lineselect;			/*!< ESC/P 下線・上線選択 */
	int linep1;				/*!< ESC/P param1 S=実線 */
	int linep2;				/*!< ESC/P param2 1=一重線, 1=二重線 */
	int linep3;				/*!< ESC/P param3 線の太さ 2=細線, 4=中線 */
	int linecolor;			/*!< ESC/P 線色 */
	bool lineenable;		/*!< ESC/P 下線・上線有効 */
	int dotsp_left;			/*!< ESC/P 左ドットスペース */
	int dotsp_right;		/*!< ESC/P 右ドットスペース */
	bool setVFUmode;		/*!< ESC/P VFU設定モード */
	int color;				/*!< ESC/P 色 */
	int hasgraphic;			/*!< ESC/P グラフィック描画ありフラグ */
	bool copymode;			/*!< ESC/P コピーモード */
	float delaynewpageposy;	/*!< ESC/P Delay New Page Position Y */
} ESCP_STAT;

/**
 * @brief commng パラレル デバイス クラス
 */
class CComSpooler : public CComBase
{
public:
	int m_pageTimeout;				/*!< プリンタタイムアウト */
	HANDLE m_hThreadTimeout;
	HANDLE m_hThreadExitEvent;
	CRITICAL_SECTION m_csPrint;
	DWORD m_lastSendTime;
	bool m_hasValidData;			/*!< 有効なデータが送られたか */
	int m_dataCounter;				/*!< プリンタに送られたデータ数 */

	static CComSpooler* CreateInstance(COMCFG *comcfg);

	void CCEndThread();
	void CCEndDocPrinter();

protected:
	CComSpooler();
	virtual ~CComSpooler();
	virtual UINT Read(UINT8* pData);
	virtual UINT Write(UINT8 cData);
	virtual UINT8 GetStat();
	virtual INTPTR Message(UINT nMessage, INTPTR nParam);

private:
	bool m_emulation;				/*!< プリンタエミュレーション有効 */
	bool m_isOpened;				/*!< プリンタ開いている */
	bool m_isStart;					/*!< ページ開始状態 */
	TCHAR m_printerName[MAX_PATH];  /*!< プリンタ名 */
	HANDLE m_hPrinter;				/*!< プリンタ ハンドル */
	DWORD m_jobId;					/*!< プリンタジョブID */
	bool m_lastHasError;			/*!< プリンタオープン等に失敗 */

	// ESC/Pエミュレーション用
	HDC m_hdc;						/*!< GDIプリンタHDC */
	int m_dpiX;						/*!< GDIプリンタDPI X */
	int m_dpiY;						/*!< GDIプリンタDPI Y */
	int m_physWidth;				/*!< GDIプリンタページ全体幅 */
	int m_physHeight;				/*!< GDIプリンタページ全体高さ */
	int m_physOffX;					/*!< GDIプリンタ有効印字領域開始 X */
	int m_physOffY;					/*!< GDIプリンタ有効印字領域開始 Y */
	int m_horzRes;					/*!< GDIプリンタ有効印字領域幅 */
	int m_vertRes;					/*!< GDIプリンタ有効印字領域高さ */
	std::vector<UINT8> m_escpbuf;	/*!< ESC/Pデータバッファ */
	std::vector<UINT8> m_escprlin;	/*!< ESC/Pデータ行描画バッファ */
	std::vector<UINT8> m_escprbuf;	/*!< ESC/Pデータ描画バッファ */
	ESCP_GDIOBJ m_escp_gdi;			/*!< ESC/P GDIオブジェクト */
	ESCP_STAT m_escp;				/*!< ESC/P 解析用状態 */
	ESCP_STAT m_escpr;				/*!< ESC/P 描画用状態 */
	bool m_escp_rectdot;			/*!< ESC/P で点を矩形で描画 */
	float m_escp_dotscale;			/*!< ESC/P で点の大きさ補正 */
	UINT8 *m_escp_colorbuf;			/*!< ESC/P カラーバッファ */
	int m_escp_colorbuf_w;			/*!< ESC/P カラーバッファ幅 */
	int m_escp_colorbuf_h;			/*!< ESC/P カラーバッファ高さ */

	bool Initialize(COMCFG* comcfg);
	bool CCOpenPrinter();
	void CCStartDocPrinter();

	void CCStartESCPPage(bool reset);
	void CCEndESCPPage();

	void CCResetESCP();
	void CCUpdateFont();
	void CCUpdateLinePen(UINT8 param1, UINT8 param2, UINT8 param3, UINT8 color);
	void CCCheckNewPage(bool delay = false);
	void CCCheckNewLine(int nextCharWidth = 0);
	void CCRenderAllESCP();
	void CCRenderESCP(UINT8 cData);
	void CCWriteLF();
	void CCWriteESCP(UINT8 cData);
};
