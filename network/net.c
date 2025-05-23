/**
 * @file	net.c
 * @brief	Virtual LAN Interface
 *
 * @author	$Author: SimK $
 */

#include	"compiler.h"

#if defined(SUPPORT_NET)

#include	"pccore.h"
#include	"net.h"
#ifdef SUPPORT_LGY98
#include	"lgy98.h"
#endif
#include <winioctl.h>
#include <tchar.h>

#if defined(_WINDOWS)
#include	<process.h>
#endif

#pragma warning(disable: 4996)
#pragma comment(lib, "Advapi32.lib")

#define DEVICE_PATH_FMT _T("\\\\.\\Global\\%s.tap")
 
#define TAP_CONTROL_CODE(request,method) \
  CTL_CODE (FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)
 
#define TAP_IOCTL_SET_MEDIA_STATUS \
  TAP_CONTROL_CODE (6, METHOD_BUFFERED)
 
#define NET_BUFLEN (16*1024) // バッファ1つの長さ（XXX: パケットサイズの最大値にしないと無駄。もっと言えば可変長で大きな1つのバッファに入れるべき？）
#define NET_ARYLEN (128) // バッファの数

	NP2NET	np2net;
	
static TCHAR *GetNetWorkDeviceGuid(CONST TCHAR *, TCHAR *, DWORD); // TAPデバイス名からGUIDを取得する

static TCHAR np2net_tapName[MAX_PATH]; // TAPデバイス名

static HANDLE	np2net_hTap = INVALID_HANDLE_VALUE; // TAPデバイスの読み書きハンドル
static HANDLE	np2net_hThreadR = NULL; // Read用スレッド
static HANDLE	np2net_hThreadW = NULL; // Write用スレッド
static int		np2net_hThreadexit = 0; // スレッド終了フラグ
static HANDLE	np2net_thread_eventhandle = INVALID_HANDLE_VALUE; // 送受信要求用

static REG8		np2net_membuf[NET_ARYLEN][NET_BUFLEN]; // 送信用バッファ
static int		np2net_membuflen[NET_ARYLEN]; // 送信用バッファにあるデータの長さ
static int		np2net_membuf_readpos = 0; // バッファ読み取り位置
static int		np2net_membuf_writepos = 0; // バッファ書き込み位置

static BOOL np2net_cs_initialized = 0; // np2net クリティカルセクション 初期化済みフラグ
static CRITICAL_SECTION	np2net_cs = {0}; // np2net クリティカルセクション
static void np2net_cs_Initialize(){
	if(!np2net_cs_initialized){
		InitializeCriticalSection(&np2net_cs);
		np2net_thread_eventhandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		np2net_cs_initialized = TRUE;
	}
}
static void np2net_cs_Finalize(){
	if(np2net_cs_initialized){
		DeleteCriticalSection(&np2net_cs);
		CloseHandle(np2net_thread_eventhandle);
		np2net_cs_initialized = FALSE;
	}
}
void np2net_cs_EnterCriticalSection(){
	if(np2net_cs_initialized){
		EnterCriticalSection(&np2net_cs);
	}
}
void np2net_cs_LeaveCriticalSection(){
	if(np2net_cs_initialized){
		LeaveCriticalSection(&np2net_cs);
	}
}

