/**
 * @file	cmspooler.cpp
 * @brief	Windowsスプーラ印刷 クラスの動作の定義を行います
 */

#include "compiler.h"
#include "cmspooler.h"

#include <process.h>

static unsigned int __stdcall cComSpooler_TimeoutThread(LPVOID vdParam)
{
	CComSpooler* t = (CComSpooler*)vdParam;
	
	while (WaitForSingleObject(t->m_hThreadExitEvent, 500) == WAIT_TIMEOUT) {
		EnterCriticalSection(&t->m_csPrint);
		if (GetTickCounter() - t->m_lastSendTime >= t->m_pageTimeout) {
			t->CCEndDocPrinter();
			t->m_hThreadTimeout = NULL;
			LeaveCriticalSection(&t->m_csPrint);
			break;
		}
		LeaveCriticalSection(&t->m_csPrint);
	}
	return 0;
}


/**
 * インスタンス作成
 * @param[in] printerName プリンタ名
 * @param[in] pageTimeout 印刷終了タイムアウト時間(msec)
 * @param[in] emulation エミュレーション印刷モード
 * @return インスタンス
 */
CComSpooler* CComSpooler::CreateInstance(LPCTSTR printerName, int pageTimeout, bool emulation)
{
	CComSpooler* pPara = new CComSpooler;
	if (!pPara->Initialize(printerName, pageTimeout, emulation))
	{
		delete pPara;
		pPara = NULL;
	}
	return pPara;
}

/**
 * コンストラクタ
 */
