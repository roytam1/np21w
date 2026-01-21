/**
 * @file	cmspooler.cpp
 * @brief	Windowsスプーラ印刷 クラスの動作の定義を行います
 */

#include "compiler.h"
#include "np2.h"
#include "cmspooler.h"

#include <process.h>
#include <codecnv/codecnv.h>

#if 1
#undef	TRACEOUT
static void trace_fmt_ex(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUT(s)	trace_fmt_ex s
static void trace_fmt_exw(const WCHAR* fmt, ...)
{
	WCHAR stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vswprintf(stmp, 2048, fmt, ap);
	wcscat(stmp, L"\n");
	va_end(ap);
	OutputDebugStringW(stmp);
}
#define	TRACEOUTW(s)	trace_fmt_exw s
#else
#define	TRACEOUTW(s)	(void)0
#endif	/* 1 */

static float graph_escp_lpi = (float)160 / 24;

 // モーダルダイアログを勝手に表示されたときに、マウスカーソルを出すようにする
static HHOOK g_hCbt = nullptr;
static bool g_mouseOn = false;
static LRESULT CALLBACK CbtProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HCBT_CREATEWND)
	{
		g_mouseOn = true;
		ShowCursor(TRUE);
	}
	return CallNextHookEx(g_hCbt, nCode, wParam, lParam);
}
static void InstallThreadModalDetectHook()
{
	if (g_hCbt) return;
	g_mouseOn = false;
	DWORD tid = GetCurrentThreadId();
	g_hCbt = SetWindowsHookExW(WH_CBT, CbtProc, nullptr, tid);
}
static void UninstallThreadModalDetectHook()
{
	if (!g_hCbt) return;
	UnhookWindowsHookEx(g_hCbt);
	g_hCbt = nullptr;
	if (g_mouseOn) {
		ShowCursor(FALSE);
	}
}

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

static bool GetActualPrinterName(const LPTSTR printerName, LPTSTR actualPrinterName)
{
	if (printerName && _tcslen(printerName) > 0)
	{
		_tcscpy(actualPrinterName, printerName);
	}
	else
	{
		DWORD buflen = MAX_PATH;
		if (!GetDefaultPrinter(actualPrinterName, &buflen)) {
			return false;
		}
		if (_tcslen(actualPrinterName) == 0) return false;
	}
	return true;
}

static unsigned short jis_to_sjis(unsigned short jis)
{
	UINT8 j1 = (UINT8)(jis >> 8);
	UINT8 j2 = (UINT8)(jis & 0xFF);

	/* JIS X 0208 の有効範囲チェック */
	if (j1 < 0x21 || j1 > 0x7E || j2 < 0x21 || j2 > 0x7E) {
		return 0;
	}

	/*
	 *   s1 = (j1 + 1)/2 + 0x70;  s1 >= 0xA0 なら s1 += 0x40;
	 *   s2 = j2 + 0x1F;          j1 が偶数なら s2 += 0x5E;
	 *   s2 >= 0x7F なら s2++;
	 */
	UINT8 s1 = (UINT8)(((j1 + 1) >> 1) + 0x70);
	if (s1 >= 0xA0) {
		s1 = (UINT8)(s1 + 0x40);
	}

	UINT8 s2 = (UINT8)(j2 + 0x1F);
	if ((j1 & 1) == 0) {               /* j1 が偶数（区が偶数） */
		s2 = (UINT8)(s2 + 0x5E);
	}
	if (s2 >= 0x7F) {
		s2 = (UINT8)(s2 + 1);
	}

	return (UINT16)((UINT16)s1 << 8 | s2);
}

static COLORREF ColorCodeToColorRef(UINT8 colorCode)
{
	switch (colorCode) {
	case 0:
		return RGB(0,0,0);
	case 1:
		return RGB(0, 0, 255);
	case 2:
		return RGB(255, 0, 0);
	case 3:
		return RGB(255, 0, 255);
	case 4:
		return RGB(0, 255, 0);
	case 5:
		return RGB(0, 255, 255);
	case 6:
		return RGB(255, 255, 0);
	case 7:
		return RGB(255, 255, 255);
	}
	return RGB(0, 0, 0);
}


/**
 * インスタンス作成
 * @param[in] comcfg COMCFG
 * @return インスタンス
 */
