/**
 * @file	cmwacom.cpp
 * @brief	Wacom Tablet �N���X�̓���̒�`���s���܂�
 */

#include "compiler.h"
#include "cmwacom.h"
#include "np2.h"

#ifdef SUPPORT_WACOM_TABLET

#include <math.h>
#include "mousemng.h"
#include "scrnmng.h"

typedef CComWacom *CMWACOM;

const CHAR cmwacom_RData[] = "~RE202C900,002,02,1270,1270\r";
const CHAR cmwacom_ModelData[] = "~#KT-0405-R00 V1.3-2\r";
const CHAR cmwacom_CData[] = "~C06400,04800\r";

#define TABLET_BASERASOLUTION	1000

#define TABLET_RAWMAX_X	5040
#define TABLET_RAWMAX_Y	3779

static bool g_wacom_initialized = false;
static bool g_wacom_allocated = false;
static CComWacom *g_cmwacom = NULL;
static WNDPROC g_lpfnDefProc = NULL;
static bool g_exclusivemode = false;
static bool g_nccontrol = false;

void cmwacom_initialize(void){
	if(!g_wacom_initialized){
		if ( LoadWintab( ) ){
			g_wacom_initialized = true;
		}else{
			g_wacom_initialized = false;
		}
	}
}
void cmwacom_finalize(void){
	if(g_wacom_initialized){
		UnloadWintab();
	}
	g_wacom_initialized = false;
}
bool cmwacom_skipMouseEvent(void){
	return g_cmwacom && g_cmwacom->SkipMouseEvent();
}
void cmwacom_setExclusiveMode(bool enable){
	g_exclusivemode = enable;
	if(g_cmwacom){
		g_cmwacom->SetExclusiveMode(enable);
	}
}
void cmwacom_setNCControl(bool enable){
	g_nccontrol = enable;
	if(g_cmwacom){
		g_cmwacom->SetNCControl(enable);
	}
}

LRESULT CALLBACK tabletWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch (msg) {
	case WT_PACKET:
		if(g_cmwacom->HandlePacketMessage(hWnd, msg, wParam, lParam)){
			return FALSE;
		}
		break;
	case WM_MOUSEMOVE:
		if(g_cmwacom->HandleMouseMoveMessage(hWnd, msg, wParam, lParam)){
			return FALSE;
		}
		break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		if(g_cmwacom->HandleMouseUpMessage(hWnd, msg, wParam, lParam)){
			return FALSE;
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if(g_cmwacom->HandleMouseDownMessage(hWnd, msg, wParam, lParam)){
			return FALSE;
		}
		break;
	case WM_ACTIVATE:
		if (wParam) {
			gpWTOverlap(g_cmwacom->GetHTab(), TRUE);
		}
		gpWTEnable(g_cmwacom->GetHTab(), (BOOL)wParam);
		{
			SINT16 x, y;
			mousemng_getstat(&x, &y, false);
			mousemng_setstat(x, y, uPD8255A_LEFTBIT|uPD8255A_RIGHTBIT);
		}
		g_cmwacom->m_skiptabletevent = 20;
		break;
    }
	return CallWindowProc(g_lpfnDefProc, hWnd, msg, wParam, lParam);
}


/**
 * �C���X�^���X�쐬
 * @param[in] lpMidiOut MIDIOUT �f�o�C�X
 * @param[in] lpMidiIn MIDIIN �f�o�C�X
 * @param[in] lpModule ���W���[��
 * @return �C���X�^���X
 */
CComWacom* CComWacom::CreateInstance(HWND hWnd)
{
	CComWacom* pWacom = new CComWacom;
	if (!pWacom->Initialize(hWnd))
	{
		delete pWacom;
		pWacom = NULL;
	}
	return pWacom;
}

/**
 * �R���X�g���N�^
 */