// パケットデータを TAP へ書き出す
static int doWriteTap(HANDLE hTap, const UCHAR *pSendBuf, DWORD len, OVERLAPPED *ovl)
{
	DWORD dwWriteLen;

	np2net_cs_EnterCriticalSection();
	if (!WriteFile(hTap, pSendBuf, len, &dwWriteLen, ovl)) {
		DWORD err = GetLastError();
		np2net_cs_LeaveCriticalSection();
		if (err == ERROR_IO_PENDING) {
			DWORD beginTime = GETTICK();
			int cancel = 0;
			// 完了待ち
			while(1){
				if (WaitForSingleObject(ovl->hEvent, 1000) == WAIT_OBJECT_0){
					BOOL result;
					np2net_cs_EnterCriticalSection();
					result = GetOverlappedResult(np2net_hTap, ovl, &dwWriteLen, FALSE);
					err = GetLastError();
					np2net_cs_LeaveCriticalSection();
					if(cancel){
						dwWriteLen = len; // 書けたことにする
					}
					if(result) break;
					if (err != ERROR_IO_INCOMPLETE) {
						break;
					}
				}
				if(!cancel && GETTICK() - beginTime > 3000){
					//CancelIoEx(np2net_hTap, ovl); // 秒単位で終わらないのはおかしいのでキャンセル
					cancel = 1;
				}
				if(np2net_hThreadexit){
					break;
				}
				//if(((np2net_membuf_writepos - np2net_membuf_readpos) & (NET_ARYLEN - 1)) < NET_ARYLEN/4){
					Sleep(0);
				//}else{
				//	Sleep(1);
				//}
			}
			if(dwWriteLen==0){
				return 1;
			}
		} else {
			TRACEOUT(("LGY-98: WriteFile err=0x%08X\n", err));
			return -1;
		}
	}else{
		np2net_cs_LeaveCriticalSection();
	}
	//TRACEOUT(("LGY-98: send %u bytes\n", dwWriteLen));
	return 0;
}

// パケットデータをバッファに送る（実際の送信はnp2net_ThreadFuncW内で行われる）
static int sendDataToBuffer(UCHAR *pSendBuf, DWORD len){
	if(len > NET_BUFLEN){
		TRACEOUT(("LGY-98: too large packet!! %d bytes", len));
		return 1;
	}
	if(np2net_membuf_readpos==(np2net_membuf_writepos+1)%NET_ARYLEN){
		TRACEOUT(("LGY-98: buffer full"));
		while(np2net_membuf_readpos==(np2net_membuf_writepos+1)%NET_ARYLEN){
			//Sleep(0); // バッファがいっぱいなので待つ
			return 1; // バッファがいっぱいなので捨てる
		}
	}
	if(NET_BUFLEN < len){
		return 1; // 巨大パケット（？）は捨てる
	}
	memcpy(np2net_membuf[np2net_membuf_writepos], pSendBuf, len);
	np2net_membuflen[np2net_membuf_writepos] = len;
	np2net_membuf_writepos = (np2net_membuf_writepos+1)%NET_ARYLEN;
	SetEvent(np2net_thread_eventhandle);
	return 0;
}

// パケット送信時に呼ばれる（デフォルト処理）
static void np2net_default_send_packet(const UINT8 *buf, int size)
{
	sendDataToBuffer((UCHAR*)buf, size);
}
// パケット受信時に呼ばれる（デフォルト処理）
static void np2net_default_recieve_packet(const UINT8 *buf, int size)
{
	// 何もしない
}