CComSpooler* CComSpooler::CreateInstance(COMCFG* comcfg)
{
	CComSpooler* pPara = new CComSpooler;
	if (!pPara->Initialize(comcfg))
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
	, m_hdc(NULL)
	, m_escpbuf()
	, m_escp_gdi()
	, m_escp()
	, m_escpr()
	, m_escp_rectdot(false)
	, m_escp_dotscale(1.0f)
	, m_escp_colorbuf(NULL)
	, m_escp_colorbuf_w(0)
	, m_escp_colorbuf_h(0)

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

		if (!m_emulation) {
			::ClosePrinter(m_hPrinter);
			m_hPrinter = NULL;
			m_isOpened = false;
		}
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
	if (!GetActualPrinterName(m_printerName, printerName)) {
		goto finalize;
	}
	if (!m_emulation) {
		if (!::OpenPrinter(printerName, &m_hPrinter, nullptr)) {
			m_isOpened = true;
			goto finalize;
		}
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
	InstallThreadModalDetectHook();
	EnterCriticalSection(&m_csPrint);

	if (!m_isOpened) goto finalize;
	if (m_isStart) goto finalize;

	SYSTEMTIME st;
	GetLocalTime(&st);

	TCHAR documentName[MAX_PATH] = { 0 };
	_stprintf(documentName, _T("NP2_PRINT_%04u%02u%02u_%02u%02u%02u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	if (!m_emulation) {
		DOC_INFO_1 di = { 0 };
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
	}
	else {
		TCHAR printerName[MAX_PATH];
		if (!GetActualPrinterName(m_printerName, printerName)) {
			goto finalize;
		}

		m_hdc = CreateDCW(L"WINSPOOL", printerName, nullptr, nullptr);
		if (!m_hdc) {
			goto finalize;
		}

		DOCINFO di = { 0 };
		di.cbSize = sizeof(di);
		di.lpszDocName = documentName;
		m_jobId = ::StartDoc(m_hdc, &di);
		if (m_jobId <= 0) {
			m_jobId = 0;
			DeleteDC(m_hdc);
			goto finalize;
		}

		if (::StartPage(m_hdc) <= 0) {
			EndDoc(m_hdc);
			DeleteDC(m_hdc);
			goto finalize;
		}

		CCStartESCPPage(true);
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
	UninstallThreadModalDetectHook();
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

	if (!m_emulation) {
		if (!m_hasValidData && m_dataCounter <= 100) {
			// ごみデータと思われるので捨てる
			::SetJob(m_hPrinter, m_jobId, 0, NULL, JOB_CONTROL_CANCEL);
		}
		::EndPagePrinter(m_hPrinter);
		::EndDocPrinter(m_hPrinter);
	}
	else {
		if (!m_hasValidData && m_dataCounter <= 100) {
			// ごみデータと思われるので捨てる
			::AbortDoc(m_hdc);
		}
		else {
			CCRenderAllESCP();
			CCEndESCPPage();

			::EndPage(m_hdc);
			::EndDoc(m_hdc);
		}

		DeleteDC(m_hdc);
		m_hdc = NULL;
	}

	m_isStart = false;
finalize:
	LeaveCriticalSection(&m_csPrint);
}

/**
 * 初期化
 * @param[in] comcfg COMCFG
 * @retval true 成功
 * @retval false 失敗
 */
bool CComSpooler::Initialize(COMCFG* comcfg)
{
	if (comcfg->spoolPrinterName) {
		_tcscpy(m_printerName, comcfg->spoolPrinterName);
	}
	m_emulation = comcfg->spoolEmulation;
	m_pageTimeout = comcfg->spoolTimeout;
	m_escp_dotscale = comcfg->spoolDotSize / 100.0;
	m_escp_rectdot = comcfg->spoolRectDot ? true : false;

	m_lastHasError = false;

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
	DWORD lastSendTime = m_lastSendTime;
	m_lastSendTime = GetTickCounter();

	EnterCriticalSection(&m_csPrint);
	DWORD dwWrittenSize;
	if (!m_isOpened) {
		if (m_lastHasError && m_lastSendTime - lastSendTime < 1000) {
			// 短時間で送られ過ぎないようにする
			m_lastSendTime = GetTickCounter();
			goto finalize;
		}
		if (!CCOpenPrinter()) {
			m_lastHasError = true;
			m_lastSendTime = GetTickCounter();
			goto finalize;
		}
		m_lastHasError = false;
	}

	if (!m_isStart) {
		// いきなりEOTは無視
		if (cData == 0x04) {
			ret = 1; // 成功扱い
			goto finalize;
		}

		if (m_lastHasError && !m_isStart && m_lastSendTime - lastSendTime < 1000) {
			// 短時間で送られ過ぎないようにする
			m_lastSendTime = GetTickCounter();
			goto finalize;
		}
		CCStartDocPrinter();
		if (!m_isStart) {
			m_lastHasError = true;
			m_lastSendTime = GetTickCounter();
			goto finalize;
		}
	}

	m_lastHasError = false;

	m_lastSendTime = GetTickCounter();
	if (m_dataCounter < 10000) {
		m_dataCounter++;
	}
	if (0x08 <= cData && cData <= 0x0d || 0x20 <= cData) {
		m_hasValidData = true;
	}
	if (!m_emulation) {
		ret = (::WritePrinter(m_hPrinter, &cData, 1, &dwWrittenSize)) ? 1 : 0;
	}
	else {
		CCWriteESCP(cData);
		ret = 1;
	}

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
		case COMMSG_REOPEN:
			EnterCriticalSection(&m_csPrint);
			if (m_isOpened)
			{
				CCEndThread();
				CCEndDocPrinter();

				::ClosePrinter(m_hPrinter);
				m_hPrinter = NULL;
				m_isOpened = false;
				m_lastHasError = false;
			}

			if (nParam) {
				COMCFG* cfg = (COMCFG*)nParam;
				if (cfg->spoolPrinterName) {
					_tcscpy(m_printerName, cfg->spoolPrinterName);
				}
				m_emulation = cfg->spoolEmulation;
				m_pageTimeout = cfg->spoolTimeout;
				m_escp_dotscale = cfg->spoolDotSize / 100.0;
				m_escp_rectdot = cfg->spoolRectDot ? true : false;
			}

			LeaveCriticalSection(&m_csPrint);
			break;

		default:
			break;
	}
	return 0;
}

// 【ESC/Pエミュレーション】
// PC-PR201系を想定
// 作り始めてから当初予定外の挙動がたくさんあったため、アルゴリズムもデータ構造も最悪水準のコードになっています。
// 将来きれいに作り直しが必要です。
// （駄目ポイントの例）
// ・当初はコマンドに従って随時描画していくつもりだったが、行高さ（ベースライン）が1行が終わるまで確定しないことに気付いて2重で回して誤魔化した
// ・無駄なループや無駄な条件判定が多すぎる

void CComSpooler::CCStartESCPPage(bool reset)
{
	if (reset) {
		m_escpbuf.clear();
		m_escprlin.clear();
		m_escprbuf.clear();
		CCResetESCP();
	}
	m_escp.posy = 0;
	m_escp.renderofsy = 0;

	//if (m_physOffX == 0) {
	//	m_physOffX = m_dpiX / m_escp.cpi;
	//	m_horzRes = m_physWidth - m_physOffX * 2;
	//}
	//if (m_physOffY == 0) {
	//	m_physOffY = m_dpiY / m_escp.lpi;
	//	m_vertRes = m_physHeight - m_physOffY * 2;
	//}
	//m_physOffX += (float)m_dpiX / m_escp.cpi / 24 / 2;
	//m_physOffY += (float)m_dpiX / m_escp.cpi / 24 / 2;
	//m_horzRes -= (float)m_dpiX / m_escp.cpi / 24;
	//m_vertRes -= (float)m_dpiX / m_escp.cpi / 24;

	memset(&m_escp_gdi, 0, sizeof(m_escp_gdi));

	const float dotPitch = 1.0f / 160;
	m_escp_colorbuf_w = (int)ceil(m_physWidth / (m_dpiX * dotPitch));
	m_escp_colorbuf_h = 24;
	m_escp_colorbuf = new UINT8[m_escp_colorbuf_w * m_escp_colorbuf_h];
	memset(m_escp_colorbuf, 0xff, m_escp_colorbuf_w * m_escp_colorbuf_h);

	const int fontPx = MulDiv(12, m_dpiY, 72); // 12pt
	LOGFONTW lf = {0};
	lf.lfHeight = -fontPx;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	lstrcpyW(lf.lfFaceName, L"MS Mincho");
	m_escp_gdi.fontbase = CreateFontIndirectW(&lf);

	lf.lfEscapement = 900;
	lf.lfOrientation = 900;
	m_escp_gdi.fontrot90 = CreateFontIndirectW(&lf);

	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfWeight = FW_BOLD;
	m_escp_gdi.fontbold = CreateFontIndirectW(&lf);
	lf.lfEscapement = 900;
	lf.lfOrientation = 900;
	m_escp_gdi.fontboldrot90 = CreateFontIndirectW(&lf);

	m_escp_gdi.oldfont = nullptr;
	if (m_escp_gdi.fontbase) m_escp_gdi.oldfont = (HFONT)SelectObject(m_hdc, m_escp_gdi.fontbase);

	CCUpdateFont();

	CCUpdateLinePen(m_escpr.linep1, m_escpr.linep2, m_escpr.linep3, m_escpr.linecolor);
	m_escp_gdi.brsDot[0] = (HBRUSH)GetStockObject(BLACK_BRUSH);
	for (int i = 1; i < _countof(m_escp_gdi.brsDot); i++) {
		if (!m_escp_gdi.brsDot[i]) {
			m_escp_gdi.brsDot[i] = CreateSolidBrush(ColorCodeToColorRef(i));
		}
	}

	SetGraphicsMode(m_hdc, GM_ADVANCED);

	SetBkMode(m_hdc, TRANSPARENT);
}
void CComSpooler::CCEndESCPPage()
{
	if (m_escp_gdi.oldfont) SelectObject(m_hdc, m_escp_gdi.oldfont);
	if (m_escp_gdi.fontbase) DeleteObject(m_escp_gdi.fontbase);
	if (m_escp_gdi.fontrot90) DeleteObject(m_escp_gdi.fontrot90);
	if (m_escp_gdi.fontbold) DeleteObject(m_escp_gdi.fontbold);
	if (m_escp_gdi.fontboldrot90) DeleteObject(m_escp_gdi.fontboldrot90);
	if (m_escp_gdi.penline) DeleteObject(m_escp_gdi.penline);
	m_escp_gdi.fontbase = NULL;
	m_escp_gdi.fontrot90 = NULL;
	m_escp_gdi.fontbold = NULL;
	m_escp_gdi.fontboldrot90 = NULL;
	m_escp_gdi.penline = NULL;

	m_escp_gdi.brsDot[0] = NULL;
	for (int i = 1; i < _countof(m_escp_gdi.brsDot); i++) {
		if (m_escp_gdi.brsDot[i]) {
			DeleteObject(m_escp_gdi.brsDot[i]);
			m_escp_gdi.brsDot[i] = NULL;
		}
	}

	if (m_escp_colorbuf) {
		delete[] m_escp_colorbuf;
		m_escp_colorbuf = NULL;
	}
}

void CComSpooler::CCResetESCP()
{
	m_escp.cmd = 0;
	m_escp.cpi = 14;
	m_escp.lpi = 6;
	m_escp.posx = 0;
	m_escp.posy = 0;
	m_escp.renderofsy = 0;
	m_escp.so = false;
	m_escp.deselect = false;
	m_escp.escmode = 0x48;
	m_escp.scalex = 1.0;
	m_escp.scaley = 1.0;
	m_escp.scalex_graph = 1.0;
	m_escp.scaley_graph = 1.0;
	m_escp.maxscaley = 1.0;
	m_escp.maxlineheight = 0;
	m_escp.bold = false;
	m_escp.lineselect = 1;
	m_escp.linep1 = 'S';
	m_escp.linep2 = 1;
	m_escp.linep3 = 2;
	m_escp.lineenable = false;
	m_escp.linecolor = 0;
	m_escp.dotsp_left = 0;
	m_escp.dotsp_right = 0;
	m_escp.color = 0;
	m_escp.hasgraphic = 0;
	m_escp.delaynewpageposy = 0;

	m_escpr = m_escp;

	m_dpiX = GetDeviceCaps(m_hdc, LOGPIXELSX);
	m_dpiY = GetDeviceCaps(m_hdc, LOGPIXELSY);
	m_physWidth = GetDeviceCaps(m_hdc, PHYSICALWIDTH);
	m_physHeight = GetDeviceCaps(m_hdc, PHYSICALHEIGHT);
	m_physOffX = GetDeviceCaps(m_hdc, PHYSICALOFFSETX);
	m_physOffY = GetDeviceCaps(m_hdc, PHYSICALOFFSETY);
	m_horzRes = GetDeviceCaps(m_hdc, HORZRES);
	m_vertRes = GetDeviceCaps(m_hdc, VERTRES);

	SetTextColor(m_hdc, ColorCodeToColorRef(m_escp.color));

	if (m_escp_colorbuf) {
		delete[] m_escp_colorbuf;
		m_escp_colorbuf = NULL;
	}
	const float dotPitch = 1.0f / 160;
	m_escp_colorbuf_w = (int)ceil(m_physWidth / (m_dpiX * dotPitch));
	m_escp_colorbuf_h = 24;
	m_escp_colorbuf = new UINT8[m_escp_colorbuf_w * m_escp_colorbuf_h];
	memset(m_escp_colorbuf, 0xff, m_escp_colorbuf_w * m_escp_colorbuf_h);
}

void CComSpooler::CCUpdateFont()
{
	if (m_escpr.escmode == 0x74) {
		if (m_escp_gdi.fontrot90 && m_escp_gdi.fontboldrot90) SelectObject(m_hdc, m_escpr.bold ? m_escp_gdi.fontboldrot90 : m_escp_gdi.fontrot90);
	}
	else {
		if (m_escp_gdi.fontbase && m_escp_gdi.fontbold) SelectObject(m_hdc, m_escpr.bold ? m_escp_gdi.fontbold : m_escp_gdi.fontbase);
	}
}

void CComSpooler::CCUpdateLinePen(UINT8 param1, UINT8 param2, UINT8 param3, UINT8 color)
{
	if (m_escp_gdi.penline == NULL || m_escpr.linep1 != param1 || m_escpr.linep2 != param2 || m_escpr.linep3 != param3 || m_escpr.linecolor != color) {
		if (m_escp_gdi.penline) {
			DeleteObject(m_escp_gdi.penline);
			m_escp_gdi.penline = NULL;
		}
		const float dotPitch = 1.0f / 160;
		int dotsize = (float)m_dpiX * dotPitch;
		dotsize *= param3 / 2;
		if (dotsize <= 0) dotsize = 1;
		m_escp_gdi.penline = CreatePen(PS_SOLID, dotsize, ColorCodeToColorRef(color));

		m_escpr.linep1 = param1;
		m_escpr.linep2 = param2;
		m_escpr.linep3 = param3;
		m_escpr.linecolor = color;
	}
}

void CComSpooler::CCCheckNewPage(bool delay)
{
	if (m_escp.posy + m_dpiY / m_escp.lpi > m_vertRes) {
		if (delay) {
			if (m_escp.delaynewpageposy == 0) {
				m_escp.delaynewpageposy = m_escp.posy;
			}
		}
		else {
			float posyoffset = m_escp.delaynewpageposy <= 0 ? 0 : (m_escp.posy - m_escp.delaynewpageposy);
			CCRenderAllESCP();
			CCEndESCPPage();
			::EndPage(m_hdc);
			::StartPage(m_hdc);
			CCStartESCPPage(false);
			m_escp.posy = posyoffset;
			m_escp.delaynewpageposy = 0;
			m_escpr = m_escp;
		}
	}
}
void CComSpooler::CCCheckNewLine(int nextCharWidth)
{
	if (m_escp.posx + nextCharWidth > m_horzRes) {
		CCRenderAllESCP();
		m_escp.posx = 0;
		CCWriteLF();
		m_escpr.posx = 0;
	}
}

void CComSpooler::CCWriteLF()
{
	if (m_escp.maxlineheight == 0) {
		m_escp.maxlineheight = max(m_escp.maxlineheight, (float)m_dpiY / m_escp.lpi * m_escp.maxscaley);
	}
	if (m_escp.copymode) {
		float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * m_escpr.scaley_graph;
		m_escp.posy += m_escp.maxlineheight * 120 / 160;// -pitchy * 2;
	}
	else {
		m_escp.posy += m_escp.maxlineheight;
	}
	m_escp.maxlineheight = 0;
	m_escp.maxscaley = 1.0;
	m_escpr = m_escp;
}

void CComSpooler::CCRenderAllESCP()
{
	m_escp.maxlineheight = max(m_escp.maxlineheight, (float)m_dpiY / m_escp.lpi * m_escp.maxscaley);
	//m_escp.posx = 0;
	m_escpr.maxlineheight = m_escp.maxlineheight;
	m_escpr.maxscaley = m_escp.maxscaley;
	m_escpr.lpi = m_escp.lpi;
	m_escpr.cpi = m_escp.cpi;
	m_escpr.hasgraphic = 0;
	//m_escpr.posx = 0;
	for (auto it = m_escprlin.begin(); it != m_escprlin.end(); ++it) {
		CCRenderESCP(*it);
	}
	if (m_escpr.hasgraphic) {
		if (m_escp.posy + m_dpiY / m_escp.lpi > m_vertRes) {
			m_escpr.hasgraphic = 0; // check
		}
		const float dotPitch = 1.0f / 160;
		float pitchx = (float)m_dpiX * dotPitch * m_escpr.scalex_graph;
		float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * m_escpr.scaley_graph;
		if (m_escp.copymode) {
			pitchy *= (float)24 / 16 * 120 / 160 * 160 / 24 / 6; // なぞのほせい
		}
		int r = (float)m_dpiX * dotPitch / 2 * m_escp_dotscale;
		int rx = (float)pitchx / 2 + 1;
		int ry = (float)pitchy / 2 + 1;
		if (r == 0) r = 1;
		HBRUSH hBrush = m_escp_gdi.brsDot[m_escpr.color];
		HPEN hPen = (HPEN)GetStockObject(NULL_PEN);
		HGDIOBJ oldPen = SelectObject(m_hdc, hPen);
		HGDIOBJ oldBrush = SelectObject(m_hdc, hBrush);
		int curColor = m_escpr.color;
		for (int y = 0; y < m_escp_colorbuf_h; y++) {
			int cy = m_physOffY + (int)(m_escpr.posy + y * pitchy);
			for (int x = 0; x < m_escp_colorbuf_w; x++) {
				int cx = m_physOffX + (int)(x * pitchx);
				int idx = y * m_escp_colorbuf_w + x;
				if ((m_escp_colorbuf[idx] & 0x7) != 0x7) {
					if ((m_escp_colorbuf[idx] & 0x7) != curColor) {
						curColor = (m_escp_colorbuf[idx] & 0x7);
						SelectObject(m_hdc, m_escp_gdi.brsDot[curColor]);
					}
					if (m_escp_rectdot) {
						Rectangle(m_hdc, cx - rx, cy - ry, cx + rx, cy + ry);
					}
					else {
						Ellipse(m_hdc, cx - r, cy - r, cx + r, cy + r);
					}
				}
			}
		}
		SelectObject(m_hdc, oldBrush);
		SelectObject(m_hdc, oldPen);
		memset(m_escp_colorbuf, 0xff, m_escp_colorbuf_w * m_escp_colorbuf_h);
	}
	m_escprlin.clear();
	m_escpr = m_escp;
}
void CComSpooler::CCRenderESCP(UINT8 cData)
{
	m_escprbuf.push_back(cData);

	const float dotPitch = 1.0f / 160;

	UINT8 cmd = m_escprbuf.at(0);
	if (cmd == 0x1d) {
		m_escpr.setVFUmode = true;
	}
	else if (cmd == 0x1e) {
		m_escpr.setVFUmode = false;
	}
	if (m_escpr.setVFUmode) {
		m_escprbuf.erase(m_escprbuf.begin());
		return;
	}
	switch (cmd) {
	case 0x0d: // CR
		m_escprbuf.erase(m_escprbuf.begin());
		m_escpr.posx = 0;
		break;

	case 0x0a: // LF
		m_escprbuf.erase(m_escprbuf.begin());
		break;

	case 0x09: // HT
		m_escprbuf.erase(m_escprbuf.begin());
		break;

	case 0x0b: // VT
	{
		float vt = (float)6 * m_dpiY / m_escpr.lpi;
		m_escprbuf.erase(m_escprbuf.begin());
		m_escpr.posy = floor((m_escpr.posy + (float)vt) / vt) * vt;
		m_escpr.maxlineheight = max(m_escpr.maxlineheight, (float)m_dpiY / m_escpr.lpi * m_escpr.scaley);
		break;
	}

	case 0x0c: // FF
		m_escprbuf.erase(m_escprbuf.begin());
		break;

	case 0x0e: // SO
		m_escprbuf.erase(m_escprbuf.begin());
		m_escpr.so = true;
		break;

	case 0x0f: // SI
		m_escprbuf.erase(m_escprbuf.begin());
		m_escpr.so = false;
		break;

	case 0x18: // CAN
		m_escprbuf.erase(m_escprbuf.begin());
		break;

	case 0x11: // DC1
		m_escprbuf.erase(m_escprbuf.begin());
		m_escpr.deselect = false;
		break;

	case 0x13: // DC3
		m_escprbuf.erase(m_escprbuf.begin());
		m_escpr.deselect = true;
		break;

	case 0x1f: // US
	{
		if (m_escprbuf.size() < 2) return;
		m_escprbuf.erase(m_escprbuf.begin());
		UINT8 param = m_escprbuf.at(0);
		m_escprbuf.erase(m_escprbuf.begin());

		if (param < 16) {
			// 未実装
		}
		else {
			m_escpr.posx = 0;
			m_escpr.posy += (param - 16) * (float)m_dpiY / m_escpr.lpi;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, (float)m_dpiY / m_escpr.lpi * m_escpr.scaley);
		}

		break;
	}

	case 0x1b: // ESC
	{
		if (m_escprbuf.size() < 2) return;
		UINT8 param = m_escprbuf.at(1);
		switch (param) {
		case 0x65:
		{
			if (m_escprbuf.size() < 4) return;
			m_escpr.scalex = m_escprbuf.at(2) - '0';
			m_escpr.scaley = m_escprbuf.at(3) - '0';
			if (m_escpr.scalex < 1) m_escpr.scalex = 1;
			if (m_escpr.scaley < 1) m_escpr.scaley = 1;
			if (m_escpr.scalex > 256) m_escpr.scalex = 256;
			if (m_escpr.scaley > 256) m_escpr.scaley = 256;
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x52:
		{
			bool isKanji = (param == 0x4b || param == 0x74);
			if (m_escprbuf.size() < 6 + (isKanji ? 1 : 0)) return;
			int repCount = (m_escprbuf.at(2) - '0') * 100 + (m_escprbuf.at(3) - '0') * 10 + (m_escprbuf.at(4) - '0');
			UINT8 repData[2];
			repData[0] = m_escprbuf.at(5);
			if (isKanji) {
				repData[1] = m_escprbuf.at(6);
				m_escprbuf.erase(m_escprbuf.begin());
			}
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			// 以下は解析時に登録済み
			//if (isKanji) {
			//	for (int i = 0; i < repCount; i++) {
			//		CCWriteESCP(repData[0]);
			//		CCWriteESCP(repData[1]);
			//	}
			//}
			//else {
			//	for (int i = 0; i < repCount; i++) {
			//		CCWriteESCP(repData[0]);
			//	}
			//}
			break;
		}
		case 0x21:
		{
			if (!m_escpr.bold) {
				m_escpr.bold = true;
				CCUpdateFont();
			}
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x22:
		{
			if (m_escpr.bold) {
				m_escpr.bold = false;
				CCUpdateFont();
			}
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x5f:
		{
			if (m_escprbuf.size() < 3) return;
			m_escpr.lineselect = m_escprbuf.at(2) - '0';
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x58:
		{
			if (!m_escpr.lineenable) {
				m_escpr.lineenable = true;
			}
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x59:
		{
			if (m_escpr.lineenable) {
				m_escpr.lineenable = false;
			}
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x64:
		{
			if (m_escprbuf.size() < 3) return;
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		case 0x46:
		{
			if (m_escprbuf.size() < 6) return;
			int dotX = (m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0');
			float newX = dotX * m_dpiX / 160;
			if (m_escpr.posx <= newX && newX < m_horzRes) {
				m_escpr.posx = newX;
			}
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6);
			break;
		}
		case 0x76:
		{
			if (m_escprbuf.back() != '.') return;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.end());
			break;
		}
		case 0x4c:
		{
			if (m_escprbuf.size() < 5) return;
			const float charWidth = (float)m_dpiX / 56;
			const int leftMargin = (m_escprbuf.at(2) - '0') * 100 + (m_escprbuf.at(3) - '0') * 10 + (m_escprbuf.at(4) - '0');
			const int leftMarginPixel = leftMargin * charWidth;
			const int oldRightMarginPixel = m_physWidth - m_horzRes - m_physOffX;
			m_physOffX = leftMarginPixel;
			m_horzRes = m_physWidth - leftMarginPixel - oldRightMarginPixel;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 5);
			break;
		}
		case 0x2f:
		{
			if (m_escprbuf.size() < 5) return;
			const float charWidth = (float)m_dpiX / 56;
			const int rightMarginPos = (m_escprbuf.at(2) - '0') * 100 + (m_escprbuf.at(3) - '0') * 10 + (m_escprbuf.at(4) - '0');
			const int rightMarginPixel = m_physWidth - rightMarginPos * charWidth;
			m_horzRes = rightMarginPixel - m_physOffX;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 5);
			break;
		}
		case 0x6e:
		case 0x73:
		case 0x6c:
		case 0x68:
		{
			if (m_escprbuf.size() < 3) return;
			UINT8 param2 = m_escprbuf.at(2);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 3);
			break;
		}
		case 0x63:
		{
			if (m_escprbuf.size() < 3) return;
			UINT8 param2 = m_escprbuf.at(2);
			if (param2 == 0x31) {
				CCResetESCP();
			}
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 3);
			break;
		}
		case 0x53:
		{
			// 8ドット グラフィック印字
			if (m_escprbuf.size() < 6) return;
			if (m_escprbuf.at(2) < '0' || '9' < m_escprbuf.at(2) ||
				m_escprbuf.at(3) < '0' || '9' < m_escprbuf.at(3) ||
				m_escprbuf.at(4) < '0' || '9' < m_escprbuf.at(4) ||
				m_escprbuf.at(5) < '0' || '9' < m_escprbuf.at(5)) {
				m_escprbuf.clear();
				break;
			}
			int gw = (m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0');
			if (m_escprbuf.size() < 6 + gw) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			int posx = (int)floor(m_escpr.posx / (m_dpiX * dotPitch) + 0.5);
			int extstep = (m_escp.so ? 2.0f : 1.0f);
			for (int i = 0; i < gw; i++) {
				int cx = posx + i * extstep;
				if (cx + extstep - 1 >= m_escp_colorbuf_w) break;
				UINT8 data = m_escprbuf.at(6 + i);
				for (int cy = 0; cy < 16; cy += 2) {
					if (data & 1) {
						int idx = cy * m_escp_colorbuf_w + cx;
						m_escp_colorbuf[idx] &= m_escpr.color;
						if (extstep == 2) {
							m_escp_colorbuf[idx + 1] &= m_escpr.color;
						}
					}
					data >>= 1;
				}
			}
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			m_escpr.posx += gw * pitchx;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6 + gw);
			break;
		}
		case 0x49:
		{
			// 16ドット グラフィック印字
			if (m_escprbuf.size() < 6) return;
			if (m_escprbuf.at(2) < '0' || '9' < m_escprbuf.at(2) ||
				m_escprbuf.at(3) < '0' || '9' < m_escprbuf.at(3) ||
				m_escprbuf.at(4) < '0' || '9' < m_escprbuf.at(4) ||
				m_escprbuf.at(5) < '0' || '9' < m_escprbuf.at(5)) {
				m_escprbuf.clear();
				break;
			}
			int gw = ((m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0')) * 2;
			if (m_escprbuf.size() < 6 + gw) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			int posx = (int)floor(m_escpr.posx / (m_dpiX * dotPitch) + 0.5);
			int extstep = (m_escp.so ? 2.0f : 1.0f);
			for (int i = 0; i < gw; i+= 2) {
				int cx = posx + i / 2 * extstep;
				if (cx + extstep - 1 >= m_escp_colorbuf_w) break;
				UINT16 data = ((UINT16)m_escprbuf.at(6 + i + 1) << 8) | m_escprbuf.at(6 + i);
				for (int cy = 0; cy < 16; cy++) {
					if (data & 1) {
						int idx = cy * m_escp_colorbuf_w + cx;
						m_escp_colorbuf[idx] &= m_escpr.color;
						if (extstep == 2) {
							m_escp_colorbuf[idx + 1] &= m_escpr.color;
						}
					}
					data >>= 1;
				}
			}
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			m_escpr.posx += gw * pitchx / 2;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6 + gw);
			break;
		}
		case 0x4a:
		{
			// 24ドット グラフィック印字
			if (m_escprbuf.size() < 6) return;
			if (m_escprbuf.at(2) < '0' || '9' < m_escprbuf.at(2) ||
				m_escprbuf.at(3) < '0' || '9' < m_escprbuf.at(3) ||
				m_escprbuf.at(4) < '0' || '9' < m_escprbuf.at(4) ||
				m_escprbuf.at(5) < '0' || '9' < m_escprbuf.at(5)) {
				m_escprbuf.clear();
				break;
			}
			int gw = ((m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0')) * 3;
			if (m_escprbuf.size() < 6 + gw) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			int posx = (int)floor(m_escpr.posx / (m_dpiX * dotPitch) + 0.5);
			int extstep = (m_escp.so ? 2.0f : 1.0f);
			for (int i = 0; i < gw; i += 3) {
				int cx = posx + i / 3 * extstep;
				if (cx + extstep - 1 >= m_escp_colorbuf_w) break;
				UINT32 data = ((UINT32)m_escprbuf.at(6 + i + 2) << 16) | ((UINT32)m_escprbuf.at(6 + i + 1) << 8) | m_escprbuf.at(6 + i);
				for (int cy = 0; cy < 24; cy++) {
					if (data & 1) {
						int idx = cy * m_escp_colorbuf_w + cx;
						m_escp_colorbuf[idx] &= m_escpr.color;
						if (extstep == 2) {
							m_escp_colorbuf[idx + 1] &= m_escpr.color;
						}
					}
					data >>= 1;
				}
			}
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			m_escpr.posx += gw * pitchx / 3;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6 + gw);
			break;
		}
		case 0x56:
		{
			// 8ドット グラフィック印字 リピート
			if (m_escprbuf.size() < 6) return;
			if (m_escprbuf.at(2) < '0' || '9' < m_escprbuf.at(2) ||
				m_escprbuf.at(3) < '0' || '9' < m_escprbuf.at(3) ||
				m_escprbuf.at(4) < '0' || '9' < m_escprbuf.at(4) ||
				m_escprbuf.at(5) < '0' || '9' < m_escprbuf.at(5)) {
				m_escprbuf.clear();
				break;
			}
			int gw = (m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0');
			if (m_escprbuf.size() < 6 + 1) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			UINT8 odata = m_escprbuf.at(6);
			int posx = (int)floor(m_escpr.posx / (m_dpiX * dotPitch) + 0.5);
			int extstep = (m_escp.so ? 2.0f : 1.0f);
			for (int i = 0; i < gw; i++) {
				int cx = posx + i * extstep;
				if (cx + extstep - 1 >= m_escp_colorbuf_w) break;
				UINT8 data = odata;
				for (int cy = 0; cy < 16; cy += 2) {
					if (data & 1) {
						int idx = cy * m_escp_colorbuf_w + cx;
						m_escp_colorbuf[idx] &= m_escpr.color;
						if (extstep == 2) {
							m_escp_colorbuf[idx + 1] &= m_escpr.color;
						}
					}
					data >>= 1;
				}
			}
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			m_escpr.posx += gw * pitchx;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6 + 1);
			break;
		}
		case 0x57:
		{
			// 16ドット グラフィック印字 リピート
			if (m_escprbuf.size() < 6) return;
			if (m_escprbuf.at(2) < '0' || '9' < m_escprbuf.at(2) || 
				m_escprbuf.at(3) < '0' || '9' < m_escprbuf.at(3) ||
				m_escprbuf.at(4) < '0' || '9' < m_escprbuf.at(4) ||
				m_escprbuf.at(5) < '0' || '9' < m_escprbuf.at(5)) {
				m_escprbuf.clear();
				break;
			}
			int gw = ((m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0')) * 2;
			if (m_escprbuf.size() < 6 + 2) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			UINT16 odata = ((UINT16)m_escprbuf.at(6 + 1) << 8) | m_escprbuf.at(6);
			int posx = (int)floor(m_escpr.posx / (m_dpiX * dotPitch) + 0.5);
			int extstep = (m_escp.so ? 2.0f : 1.0f);
			for (int i = 0; i < gw; i += 2) {
				int cx = posx + i / 2 * extstep;
				if (cx + extstep - 1 >= m_escp_colorbuf_w) break;
				UINT16 data = odata;
				for (int cy = 0; cy < 16; cy++) {
					if (data & 1) {
						int idx = cy * m_escp_colorbuf_w + cx;
						m_escp_colorbuf[idx] &= m_escpr.color;
						if (extstep == 2) {
							m_escp_colorbuf[idx + 1] &= m_escpr.color;
						}
					}
					data >>= 1;
				}
			}
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			m_escpr.posx += gw * pitchx / 2;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6 + 2);
			break;
		}
		case 0x55:
		{
			// 24ドット グラフィック印字 リピート
			if (m_escprbuf.size() < 6) return;
			if (m_escprbuf.at(2) < '0' || '9' < m_escprbuf.at(2) ||
				m_escprbuf.at(3) < '0' || '9' < m_escprbuf.at(3) ||
				m_escprbuf.at(4) < '0' || '9' < m_escprbuf.at(4) ||
				m_escprbuf.at(5) < '0' || '9' < m_escprbuf.at(5)) {
				m_escprbuf.clear();
				break;
			}
			int gw = ((m_escprbuf.at(2) - '0') * 1000 + (m_escprbuf.at(3) - '0') * 100 + (m_escprbuf.at(4) - '0') * 10 + (m_escprbuf.at(5) - '0')) * 3;
			if (m_escprbuf.size() < 6 + 3) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			UINT32 odata = ((UINT32)m_escprbuf.at(6 + 2) << 16) | ((UINT32)m_escprbuf.at(6 + 1) << 8) | m_escprbuf.at(6);
			int posx = (int)floor(m_escpr.posx / (m_dpiX * dotPitch) + 0.5);
			int extstep = (m_escp.so ? 2.0f : 1.0f);
			for (int i = 0; i < gw; i += 3) {
				int cx = posx + i / 3 * extstep;
				if (cx + extstep - 1 >= m_escp_colorbuf_w) break;
				UINT32 data = odata;
				for (int cy = 0; cy < 24; cy++) {
					if (data & 1) {
						int idx = cy * m_escp_colorbuf_w + cx;
						m_escp_colorbuf[idx] &= m_escpr.color;
						if (extstep == 2) {
							m_escp_colorbuf[idx + 1] &= m_escpr.color;
						}
					}
					data >>= 1;
				}
			}
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			m_escpr.posx += gw * pitchx / 3;
			m_escpr.maxlineheight = max(m_escpr.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6 + 3);
			break;
		}
		case 0x41:
		{
			m_escpr.lpi = 6; // 1/6
			m_escpr.maxlineheight = 0; // 強制リセット
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 2);
			break;
		}
		case 0x42:
		{
			m_escpr.lpi = 8; // 1/8
			m_escpr.maxlineheight = 0; // 強制リセット
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 2);
			break;
		}
		case 0x54:
		{
			if (m_escprbuf.size() < 4) return;
			int value = (m_escprbuf.at(2) - '0') * 10 + (m_escprbuf.at(3) - '0');
			m_escpr.lpi = 120.0f / value; // n/240
			m_escpr.maxlineheight = 0; // 強制リセット
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 4);
			break;
		}
		case 0x4f:
		{
			if (m_escprbuf.size() < 3) return;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 3);
			break;
		}
		case 0x44:
		{
			m_escpr.copymode = true;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 2);
			break;
		}
		case 0x4d:
		{
			m_escpr.copymode = false;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 2);
			break;
		}
		case 0x43:
		{
			if (m_escprbuf.size() < 3) return;
			m_escpr.color = (m_escprbuf.at(2) - '0') & 0x7;
			CCUpdateLinePen(m_escpr.linep1, m_escpr.linep2, m_escpr.linep3, m_escprbuf.at(2) - '0');
			SetTextColor(m_hdc, ColorCodeToColorRef(m_escprbuf.at(2) - '0'));
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 3);
			break;
		}
		default:
		{
			if (param == 0x4e ||
				param == 0x48 ||
				param == 0x51 ||
				param == 0x45 ||
				param == 0x50 ||
				param == 0x4b ||
				param == 0x74) {
				if (param != m_escpr.escmode) {
					m_escpr.escmode = param;
					CCUpdateFont();
				}
			}
			else if (param <= 0x08) {
				float pitchx = (float)m_dpiX * dotPitch;
				m_escpr.posx += pitchx * param;
			}
			m_escprbuf.erase(m_escprbuf.begin());
			m_escprbuf.erase(m_escprbuf.begin());
			break;
		}
		}
		break;
	}

	case 0x1c: // FS
	{
		if (m_escprbuf.size() < 2) return;
		UINT8 param = m_escprbuf.at(1);
		switch (param) {
		case 0x30:
		{
			if (m_escprbuf.size() < 4) return;
			UINT8 param2 = m_escprbuf.at(2);
			UINT8 param3 = m_escprbuf.at(3);
			if (param2 = 0x34 && (param3 == 0x3c || param3 == 0x53)) {
				if (m_escprbuf.size() < 7) return;
				m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 7);
			}
			else if (param2 = 0x36 && param3 == 0x46) {
				if (m_escprbuf.size() < 9) return;
				m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 9);
			}
			else if (param2 = 0x34 && param3 == 0x4c) {
				if (m_escprbuf.size() < 7) return;
				CCUpdateLinePen(m_escprbuf.at(4), m_escprbuf.at(5) - '0', m_escprbuf.at(6) - '0', m_escpr.linecolor);
				m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 7);
			}
			else {
				m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 4);
			}
			break;
		}
		case 0x77:
		{
			if (m_escprbuf.size() < 6) return;
			m_escpr.dotsp_left = m_escprbuf.at(2) - '0';
			m_escpr.dotsp_right = m_escprbuf.at(4) - '0';
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6);
			break;
		}
		case 0x70:
		{
			if (m_escprbuf.size() < 8) return;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 8);
			break;
		}
		case 0x66:
		{
			if (m_escprbuf.size() < 6) return;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 6);
			break;
		}
		case 0x63:
		{
			if (m_escprbuf.back() != '.') return;
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.end());
			break;
		}
		default:
		{
			m_escprbuf.erase(m_escprbuf.begin(), m_escprbuf.begin() + 2);
			break;
		}
		}
		break;
	}

	default:
	{
		bool isKanji = (m_escpr.escmode == 0x4b || m_escpr.escmode == 0x74);
		if (isKanji) {
			if (m_escprbuf.size() < 2) return;
		}
		if (0x20 <= cmd || (isKanji && cmd == 0 && 0x20 <= m_escprbuf.at(1))) {
			float charWidth = (float)m_dpiX / m_escpr.cpi * (m_escpr.so ? 2 : 1);
			TCHAR buf[3] = { 9 };
			UINT8 th[3] = { 0 };
			if (isKanji) {
				if (cmd == 0) {
					// 実質1バイト文字
					th[0] = m_escprbuf.at(1);
				}
				else {
					UINT16 sjis = jis_to_sjis(cmd << 8 | m_escprbuf.at(1));
					th[0] = sjis >> 8;
					th[1] = sjis & 0xff;
					charWidth *= 2;
				}
				m_escprbuf.erase(m_escprbuf.begin());
			}
			else {
				th[0] = cmd;
			}
			UINT16 thw[2];
			codecnv_sjistoucs2(thw, 1, (const char*)th, 2);
			buf[0] = (TCHAR)thw[0];
			buf[1] = (TCHAR)thw[1];
			m_escprbuf.erase(m_escprbuf.begin());

			float pitchx = (float)m_dpiX * dotPitch;
			m_escpr.posx += m_escpr.dotsp_left * pitchx;

			if (!m_escpr.deselect) {
				int yOfsEx = (m_escpr.escmode == 0x74) ? (m_dpiY / m_escpr.lpi) : 0;
				float base_escp_posy = m_escpr.posy + m_escpr.maxlineheight - m_dpiY / m_escpr.lpi * m_escpr.scaley;
				if (m_escpr.so || m_escpr.escmode == 0x45 || m_escpr.escmode == 0x50 || m_escpr.escmode == 0x51 || m_escpr.scalex != 1 || m_escpr.scaley != 1) {
					XFORM xf = {0};
					xf.eM11 = (m_escpr.so ? 2.0f : 1.0f) * m_escpr.scalex;  // X倍率
					xf.eM22 = 1.0f * m_escpr.scaley;  // Y倍率
					xf.eM12 = xf.eM21 = 0.0f;
					xf.eDx = 0.0f;
					xf.eDy = 0.0f;

					if (m_escpr.escmode == 0x51) { // コンデンス
						xf.eM11 *= 0.6;
						charWidth *= 0.6;
					}
					else if (m_escpr.escmode == 0x45) { // エリート
						xf.eM11 *= 0.8;
						charWidth *= 0.8;
					}
					else if (m_escp.escmode == 0x50) { // プロポーショナル XXX; 本当は字の幅が可変
						xf.eM11 *= 0.9;
						charWidth *= 0.9;
					}

					charWidth *= m_escpr.scalex;

					SetWorldTransform(m_hdc, &xf);
					TextOut(m_hdc, (m_physOffX + (int)m_escpr.posx) / xf.eM11, (m_physOffY + (int)base_escp_posy + yOfsEx) / xf.eM22, buf, 1);

					ModifyWorldTransform(m_hdc, nullptr, MWT_IDENTITY);
				}
				else {
					TextOut(m_hdc, m_physOffX + (int)m_escpr.posx, m_physOffY + (int)base_escp_posy + yOfsEx, buf, 1);
				}
				if (m_escpr.lineenable) {
					float lineBeginX = m_escpr.posx - m_escpr.dotsp_left * pitchx;
					float lineEndX = m_escpr.posx + charWidth + m_escpr.dotsp_right * pitchx;
					const float dotPitch = 1.0f / 160;
					int dotsize = (float)m_dpiX * dotPitch;
					dotsize *= m_escpr.linep3 / 2;
					HPEN hOldPen = (HPEN)SelectObject(m_hdc, m_escp_gdi.penline);
					int ypos = 0;
					if (m_escpr.lineselect == 1) {
						// 下線
						ypos = m_physOffY + base_escp_posy + m_dpiY / m_escpr.lpi - dotsize / 2;
					}
					else if (m_escpr.lineselect == 2) {
						// 上線
						ypos = m_physOffY + base_escp_posy + dotsize / 2;
					}
					MoveToEx(m_hdc, m_physOffX + lineBeginX, ypos, NULL);
					LineTo(m_hdc, m_physOffX + lineEndX, ypos);
					SelectObject(m_hdc, hOldPen);
				}
			}
			m_escpr.posx += charWidth;
			m_escpr.posx += m_escpr.dotsp_right * pitchx;
		}
		else {
			m_escprbuf.erase(m_escprbuf.begin());
			if (isKanji) {
				m_escprbuf.erase(m_escprbuf.begin());
			}
		}
	}
	}
}