CComWacom::CComWacom()
	: CComBase(COMCONNECT_TABLET)
	, m_hwndMain(NULL)
	, m_hTab(NULL)
	, m_hMgr(NULL)
	, m_start(false)
	, m_cmdbuf_pos(0)
	, m_sBuffer_wpos(0)
	, m_sBuffer_rpos(0)
	, m_wait(0)
	, m_skipmouseevent(0)
	, m_skiptabletevent(0)
	, m_exclusivemode(false)
	, m_nccontrol(false)
	, m_resolution(TABLET_BASERASOLUTION)
	, m_lastdatalen(0)
{
	ZeroMemory(m_sBuffer, sizeof(m_sBuffer));
}

/**
 * �f�X�g���N�^
 */
CComWacom::~CComWacom()
{
	if(g_lpfnDefProc){
		SetWindowLongPtr(m_hwndMain, GWLP_WNDPROC, (LONG_PTR)g_lpfnDefProc);
		g_lpfnDefProc = NULL;
	}
}

/**
 * ������
 * @retval true ����
 * @retval false ���s
 */
bool CComWacom::Initialize(HWND hWnd)
{
	LOGCONTEXTA lcMine;
	AXIS axis;
	AXIS pressAxis;
	AXIS rotAxis[3] = {0};
	
	if(!g_wacom_initialized){
		return false; // ����������Ă��Ȃ�
	}
	if(g_wacom_allocated){
		return false; // �����̗��p�͕s��
	}

	if (!gpWTInfoA(0, 0, NULL)) {
		return false; // WinTab�g�p�s��
	}
	
	m_hwndMain = hWnd;
	
	if(m_hwndMain && !g_lpfnDefProc){
		g_lpfnDefProc = (WNDPROC)GetWindowLongPtr(m_hwndMain, GWLP_WNDPROC);
		SetWindowLongPtr(m_hwndMain, GWLP_WNDPROC, (LONG_PTR)tabletWndProc);
	}

	g_cmwacom = this;

	SetExclusiveMode(g_exclusivemode);
	SetNCControl(g_nccontrol);

	InitializeTabletDevice();
	
	return true;
}


