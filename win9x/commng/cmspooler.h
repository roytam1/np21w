/**
 * @file	cmspooler.h
 * @brief	Windowsスプーラ印刷 クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "cmbase.h"

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

	static CComSpooler* CreateInstance(LPCTSTR printerName, int pageTimeout, bool emulation);

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
	DWORD m_jobId;

	bool Initialize(LPCTSTR printerName, int pageTimeout, bool emulation);
	bool CCOpenPrinter();
	void CCStartDocPrinter();
};
