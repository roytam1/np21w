/**
 * @file	cmspooler.h
 * @brief	Windowsスプーラ印刷 クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include <vector>

#include "cmbase.h"
#include "print/pmbase.h"

#define PRINT_EMU_MODE_RAW		0
#define PRINT_EMU_MODE_PR201	1
#define PRINT_EMU_MODE_ESCP		2

#define ESCPEMU_PAGE_ALIGNMENT_LEFT		0
#define ESCPEMU_PAGE_ALIGNMENT_CENTER	1

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
	UINT8 m_emulation;				/*!< プリンタエミュレーションモード */
	bool m_isOpened;				/*!< プリンタ開いている */
	bool m_isStart;					/*!< ページ開始状態 */
	TCHAR m_printerName[MAX_PATH];  /*!< プリンタ名 */
	HANDLE m_hPrinter;				/*!< プリンタ ハンドル */
	DWORD m_jobId;					/*!< プリンタジョブID */
	bool m_lastHasError;			/*!< プリンタオープン等に失敗 */

	// プリンタエミュレーション用
	CPrintBase *m_print;			/*!< プリンタエミュレーション */

	HDC m_hdc;						/*!< GDIプリンタHDC */
	bool m_requestNewPage;			/*!< 改ページリクエスト（空白ページが末尾に付くのを防ぐため用） */

	bool m_rectdot;				/*!< 点を矩形で描画 */
	float m_dotscale;			/*!< 点の大きさ補正 */
	UINT8* m_colorbuf;			/*!< カラーバッファ */
	int m_colorbuf_w;			/*!< カラーバッファ幅 */
	int m_colorbuf_h;			/*!< カラーバッファ高さ */
	int m_pageAlignment;		/*!< ページアライメント */
	int m_additionalOfsX;		/*!< 追加位置オフセット X */
	int m_additionalOfsY;		/*!< 追加位置オフセット Y */
	float m_scale;				/*!< スケール調整 */

	bool SetConfigurations(COMCFG* comcfg);
	bool Initialize(COMCFG* comcfg);
	bool CCOpenPrinter();
	void CCStartDocPrinter();

	void CCStartPrint();
};