void CComWacom::InitializeTabletDevice(){
	LOGCONTEXTA lcMine;
	AXIS axis;
	AXIS pressAxis;
	AXIS rotAxis[3] = {0};
	
	if(!g_wacom_initialized){
		return; // ����������Ă��Ȃ�
	}
	if(g_wacom_allocated){
		return; // �����̗��p�͕s��
	}

	if (!gpWTInfoA(0, 0, NULL)) {
		return; // WinTab�g�p�s��
	}
	
	if(m_exclusivemode){
		FIX32 axRes;
		gpWTInfoA(WTI_DEVICES, DVC_X, &axis);
		m_minX = axis.axMin;
		m_maxX = axis.axMax; /* �w�����̍ő���W */
		m_resX = axis.axResolution; /* �w���W�̕���\ �P��:line/inch */
		gpWTInfoA(WTI_DEVICES, DVC_Y, &axis);
		m_minY = axis.axMin;
		m_maxY = axis.axMax; /* �x�����̍ő���W */
		m_resY = axis.axResolution; /* �x���W�̕���\ �P��:line/inch */
		gpWTInfoA(WTI_DEFSYSCTX, 0, &lcMine);
	}

	gpWTInfoA(m_exclusivemode ? WTI_DEFCONTEXT : WTI_DEFSYSCTX, 0, &lcMine);
	lcMine.lcOptions |= CXO_MESSAGES|CXO_MARGIN; /* Wintabү���ނ��n�����悤�ɂ��� */
	lcMine.lcMsgBase = WT_DEFBASE;
	lcMine.lcPktData = PACKETDATA;
	lcMine.lcPktMode = PACKETMODE;
	lcMine.lcMoveMask = PACKETDATA;
	lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;
	if(m_exclusivemode){
		lcMine.lcInOrgX = m_minX;
		lcMine.lcInOrgY = m_minY;
		lcMine.lcInExtX = m_maxX; /* ���گĂ̗L���͈͑S��𑀍�ر�Ƃ��܂� */
		lcMine.lcInExtY = m_maxY;
		lcMine.lcOutOrgX = 0;
		lcMine.lcOutOrgY = 0;
		lcMine.lcOutExtX = m_maxX-m_minX; /* ���گĂ̕���\�Ɠ����ް��̕���\�� */
		lcMine.lcOutExtY = m_maxY-m_minY; /* ���킹�܂� */

	}else{
		m_minX = lcMine.lcOutOrgX;
		m_minY = lcMine.lcOutOrgY;
		m_maxX = lcMine.lcOutExtX;
		m_maxY = lcMine.lcOutExtY;
	}
	m_hTab = gpWTOpenA(m_hwndMain, &lcMine, TRUE);
	if (!m_hTab)
	{
		return;
	}
	// WTI_DEVICES��DVC_NPRESSURE���擾 �i�M���l�̍ő�A�ŏ��j
	gpWTInfoA(WTI_DEVICES, DVC_NPRESSURE, &pressAxis);
	m_rawPressureMax = pressAxis.axMax;
	m_rawPressureMin = pressAxis.axMin;
	gpWTInfoA(WTI_DEVICES, DVC_TPRESSURE, &pressAxis);
	m_rawTangentPressureMax = pressAxis.axMax;
	m_rawTangentPressureMin = pressAxis.axMin;
	gpWTInfoA(WTI_DEVICES, DVC_ROTATION, rotAxis);
	m_rawAzimuthMax = rotAxis[0].axMax;
	m_rawAzimuthMin = rotAxis[0].axMin;
	m_rawAltitudeMax = rotAxis[1].axMax;
	m_rawAltitudeMin = rotAxis[1].axMin;
	m_rawTwistMax = rotAxis[2].axMax;
	m_rawTwistMin = rotAxis[2].axMin;

    m_hMgr = gpWTMgrOpen(m_hwndMain, WT_DEFBASE);
    if(m_hMgr) {
        m_ObtBuf[0] = 1;
        gpWTMgrExt(m_hMgr, WTX_OBT, m_ObtBuf);
    }
}
void CComWacom::FinalizeTabletDevice(){
	if (m_hTab)
	{
		if (m_hMgr) {
			// Out of Bounds Tracking �̉���
			m_ObtBuf[0] = 0;
			gpWTMgrExt(m_hMgr, WTX_OBT, m_ObtBuf);
			gpWTMgrClose(m_hMgr);
			m_hMgr = NULL;
		}
		gpWTClose(m_hTab);
		m_hTab = NULL;
		g_wacom_allocated = false;
	}
}