void CComSpooler::CCWriteESCP(UINT8 cData)
{
	m_escpbuf.push_back(cData);
	m_escprlin.push_back(cData);
	
	float dotPitch = 1.0f / 160;

	UINT8 cmd = m_escpbuf.at(0);
	if (cmd == 0x1d) {
		m_escp.setVFUmode = true;
	}
	else if (cmd == 0x1e) {
		m_escp.setVFUmode = false;
	}
	if (m_escp.setVFUmode) {
		m_escpbuf.erase(m_escpbuf.begin());
		return;
	}
	switch (cmd) {
	case 0x0d: // CR
		m_escpbuf.erase(m_escpbuf.begin());
		m_escp.posx = 0;
		break;

	case 0x0a: // LF
		m_escpbuf.erase(m_escpbuf.begin());
		CCRenderAllESCP();
		CCWriteLF();
		break;

	case 0x09: // HT
		m_escpbuf.erase(m_escpbuf.begin());
		break;

	case 0x0b: // VT
	{
		float vt = (float)6 * m_dpiY / m_escp.lpi;
		m_escpbuf.erase(m_escpbuf.begin());
		CCRenderAllESCP();
		m_escp.posy = floor((m_escp.posy + (float)vt) / vt) * vt;
		break;
	}

	case 0x0c: // FF
		m_escpbuf.erase(m_escpbuf.begin());
		CCRenderAllESCP();
		m_escp.posy = m_vertRes + m_dpiY / m_escp.lpi;
		break;

	case 0x0e: // SO
		m_escpbuf.erase(m_escpbuf.begin());
		m_escp.so = true;
		break;

	case 0x0f: // SI
		m_escpbuf.erase(m_escpbuf.begin());
		m_escp.so = false;
		break;

	case 0x18: // CAN
		m_escpbuf.erase(m_escpbuf.begin());
		break;

	case 0x11: // DC1
		m_escpbuf.erase(m_escpbuf.begin());
		m_escp.deselect = false;
		break;

	case 0x13: // DC3
		m_escpbuf.erase(m_escpbuf.begin());
		m_escp.deselect = true;
		break;

	case 0x1f: // US
	{
		if (m_escpbuf.size() < 2) return;
		m_escpbuf.erase(m_escpbuf.begin());
		UINT8 param = m_escpbuf.at(0);
		m_escpbuf.erase(m_escpbuf.begin());

		if (param < 16) {
			// 未実装
		}
		else {
			m_escp.posx = 0;
			CCRenderAllESCP();
			m_escp.posy += (param - 16) * (float)m_dpiY / m_escp.lpi;
		}

		break;
	}

	case 0x1b: // ESC
	{
		if (m_escpbuf.size() < 2) return;
		UINT8 param = m_escpbuf.at(1);
		switch (param) {
		case 0x65:
		{
			if (m_escpbuf.size() < 4) return;
			m_escp.scalex = m_escpbuf.at(2) - '0';
			m_escp.scaley = m_escpbuf.at(3) - '0';
			if (m_escp.scalex < 1) m_escp.scalex = 1;
			if (m_escp.scaley < 1) m_escp.scaley = 1;
			if (m_escp.scalex > 256) m_escp.scalex = 256;
			if (m_escp.scaley > 256) m_escp.scaley = 256;
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x52:
		{
			bool isKanji = (param == 0x4b || param == 0x74);
			if (m_escpbuf.size() < 6 + (isKanji ? 1 : 0)) return;
			int repCount = (m_escpbuf.at(2) - '0') * 100 + (m_escpbuf.at(3) - '0') * 10 + (m_escpbuf.at(4) - '0');
			UINT8 repData[2];
			repData[0] = m_escpbuf.at(5);
			if (isKanji) {
				repData[1] = m_escpbuf.at(6);
				m_escpbuf.erase(m_escpbuf.begin());
			}
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			if (isKanji) {
				for (int i = 0; i < repCount; i++) {
					CCWriteESCP(repData[0]);
					CCWriteESCP(repData[1]);
				}
			}
			else {
				for (int i = 0; i < repCount; i++) {
					CCWriteESCP(repData[0]);
				}
			}
			break;
		}
		case 0x21:
		{
			if (!m_escp.bold) {
				m_escp.bold = true;
				CCUpdateFont();
			}
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x22:
		{
			if (m_escp.bold) {
				m_escp.bold = false;
				CCUpdateFont();
			}
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x5f:
		{
			if (m_escpbuf.size() < 3) return;
			m_escp.lineselect = m_escpbuf.at(2) - '0';
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x58:
		{
			if (!m_escp.lineenable) {
				m_escp.lineenable = true;
			}
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x59:
		{
			if (m_escp.lineenable) {
				m_escp.lineenable = false;
			}
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x64:
		{
			if (m_escpbuf.size() < 3) return;
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		case 0x46:
		{
			if (m_escpbuf.size() < 6) return;
			int dotX = (m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0');
			float newX = dotX * m_dpiX / 160;
			if (m_escp.posx <= newX && newX < m_horzRes) {
				m_escp.posx = newX;
			}
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6);
			break;
		}
		case 0x76:
		{
			if (m_escpbuf.back() != '.') return;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.end());
			break;
		}
		case 0x4c:
		{
			if (m_escpbuf.size() < 5) return;
			const float charWidth = (float)m_dpiX / 56;
			const int leftMargin = (m_escpbuf.at(2) - '0') * 100 + (m_escpbuf.at(3) - '0') * 10 + (m_escpbuf.at(4) - '0');
			const int leftMarginPixel = leftMargin * charWidth;
			const int oldRightMarginPixel = m_physWidth - m_horzRes - m_physOffX;
			m_physOffX = leftMarginPixel;
			m_horzRes = m_physWidth - leftMarginPixel - oldRightMarginPixel;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 5);
			break;
		}
		case 0x2f:
		{
			if (m_escpbuf.size() < 5) return;
			const float charWidth = (float)m_dpiX / 56;
			const int rightMarginPos = (m_escpbuf.at(2) - '0') * 100 + (m_escpbuf.at(3) - '0') * 10 + (m_escpbuf.at(4) - '0');
			const int rightMarginPixel = m_physWidth - rightMarginPos * charWidth;
			m_horzRes = rightMarginPixel - m_physOffX;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 5);
			break;
		}
		case 0x6e:
		case 0x73:
		case 0x6c:
		case 0x68:
		{
			if (m_escpbuf.size() < 3) return;
			UINT8 param2 = m_escpbuf.at(2);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 3);
			break;
		}
		case 0x63:
		{
			if (m_escpbuf.size() < 3) return;
			UINT8 param2 = m_escpbuf.at(2);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 3);
			break;
		}
		case 0x53:
		{
			// 8ドット グラフィック印字
			CCCheckNewPage();
			if (m_escpbuf.size() < 6) return;
			if (m_escpbuf.at(2) < '0' || '9' < m_escpbuf.at(2) ||
				m_escpbuf.at(3) < '0' || '9' < m_escpbuf.at(3) ||
				m_escpbuf.at(4) < '0' || '9' < m_escpbuf.at(4) ||
				m_escpbuf.at(5) < '0' || '9' < m_escpbuf.at(5)) {
				m_escpbuf.clear();
				break;
			}
			int gw = (m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0');
			if (m_escpbuf.size() < 6 + gw) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * 2 * m_escp.scaley;
			m_escp.posx += gw * pitchx;
			m_escp.maxlineheight = max(m_escp.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6 + gw);
			m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			break;
		}
		case 0x49:
		{
			// 16ドット グラフィック印字
			CCCheckNewPage();
			if (m_escpbuf.size() < 6) return;
			if (m_escpbuf.at(2) < '0' || '9' < m_escpbuf.at(2) ||
				m_escpbuf.at(3) < '0' || '9' < m_escpbuf.at(3) ||
				m_escpbuf.at(4) < '0' || '9' < m_escpbuf.at(4) ||
				m_escpbuf.at(5) < '0' || '9' < m_escpbuf.at(5)) {
				m_escpbuf.clear();
				break;
			}
			int gw = ((m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0')) * 2;
			if (m_escpbuf.size() < 6 + gw) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * m_escp.scaley;
			m_escp.posx += gw * pitchx / 2;
			m_escp.maxlineheight = max(m_escp.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6 + gw);
			m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			break;
		}
		case 0x4a:
		{
			// 24ドット グラフィック印字
			CCCheckNewPage();
			if (m_escpbuf.size() < 6) return;
			if (m_escpbuf.at(2) < '0' || '9' < m_escpbuf.at(2) ||
				m_escpbuf.at(3) < '0' || '9' < m_escpbuf.at(3) ||
				m_escpbuf.at(4) < '0' || '9' < m_escpbuf.at(4) ||
				m_escpbuf.at(5) < '0' || '9' < m_escpbuf.at(5)) {
				m_escpbuf.clear();
				break;
			}
			int gw = ((m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0')) * 3;
			if (m_escpbuf.size() < 6 + gw) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * m_escp.scaley;
			m_escp.posx += gw * pitchx / 3;
			m_escp.maxlineheight = max(m_escp.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6 + gw);
			m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			break;
		}
		case 0x56:
		{
			// 8ドット グラフィック印字 リピート
			CCCheckNewPage();
			if (m_escpbuf.size() < 6) return;
			if (m_escpbuf.at(2) < '0' || '9' < m_escpbuf.at(2) ||
				m_escpbuf.at(3) < '0' || '9' < m_escpbuf.at(3) ||
				m_escpbuf.at(4) < '0' || '9' < m_escpbuf.at(4) ||
				m_escpbuf.at(5) < '0' || '9' < m_escpbuf.at(5)) {
				m_escpbuf.clear();
				break;
			}
			int gw = (m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0');
			if (m_escpbuf.size() < 6 + 1) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * 2 * m_escp.scaley;
			m_escp.posx += gw * pitchx;
			m_escp.maxlineheight = max(m_escp.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6 + 1);
			m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			break;
		}
		case 0x57:
		{
			// 16ドット グラフィック印字 リピート
			CCCheckNewPage();
			if (m_escpbuf.size() < 6) return;
			if (m_escpbuf.at(2) < '0' || '9' < m_escpbuf.at(2) ||
				m_escpbuf.at(3) < '0' || '9' < m_escpbuf.at(3) ||
				m_escpbuf.at(4) < '0' || '9' < m_escpbuf.at(4) ||
				m_escpbuf.at(5) < '0' || '9' < m_escpbuf.at(5)) {
				m_escpbuf.clear();
				break;
			}
			int gw = ((m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0')) * 2;
			if (m_escpbuf.size() < 6 + 2) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * m_escp.scaley;
			m_escp.posx += gw * pitchx / 2;
			m_escp.maxlineheight = max(m_escp.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6 + 2);
			m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			break;
		}
		case 0x55:
		{
			// 24ドット グラフィック印字 リピート
			CCCheckNewPage();
			if (m_escpbuf.size() < 6) return;
			if (m_escpbuf.at(2) < '0' || '9' < m_escpbuf.at(2) ||
				m_escpbuf.at(3) < '0' || '9' < m_escpbuf.at(3) ||
				m_escpbuf.at(4) < '0' || '9' < m_escpbuf.at(4) ||
				m_escpbuf.at(5) < '0' || '9' < m_escpbuf.at(5)) {
				m_escpbuf.clear();
				break;
			}
			int gw = ((m_escpbuf.at(2) - '0') * 1000 + (m_escpbuf.at(3) - '0') * 100 + (m_escpbuf.at(4) - '0') * 10 + (m_escpbuf.at(5) - '0')) * 3;
			if (m_escpbuf.size() < 6 + 3) return;
			m_escpr.hasgraphic = 1;
			m_escpr.scalex_graph = m_escp.scalex;
			m_escpr.scaley_graph = m_escp.scaley;
			float pitchx = (float)m_dpiX * dotPitch * (m_escp.so ? 2.0f : 1.0f) * m_escp.scalex;
			float pitchy = (float)m_dpiY / graph_escp_lpi / 24 * m_escp.scaley;
			m_escp.posx += gw * pitchx / 3;
			m_escp.maxlineheight = max(m_escp.maxlineheight, m_dpiY / graph_escp_lpi * m_escpr.scaley);
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6 + 3);
			m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			break;
		}
		case 0x41:
		{
			m_escp.lpi = 6; // 1/6
			m_escp.maxlineheight = 0; // 強制リセット
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 2);
			CCCheckNewPage(true);
			break;
		}
		case 0x42:
		{
			m_escp.lpi = 8; // 1/8
			m_escp.maxlineheight = 0; // 強制リセット
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 2);
			CCCheckNewPage(true);
			break;
		}
		case 0x54:
		{
			if (m_escpbuf.size() < 4) return;
			int value = (m_escpbuf.at(2) - '0') * 10 + (m_escpbuf.at(3) - '0');
			m_escp.lpi = 120.0f / value; // n/240
			m_escp.maxlineheight = 0; // 強制リセット
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 4);
			CCCheckNewPage(true);
			break;
		}
		case 0x4f:
		{
			if (m_escpbuf.size() < 3) return;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 3);
			break;
		}
		case 0x44:
		{
			m_escp.copymode = true;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 2);
			break;
		}
		case 0x4d:
		{
			m_escp.copymode = false;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 2);
			break;
		}
		case 0x43:
		{
			if (m_escpbuf.size() < 3) return;
			m_escp.color = (m_escpbuf.at(2) - '0') & 0x7;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 3);
			break;
		}
		default:
		{
			if (param == 0x4e ||
				param == 0x48 ||
				param == 0x51 ||
				param == 0x45 ||
				param == 0x50 ||
				param == 0x4b ||
				param == 0x74) {
				if (param != m_escp.escmode) {
					m_escp.escmode = param;
					CCUpdateFont();
				}
			}
			else if (param <= 0x08) {
				float pitchx = (float)m_dpiX * dotPitch;
				m_escp.posx += pitchx * param;
				CCCheckNewLine();
				CCCheckNewPage();
			}
			m_escpbuf.erase(m_escpbuf.begin());
			m_escpbuf.erase(m_escpbuf.begin());
			break;
		}
		}
		break;
	}

	case 0x1c: // FS
	{
		if (m_escpbuf.size() < 2) return;
		UINT8 param = m_escpbuf.at(1);
		switch (param) {
		case 0x30:
		{
			if (m_escpbuf.size() < 4) return;
			UINT8 param2 = m_escpbuf.at(2);
			UINT8 param3 = m_escpbuf.at(3);
			if (param2 = 0x34 && (param3 == 0x3c || param3 == 0x53)) {
				if (m_escpbuf.size() < 7) return;
				m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 7);
			}
			else if (param2 = 0x36 && param3 == 0x46) {
				if (m_escpbuf.size() < 9) return;
				m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 9);
			}
			else if (param2 = 0x34 && param3 == 0x4c) {
				if (m_escpbuf.size() < 7) return;
				m_escp.linep1 = m_escpbuf.at(4);
				m_escp.linep2 = m_escpbuf.at(5) - '0';
				m_escp.linep3 = m_escpbuf.at(6) - '0';
				m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 7);
			}
			else {
				m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 4);
			}
			break;
		}
		case 0x77:
		{
			if (m_escpbuf.size() < 6) return;
			m_escp.dotsp_left = m_escpbuf.at(2) - '0';
			m_escp.dotsp_right = m_escpbuf.at(4) - '0';
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6);
			break;
		}
		case 0x70:
		{
			if (m_escpbuf.size() < 8) return;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 8);
			break;
		}
		case 0x66:
		{
			if (m_escpbuf.size() < 6) return;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 6);
			break;
		}
		case 0x63:
		{
			if (m_escpbuf.back() != '.') return;
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.end());
			break;
		}
		default:
		{
			m_escpbuf.erase(m_escpbuf.begin(), m_escpbuf.begin() + 2);
			break;
		}
		}
		break;
	}

	default:
	{
		bool isKanji = (m_escp.escmode == 0x4b || m_escp.escmode == 0x74);
		if (isKanji) {
			if (m_escpbuf.size() < 2) return;
		}
		if (0x20 <= cmd || (isKanji && cmd == 0 && 0x20 <= m_escpbuf.at(1))) {
			CCCheckNewPage();
			float charWidth = (float)m_dpiX / m_escp.cpi * (m_escp.so ? 2 : 1);
			TCHAR buf[3] = { 9 };
			UINT8 th[3] = { 0 };
			if (isKanji) {
				if (cmd == 0) {
					// 実質1バイト文字
					th[0] = m_escpbuf.at(1);
				}
				else {
					UINT16 sjis = jis_to_sjis(cmd << 8 | m_escpbuf.at(1));
					th[0] = sjis >> 8;
					th[1] = sjis & 0xff;
					charWidth *= 2;
				}
				m_escpbuf.erase(m_escpbuf.begin());
			}
			else {
				th[0] = cmd;
			}
			UINT16 thw[2];
			codecnv_sjistoucs2(thw, 1, (const char*)th, 2);
			buf[0] = (TCHAR)thw[0];
			buf[1] = (TCHAR)thw[1];
			m_escpbuf.erase(m_escpbuf.begin());

			float pitchx = (float)m_dpiX * dotPitch;
			m_escp.posx += m_escp.dotsp_left * pitchx;

			UINT8 dat1 = isKanji ? *(m_escprlin.end() - 2) : 0;
			UINT8 dat2 = *(m_escprlin.end() - 1);
			m_escprlin.erase(m_escprlin.end() - (isKanji ? 2 : 1), m_escprlin.end());
			CCCheckNewLine(charWidth);
			if(isKanji) m_escprlin.push_back(dat1);
			m_escprlin.push_back(dat2);
			if (!m_escp.deselect) {
				if (m_escp.so || m_escp.escmode == 0x45 || m_escp.escmode == 0x50 || m_escp.escmode == 0x51 || m_escp.scalex != 1 || m_escp.scaley != 1) {
					charWidth *= m_escp.scalex;
					if (m_escp.escmode == 0x51) { // コンデンス
						charWidth *= 0.6;
					}
					else if (m_escp.escmode == 0x45) { // エリート
						charWidth *= 0.8;
					}
					else if (m_escp.escmode == 0x50) { // プロポーショナル XXX; 本当は字の幅が可変
						charWidth *= 0.9;
					}
				}
				m_escp.maxscaley = max(m_escp.maxscaley, m_escp.scaley);
			}
			m_escp.posx += charWidth;
			m_escp.posx += m_escp.dotsp_right * pitchx;
		}
		else {
			m_escpbuf.erase(m_escpbuf.begin());
			if (isKanji) {
				m_escpbuf.erase(m_escpbuf.begin());
			}
		}
	}
	}
}