//  非同期で通信してみる（Write）
static unsigned int __stdcall np2net_ThreadFuncW(LPVOID vdParam) {
	HANDLE hEvent = NULL;
	OVERLAPPED ovl;

	// OVERLAPPED非同期書き込み準備
	memset(&ovl, 0, sizeof(OVERLAPPED));
	ovl.hEvent = hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hEvent == NULL) return 0;
	ovl.Offset = 0;
	ovl.OffsetHigh = 0;

	while (WaitForSingleObject(np2net_thread_eventhandle, INFINITE) == WAIT_OBJECT_0)
	{
		if (np2net_hThreadexit) break;
		while (np2net_membuf_readpos != np2net_membuf_writepos)
		{
			if (np2net_hThreadexit) break;
			doWriteTap(np2net_hTap, (UCHAR*)(np2net_membuf[np2net_membuf_readpos]), np2net_membuflen[np2net_membuf_readpos], &ovl);
			np2net_membuf_readpos = (np2net_membuf_readpos + 1) % NET_ARYLEN;
		}
	}
	CloseHandle(hEvent);
	hEvent = NULL;
	return 0;
}
//  非同期で通信してみる（Read）
static unsigned int __stdcall np2net_ThreadFuncR(LPVOID vdParam) {
	HANDLE hEvent = NULL;
	DWORD dwLen;
	OVERLAPPED ovl;
	int nodatacount = 0;
	int sleepcount = 0;
	int timediff = 0;
	CHAR* np2net_Buf = (CHAR*)malloc(NET_BUFLEN);
	if (!np2net_Buf) {
		return 0;
	}

	// OVERLAPPED非同期読み取り準備
	memset(&ovl, 0, sizeof(OVERLAPPED));
	ovl.hEvent = hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hEvent == NULL) {
		free(np2net_Buf);
		return 0;
	}
	ovl.Offset = 0;
	ovl.OffsetHigh = 0;
 
	while (!np2net_hThreadexit) {
		np2net_cs_EnterCriticalSection();
		if (!ReadFile(np2net_hTap, np2net_Buf, NET_BUFLEN, &dwLen, &ovl)) {
			DWORD err = GetLastError();
			np2net_cs_LeaveCriticalSection();
			if (err == ERROR_IO_PENDING) {
				// 読み取り待ち
				while(1){
					if (WaitForSingleObject(ovl.hEvent, 1000) == WAIT_OBJECT_0){
						BOOL result;
						np2net_cs_EnterCriticalSection();
						result = GetOverlappedResult(np2net_hTap, &ovl, &dwLen, FALSE);
						err = GetLastError();
						np2net_cs_LeaveCriticalSection();
						if(result) break;
						if (err != ERROR_IO_INCOMPLETE) {
							break;
						}
					}
					if(np2net_hThreadexit){
						break;
					}
					Sleep(0);
				}
				if(dwLen>0){
					//TRACEOUT(("LGY-98: recieve %u bytes\n", dwLen));
					np2net.recieve_packet((UINT8*)np2net_Buf, dwLen); // 受信できたので通知する
				}
			} else {
				// 読み取りエラー
				printf("TAP-Win32: ReadFile err=0x%08X\n", err);
				//CloseHandle(hTap);
				//return -1;
				Sleep(0);
			}
		} else {
			np2net_cs_LeaveCriticalSection();
			// 読み取り成功
			if(dwLen>0){
				//TRACEOUT(("LGY-98: recieve %u bytes\n", dwLen));
				np2net.recieve_packet((UINT8*)np2net_Buf, dwLen); // 受信できたので通知する
			}else{
				Sleep(0);
			}
		}
	}
	CloseHandle(hEvent);
	hEvent = NULL;
	free(np2net_Buf);
	return 0;
}