HCTX CComWacom::GetHTab(){
	return m_hTab;
}
bool CComWacom::HandlePacketMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	PACKET tPckt;
	static SINT32 last_rawButtons;
	
	m_fixedaspect = np2oscfg.pentabfa;

	if (gpWTPacket((HCTX)LOWORD(lParam), (UINT)wParam, (LPSTR)&tPckt)) {
		bool proximityflag = false;
		int newbuttons;
		if(!m_exclusivemode && !m_nccontrol){
			m_skipmouseevent = 0;
			return true;
		}
		if(m_skiptabletevent>0){
			m_skiptabletevent--;
			return true;
		}
		if(tPckt.pkStatus & TPS_PROXIMITY){
			proximityflag = true;
		}
		
		m_skipmouseevent = 2; // WinTab�̃o�O���

		m_rawX = tPckt.pkX; /* �f�W�^�C�U��̂w���W */
		m_rawY = tPckt.pkY; /* �f�W�^�C�U��̂x���W */
		newbuttons = LOWORD(tPckt.pkButtons); /* �{�^���ԍ� */
		m_rawStatus = tPckt.pkStatus;

		m_rawPressure = LOWORD(tPckt.pkNormalPressure); /* �M�� */
		if(m_rawPressureMax-m_rawPressureMin > 0){
			if(m_rawPressure < m_rawPressureMin){
				m_pressure = 0.0;
			}else{
				m_pressure = (double)(m_rawPressure-m_rawPressureMin)/(m_rawPressureMax-m_rawPressureMin);
			}
		}else{
			m_pressure = 0.0;
		}
		
		if((tPckt.pkStatus & TPS_INVERT)){
			newbuttons |= 0x4;
		}
		m_rawButtons = newbuttons;

		m_rawTangentPressure = tPckt.pkTangentPressure;
		if(m_rawTangentPressureMax>0){
			if(m_rawTangentPressure < m_rawTangentPressureMin){
				m_tangentPressure = 0.0;
			}else{
				m_tangentPressure = (double)(m_rawTangentPressure-m_rawTangentPressureMin)/(m_rawTangentPressureMax-m_rawTangentPressureMin);
			}
		}else{
			m_tangentPressure = 0.0;
		}
			   
		m_rawAzimuth = tPckt.pkOrientation.orAzimuth;
		if(m_rawAzimuthMax>0){
			m_azimuth = (double)(m_rawAzimuth)/m_rawAzimuthMax;
		}else{
			m_azimuth = 0.0;
		}

		m_rawAltitude = tPckt.pkOrientation.orAltitude;
		if(m_rawAltitudeMax>0){
			m_altitude = (double)(m_rawAltitude)/m_rawAltitudeMax;
		}else{
			m_altitude = 0.0;
		}

		m_rawTwist = tPckt.pkOrientation.orTwist;
		if(m_rawTwistMax>0){
			m_twist = (double)(m_rawTwist)/m_rawTwistMax;
			m_rotationDeg = (double)(m_rawTwist*360)/m_rawTwistMax;
		}else{
			m_twist = 0.0;
			m_rotationDeg = 0.0;
		}

		if(proximityflag){
			m_pressure = 0.0;
		}

		m_tiltX = m_altitude * cos(m_azimuth*2*M_PI);
		m_tiltY = m_altitude * sin(m_azimuth*2*M_PI);

		if(m_wait==0){
			UINT16 pktPressure = (UINT32)(m_pressure * 255);
			SINT32 pktXtmp, pktYtmp;
			UINT16 pktX, pktY;
			SINT32 tablet_resX = TABLET_RAWMAX_X * m_resolution / TABLET_BASERASOLUTION;
			SINT32 tablet_resY = TABLET_RAWMAX_Y * m_resolution / TABLET_BASERASOLUTION;
			char buf[10];
			if(m_exclusivemode){
				if(m_fixedaspect){
					// �A�X�y�N�g�䂪ArtPad II�Ɠ����ɂȂ�悤�ɏC��
					if(tablet_resX * (m_maxY - m_minY) > tablet_resY * (m_maxX - m_minX)){
						pktXtmp = (m_rawX * tablet_resX / (m_maxX - m_minX));
						pktYtmp = tablet_resY - (m_rawY * tablet_resX / (m_maxX - m_minX)); // �������_�ł����Ă�
					}else{
						pktXtmp = (m_rawX * tablet_resY / (m_maxY - m_minY));
						pktYtmp = tablet_resY - (m_rawY * tablet_resY / (m_maxY - m_minY)); // �������_�ł����Ă�
					}
				}else{
					pktXtmp = (m_rawX * tablet_resX / (m_maxX - m_minX));
					pktYtmp = tablet_resY - (m_rawY * tablet_resY / (m_maxY - m_minY)); // �������_�ł����Ă�
				}
			}else{
				RECT rectClient;
				POINT pt;
				//GetClientRect(hWnd, &rectClient);
				//rectClient.left++;
				//rectClient.top++;
				//rectClient.right--;
				//rectClient.bottom--;
				scrnmng_getrect(&rectClient);
				pt.x = rectClient.left;
				pt.y = rectClient.top;
				ClientToScreen(hWnd, &pt);
				pktXtmp = ((m_rawX - pt.x) * tablet_resX / (rectClient.right - rectClient.left));
				pktYtmp = (((m_maxY - m_rawY) - pt.y) * tablet_resY / (rectClient.bottom - rectClient.top));
				//pktXtmp = m_rawX - pt.x;
				//pktYtmp = m_rawY - pt.y;
				if(pktXtmp < 0 || pktYtmp < 0 || pktXtmp > tablet_resX || pktYtmp > tablet_resY){
					// �͈͊O�͈ړ��̂݉\
					m_rawButtons = m_rawButtons & 0x4;
					pktPressure = 0;
					//proximityflag = true;
				}
			}
			if(pktXtmp < 0) pktXtmp = 0;
			if(pktYtmp < 0) pktYtmp = 0;
			if(pktXtmp > tablet_resX) pktXtmp = tablet_resX;
			if(pktYtmp > tablet_resY) pktYtmp = tablet_resY;
			pktX = (UINT16)pktXtmp;
			pktY = (UINT16)pktYtmp;
			buf[0] = (proximityflag ? 0xA0 : 0xE0)|(m_rawButtons ? 0x8 : 0)|((pktX >> 14) & 0x2)|((((pktPressure) & 1) << 2));
			buf[1] = ((pktX >> 7) & 0x7f);
			buf[2] = (pktX & 0x7f);
			buf[3] = ((m_rawButtons & 0xf) << 3)|((pktY >> 14) & 0x2)|((((pktPressure >> 1) & 1) << 2));
			buf[4] = ((pktY >> 7) & 0x7f);
			buf[5] = (pktY & 0x7f);
			buf[6] = (((m_rawButtons & ~0x4) ? 0x00 : 0x40))|(pktPressure >> 2);
			memcpy(m_lastdata, buf, 7);
			m_lastdatalen = 7;
			if(SendDataToReadBuffer(buf, 7)){
				m_wait += 0;
			}
		}
	}

	return true;
}
bool CComWacom::HandleMouseMoveMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	if(m_skipmouseevent > 0){
		m_skipmouseevent--;
		return true;
	}
	return false;
}
bool CComWacom::HandleMouseUpMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	if(!m_exclusivemode && m_skipmouseevent > 0){
		return true;
	}
	return false;
}
bool CComWacom::HandleMouseDownMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
	if(!m_exclusivemode && m_skipmouseevent > 0){
		return true;
	}
	return false;
}
bool CComWacom::SkipMouseEvent(){
	return (m_skipmouseevent > 0);
}
void CComWacom::SetExclusiveMode(bool enable){
	if(m_exclusivemode != enable){
		FinalizeTabletDevice();
		m_exclusivemode = enable;
		InitializeTabletDevice();
	}
}
void CComWacom::SetNCControl(bool enable){
	m_nccontrol = enable;
}