CComSpooler::CComSpooler()
	: CComBase(COMCONNECT_PARALLEL)
	, m_emulation(false)
	, m_pageTimeout(5000)
	, m_lastSendTime(0)
	, m_isOpened(false)
	, m_isStart(false)
	, m_printerName()
	, m_hPrinter(NULL)
	, m_hThreadTimeout(NULL)
	, m_hThreadExitEvent(NULL)
	, m_csPrint()
	, m_hasValidData(false)
	, m_dataCounter(0)
	, m_jobId(0)
{
	InitializeCriticalSection(&m_csPrint);
	m_hThreadExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

/**
 * デストラクタ
 */
CComSpooler::~CComSpooler()
{
	if (m_isOpened)
	{
		CCEndThread();
		CCEndDocPrinter();

		::ClosePrinter(m_hPrinter);
		m_hPrinter = NULL;
		m_isOpened = false;
	}
	CloseHandle(m_hThreadExitEvent);
	DeleteCriticalSection(&m_csPrint);
}

/**
 * プリンタ開く
 */
bool CComSpooler::CCOpenPrinter()
{
	EnterCriticalSection(&m_csPrint);

	if (m_isOpened) goto finalize;

	TCHAR printerName[MAX_PATH];
	if (m_printerName && _tcslen(m_printerName) > 0) 
	{
		_tcscpy(printerName, m_printerName);
	}
	else
	{
		DWORD buflen = MAX_PATH;
		if (!GetDefaultPrinter(printerName, &buflen)) {
			goto finalize;
		}
		if (_tcslen(printerName) == 0) goto finalize;
	}
	if (!::OpenPrinter(printerName, &m_hPrinter, nullptr)) {
		m_isOpened = true;
		goto finalize;
	}
	m_isOpened = true;

finalize:
	LeaveCriticalSection(&m_csPrint);
	return m_isOpened;
}
/**
 * プリンタ印刷開始
 */
void CComSpooler::CCStartDocPrinter()
{
	DOC_INFO_1 di = {0};

	EnterCriticalSection(&m_csPrint);

	if (!m_isOpened) goto finalize;
	if (m_isStart) goto finalize;

	SYSTEMTIME st;
	GetLocalTime(&st);

	TCHAR documentName[MAX_PATH] = { 0 };
	_stprintf(documentName, _T("NP2_PRINT_%04u%02u%02u_%02u%02u%02u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	di.pDocName = documentName;
	di.pOutputFile = nullptr;
	di.pDatatype = _T("RAW");

	m_jobId = ::StartDocPrinter(m_hPrinter, 1, (LPBYTE)&di);
	if (m_jobId == 0) {
		goto finalize;
	}

	if (!::StartPagePrinter(m_hPrinter)) {
		::EndDocPrinter(m_hPrinter);
		goto finalize;
	}

	// タイムアウト監視開始
	if (m_pageTimeout > 0 && !m_hThreadTimeout) {
		unsigned int dwID;
		m_hThreadTimeout = (HANDLE)_beginthreadex(NULL, 0, cComSpooler_TimeoutThread, this, 0, &dwID);
	}

	m_lastSendTime = GetTickCounter();
	m_dataCounter = 0;
	m_hasValidData = false;
	m_isStart = true;
finalize:
	LeaveCriticalSection(&m_csPrint);
}
/**
 * タイムアウト監視スレッド終了
 */
void CComSpooler::CCEndThread()
{
	// タイムアウト監視終了
	if (m_hThreadTimeout) {
		SetEvent(m_hThreadExitEvent);
		if (WaitForSingleObject(m_hThreadTimeout, 10000) == WAIT_TIMEOUT)
		{
			TerminateThread(m_hThreadTimeout, 0); // ゾンビスレッド死すべし
		}
		CloseHandle(m_hThreadTimeout);
		m_hThreadTimeout = NULL;
	}
}

/**
 * プリンタ印刷終了
 */
void CComSpooler::CCEndDocPrinter()
{
	EnterCriticalSection(&m_csPrint);

	if (!m_isOpened) goto finalize;
	if (!m_isStart) goto finalize;

	if (!m_hasValidData && m_dataCounter <= 100) {
		// ごみデータと思われるので捨てる
		::SetJob(m_hPrinter, m_jobId, 0, NULL, JOB_CONTROL_CANCEL);
	}
	::EndPagePrinter(m_hPrinter);
	::EndDocPrinter(m_hPrinter);

	m_isStart = false;
finalize:
	LeaveCriticalSection(&m_csPrint);
}

/**
 * 初期化
 * @param[in] printerName プリンタ名
 * @param[in] pageTimeout 印刷終了タイムアウト時間(msec)
 * @param[in] emulation エミュレーション印刷モード
 * @retval true 成功
 * @retval false 失敗
 */
bool CComSpooler::Initialize(LPCTSTR printerName, int pageTimeout, bool emulation)
{
	if (printerName) {
		_tcscpy(m_printerName, printerName);
	}
	m_emulation = emulation;
	m_pageTimeout = pageTimeout;
	return true;
}

/**
 * 読み込み
 * @param[out] pData バッファ
 * @return サイズ
 */
UINT CComSpooler::Read(UINT8* pData)
{
	return 0;
}

/**
 * 書き込み
 * @param[out] cData データ
 * @return サイズ
 */
UINT CComSpooler::Write(UINT8 cData)
{
	UINT ret = 0;

	EnterCriticalSection(&m_csPrint);
	DWORD dwWrittenSize;
	if (!m_isOpened) {
		if (!CCOpenPrinter()) {
			goto finalize;
		}
	}

	if (!m_isStart) {
		// いきなりEOTは無視
		if (cData == 0x04) {
			ret = 1; // 成功扱い
			goto finalize;
		}

		CCStartDocPrinter();
		if (!m_isStart) {
			goto finalize;
		}
	}

	m_lastSendTime = GetTickCounter();
	if (m_dataCounter < 10000) {
		m_dataCounter++;
	}
	if (0x08 <= cData && cData <= 0x0d || 0x20 <= cData) {
		m_hasValidData = true;
	}
	ret = (::WritePrinter(m_hPrinter, &cData, 1, &dwWrittenSize)) ? 1 : 0;

finalize:
	LeaveCriticalSection(&m_csPrint);
	return ret;
}

/**
 * ステータスを得る
 * bit 7: ~CI (RI, RING)
 * bit 6: ~CS (CTS)
 * bit 5: ~CD (DCD, RLSD)
 * bit 4: reserved
 * bit 3: reserved
 * bit 2: reserved
 * bit 1: reserved
 * bit 0: ~DSR (DR)
 * @return ステータス
 */
UINT8 CComSpooler::GetStat()
{
	return 0x00;
}

/**
 * メッセージ
 * @param[in] nMessage メッセージ
 * @param[in] nParam パラメタ
 * @return リザルト コード
 */
INTPTR CComSpooler::Message(UINT nMessage, INTPTR nParam)
{
	switch (nMessage)
	{
		case COMMSG_PURGE:
			if (m_isOpened)
			{
				EnterCriticalSection(&m_csPrint);
				CCEndThread();
				CCEndDocPrinter();

				::ClosePrinter(m_hPrinter);
				m_hPrinter = NULL;
				m_isOpened = false;
				LeaveCriticalSection(&m_csPrint);
			}
			break;

		default:
			break;
	}
	return 0;
}