//  TAPデバイスを閉じる
static void np2net_closeTAP(){
	ULONG status = FALSE;
	DWORD dwLen;
    if (np2net_hTap != INVALID_HANDLE_VALUE) {
		if(np2net_hThreadR){
			np2net_hThreadexit = 1;
			SetEvent(np2net_thread_eventhandle);
			if(WaitForSingleObject(np2net_hThreadR, 10000) == WAIT_TIMEOUT){
				TerminateThread(np2net_hThreadR, 0);
			}
			if(WaitForSingleObject(np2net_hThreadW, 10000) == WAIT_TIMEOUT){
				TerminateThread(np2net_hThreadW, 0);
			}
			np2net_membuf_readpos = np2net_membuf_writepos;
			np2net_hThreadexit = 0;
			CloseHandle(np2net_hThreadR);
			CloseHandle(np2net_hThreadW);
			np2net_hThreadR = NULL;
			np2net_hThreadW = NULL;
		}
		
		// TAP デバイスを非アクティブに
		status = FALSE;
		if (!DeviceIoControl(np2net_hTap,TAP_IOCTL_SET_MEDIA_STATUS,
					&status, sizeof(status), &status, sizeof(status),
					&dwLen, NULL)) {
			TRACEOUT(("LGY-98: TAP_IOCTL_SET_MEDIA_STATUS err"));
		}
 
		CloseHandle(np2net_hTap);
		TRACEOUT(("LGY-98: TAP is closed"));
		np2net_hTap = INVALID_HANDLE_VALUE;
    }
}
//  TAPデバイスを開く
static int np2net_openTAP(const TCHAR* tapname){
	unsigned int dwID;
	DWORD dwLen;
	ULONG status = TRUE;
	TCHAR Buf[2048];
	TCHAR szDevicePath[256];
	TCHAR szTAPname[MAX_PATH] = _T("TAP1");

	if(*tapname){
		_tcscpy(szTAPname, tapname);
	}

	np2net_closeTAP();

	// 指定された表示名から TAP の GUID を得る
	if (!GetNetWorkDeviceGuid(szTAPname, Buf, 2048)) {
		TRACEOUT(("LGY-98: [%s] GUID is not found\n", szTAPname));
		return 1;
	}
	TRACEOUT(("LGY-98: [%s] GUID = %s\n", szTAPname, Buf));
	_stprintf(szDevicePath, DEVICE_PATH_FMT, Buf);
 
	// TAP デバイスを開く
	np2net_hTap = CreateFile (szDevicePath, GENERIC_READ | GENERIC_WRITE,
		0, 0, OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
 
	if (np2net_hTap == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		TRACEOUT(("LGY-98: Failed to open [%s] error:%d", szDevicePath, err));
		return 2;
	}

	TRACEOUT(("LGY-98: TAP is opened"));
	
	// TAP デバイスをアクティブに
	status = TRUE;
	if (!DeviceIoControl(np2net_hTap,TAP_IOCTL_SET_MEDIA_STATUS,
				&status, sizeof(status), &status, sizeof(status),
				&dwLen, NULL)) {
		TRACEOUT(("LGY-98: TAP_IOCTL_SET_MEDIA_STATUS err"));
		np2net_closeTAP();
		return 3;
	}
 
	np2net_hThreadR = (HANDLE)_beginthreadex(NULL , 0 , np2net_ThreadFuncR  , NULL , 0 , &dwID);
	np2net_hThreadW = (HANDLE)_beginthreadex(NULL , 0 , np2net_ThreadFuncW , NULL , 0 , &dwID);
	
	return 0;
}

// NP2起動時の処理
void np2net_init(void)
{
	np2net_cs_Initialize();
	memset(np2net_tapName, 0, sizeof(np2net_tapName));
	np2net.send_packet = np2net_default_send_packet;
	np2net.recieve_packet = np2net_default_recieve_packet;
}
// リセット時に呼ばれる？
void np2net_reset(const NP2CFG *pConfig){
	_tcscpy(np2net_tapName, pConfig->np2nettap);
	if(pConfig->uselgy98){ // XXX: 使われていないならTAPデバイスはオープンしない
		np2net_openTAP(np2net_tapName);
	}
}
// リセット時に呼ばれる？（np2net_resetより後・iocore_attach〜が使える）
void np2net_bind(void){
}
// NP2終了時の処理
void np2net_shutdown(void)
{
	np2net_hThreadexit = 1;
	SetEvent(np2net_thread_eventhandle);
	np2net_closeTAP();

#ifdef SUPPORT_LGY98
	lgy98_shutdown();
#endif
	np2net_cs_Finalize();
}

// 参考文献: http://dsas.blog.klab.org/archives/51012690.html

// ネットワークデバイス表示名からデバイス GUID 文字列を検索
static TCHAR *GetNetWorkDeviceGuid(CONST TCHAR *pDisplayName, TCHAR *pszBuf, DWORD cbBuf)
{
  CONST TCHAR *SUBKEY = _T("SYSTEM\\CurrentControlSet\\Control\\Network");
 
#define BUFSZ 256
  // HKLM\SYSTEM\\CurrentControlSet\\Control\\Network\{id1]\{id2}\Connection\Name が
  // ネットワークデバイス名（ユニーク）の格納されたエントリであり、
  // {id2} がこのデバイスの GUID である
 
  HKEY hKey1, hKey2, hKey3;
  LONG nResult;
  DWORD dwIdx1, dwIdx2;
  TCHAR szData[64], *pKeyName1, *pKeyName2, *pKeyName3, *pKeyName4; 
  DWORD dwSize, dwType = REG_SZ;
  BOOL bDone = FALSE;
  FILETIME ft;

  hKey1 = hKey2 = hKey3 = NULL;
  pKeyName1 = pKeyName2 = pKeyName3 = pKeyName4 = NULL;
 
  // 主キーのオープン
  nResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, SUBKEY, 0, KEY_READ, &hKey1);
  if (nResult != ERROR_SUCCESS) {
    return NULL;
  }
  pKeyName1 = (TCHAR*)malloc(sizeof(TCHAR)*BUFSZ);
  pKeyName2 = (TCHAR*)malloc(sizeof(TCHAR)*BUFSZ);
  pKeyName3 = (TCHAR*)malloc(sizeof(TCHAR)*BUFSZ);
  pKeyName4 = (TCHAR*)malloc(sizeof(TCHAR)*BUFSZ);
 
  dwIdx1 = 0;
  while (bDone != TRUE) { // {id1} を列挙するループ
 
    dwSize = BUFSZ;
    nResult = RegEnumKeyEx(hKey1, dwIdx1++, pKeyName1,
                          &dwSize, NULL, NULL, NULL, &ft);
    if (nResult == ERROR_NO_MORE_ITEMS) {
      break;
    }
 
    // SUBKEY\{id1} キーをオープン
    _stprintf(pKeyName2, _T("%s\\%s"), SUBKEY, pKeyName1);
    nResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pKeyName2,
                          0, KEY_READ, &hKey2);
    if (nResult != ERROR_SUCCESS) {
      continue;
    }
    dwIdx2 = 0;
    while (1) { // {id2} を列挙するループ
      dwSize = BUFSZ;
      nResult = RegEnumKeyEx(hKey2, dwIdx2++, pKeyName3,
                          &dwSize, NULL, NULL, NULL, &ft);
      if (nResult == ERROR_NO_MORE_ITEMS) {
        break;
      }
 
      if (nResult != ERROR_SUCCESS) {
        continue;
      }
 
      // SUBKEY\{id1}\{id2]\Connection キーをオープン
      _stprintf(pKeyName4, _T("%s\\%s\\%s"),
                      pKeyName2, pKeyName3, _T("Connection"));
      nResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                      pKeyName4, 0, KEY_READ, &hKey3);
      if (nResult != ERROR_SUCCESS) {
        continue;
      }
 
      // SUBKEY\{id1}\{id2]\Connection\Name 値を取得
      dwSize = sizeof(szData);
      nResult = RegQueryValueEx(hKey3, _T("Name"),
                      0, &dwType, (LPBYTE)szData, &dwSize);
 
      if (nResult == ERROR_SUCCESS) {
        if (_tcsicmp(szData, pDisplayName) == 0) {
           	_tcscpy(pszBuf, pKeyName3);
          bDone = TRUE;
          break;
        }
      }
      RegCloseKey(hKey3);
      hKey3 = NULL;
    }
    RegCloseKey(hKey2);
    hKey2 = NULL;
  }
 
  if (hKey1) { RegCloseKey(hKey1); }
  if (hKey2) { RegCloseKey(hKey2); }
  if (hKey3) { RegCloseKey(hKey3); }
 
  if (pKeyName1) { free(pKeyName1); }
  if (pKeyName2) { free(pKeyName2); }
  if (pKeyName3) { free(pKeyName3); }
  if (pKeyName4) { free(pKeyName4); }
 
  // GUID を発見できず
  if (bDone != TRUE) {
    return NULL;
  }
  return pszBuf;
}

#endif	/* SUPPORT_NET */