bool CComWacom::SendDataToReadBuffer(const char *data, int len){
	int bufused = (m_sBuffer_wpos - m_sBuffer_rpos) & (WACOM_BUFFER - 1);
	if(bufused + len >= WACOM_BUFFER){
		// Buffer Full
		return false;
	}
	for(int i=0;i<len;i++){
		m_sBuffer[m_sBuffer_wpos] = *data;
		data++;
		m_sBuffer_wpos = (m_sBuffer_wpos + 1) & (WACOM_BUFFER - 1);
	}
	return true;
}

/**
 * �ǂݍ���
 * @param[out] pData �o�b�t�@
 * @return �T�C�Y
 */
UINT CComWacom::Read(UINT8* pData)
{
	static int nodatacounter = 0;
	if(m_wait > 0){
		m_wait--;
		return 0;
	}
	if(m_sBuffer_wpos != m_sBuffer_rpos){
		*pData = m_sBuffer[m_sBuffer_rpos];
		m_sBuffer_rpos = (m_sBuffer_rpos + 1) & (WACOM_BUFFER - 1);
		return 1;
	}else{
		nodatacounter++;
		// XXX: �����ԃf�[�^���Ȃ���Win9x�ł��������Ȃ�悤�Ȃ̂Ŏb��΍�
		if(nodatacounter > 256){
			if(m_lastdatalen > 0){
				char buf[10];
				memcpy(buf, m_lastdata, 7);
				if(m_rawButtons != 0) {
					buf[0] |= 0x40;
				}else{
					buf[0] &= ~0x40;
				}
				SendDataToReadBuffer(buf, m_lastdatalen);
			}
			nodatacounter = 0;
		}
		return 0;
	}
	return 0;
}

/**
 * ��������
 * @param[out] cData �f�[�^
 * @return �T�C�Y
 */
UINT CComWacom::Write(UINT8 cData)
{
	char buf[1024];
	CMWACOM wtab = this;

	if(m_cmdbuf_pos == WACOM_CMDBUFFER){
		m_cmdbuf_pos = 0; // Buffer Full!
	}
	if (m_hTab)
	{
		if(cData=='\r' || cData=='\n'){ // �R�}���h�I�[�̎��iXXX: LF���R�}���h�I�[�����H�j
			m_cmdbuf[m_cmdbuf_pos] = '\0';
			if(strcmp(m_cmdbuf, "#")==0){
				// Reset to protocol IV command set
				m_sBuffer_rpos = m_sBuffer_wpos; // �f�[�^����
			}else if(strcmp(m_cmdbuf, "$")==0){
				// Reset to 9600 bps (sent at 19200 bps)
				m_sBuffer_rpos = m_sBuffer_wpos; // �f�[�^����
			}else if(strcmp(m_cmdbuf, "ST")==0){
				m_start = true; // Start sending coordinates
			}else if(strcmp(m_cmdbuf, "SP")==0){
				m_start = false; // Stop sending coordinates
			}else if(strcmp(m_cmdbuf, "~R")==0){
				SendDataToReadBuffer(cmwacom_RData, sizeof(cmwacom_RData));
				if(m_wait < WACOM_BUFFER) m_wait += sizeof(cmwacom_RData)*2;
			}else if(strcmp(m_cmdbuf, "~C")==0){
				SendDataToReadBuffer(cmwacom_CData, sizeof(cmwacom_CData));
				if(m_wait < WACOM_BUFFER) m_wait += sizeof(cmwacom_CData)*2;
			}else if(strncmp(m_cmdbuf, "NR", 2)==0){
				// Set Resolution
				m_resolution = atoi(m_cmdbuf + 2);
			}
			m_cmdbuf_pos = 0;
		}else{
			m_cmdbuf[m_cmdbuf_pos] = cData;
			m_cmdbuf_pos++;
			if(m_cmdbuf_pos >= 2 && strncmp(&m_cmdbuf[m_cmdbuf_pos-2], "~#", 2)==0){ // ��O�I�ɑ�����
				SendDataToReadBuffer(cmwacom_ModelData, sizeof(cmwacom_ModelData));
				//if(m_wait < WACOM_BUFFER) m_wait += sizeof(cmwacom_ModelData)*2;
				m_wait = sizeof(cmwacom_ModelData);
				m_cmdbuf_pos = 0;
			}
		}
	}
	return 0;
}

/**
 * �X�e�[�^�X�𓾂�
 * @return �X�e�[�^�X
 */
UINT8 CComWacom::GetStat()
{
	//if(m_sBuffer_wpos != m_sBuffer_rpos){
	//	if(!m_dcdflag){
	//		m_dcdflag = true;
	//		return 0x20;
	//	}else{
	//		return 0x00;
	//	}
	//}else{
		return 0x20;
	//}
}

/**
 * ���b�Z�[�W
 * @param[in] nMessage ���b�Z�[�W
 * @param[in] nParam �p�����^
 * @return ���U���g �R�[�h
 */
INTPTR CComWacom::Message(UINT nMessage, INTPTR nParam)
{
	return 0;
}

#endif