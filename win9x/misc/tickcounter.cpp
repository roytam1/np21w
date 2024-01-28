/**
 * @file	tickcounter.cpp
 * @brief	TICK �J�E���^�̓���̒�`���s���܂�
 */

#include "compiler.h"
#include "tickcounter.h"

/**
 * @brief TICK �J�E���^�[ �N���X
 */
class TickCounter
{
public:
	TickCounter();
	DWORD Get();
	void SetMode(int mode);

private:
	LARGE_INTEGER m_nFreq;		//!< ���g��
	LARGE_INTEGER m_nLast;		//!< �Ō�̃J�E���^
	DWORD m_dwLastTick;			//!< �Ō�� TICK
	int m_mode;					//!< �J�E���^���[�h
};

/**
 * �R���X�g���N�^
 */
TickCounter::TickCounter()
{
	m_nFreq.QuadPart = 0;
	::QueryPerformanceFrequency(&m_nFreq);
	if (m_nFreq.QuadPart)
	{
		::QueryPerformanceCounter(&m_nLast);
		m_mode = TCMODE_PERFORMANCECOUNTER;
	}
	else
	{
		m_mode = TCMODE_TIMEGETTIME;
	}
	m_dwLastTick = ::timeGetTime();
	//m_dwLastTick = ::GetTickCount();
}

/**
 * TICK �𓾂�
 * @return TICK
 */
DWORD TickCounter::Get()
{
	switch(m_mode)
	{
	case TCMODE_GETTICKCOUNT:
		return ::GetTickCount();

	case TCMODE_TIMEGETTIME:
		return ::timeGetTime();

	case TCMODE_PERFORMANCECOUNTER:
		{
			LARGE_INTEGER nNow;
			::QueryPerformanceCounter(&nNow);
			const ULONGLONG nPast = nNow.QuadPart - m_nLast.QuadPart;

			const DWORD dwTick = static_cast<DWORD>((nPast * 1000U) / m_nFreq.QuadPart);
			const DWORD dwRet = m_dwLastTick + dwTick;
			if (dwTick >= 1000)
			{
				const DWORD dwSeconds = dwTick / 1000;
				m_nLast.QuadPart += m_nFreq.QuadPart * dwSeconds;
				m_dwLastTick += dwSeconds * 1000;
			}
			return dwRet;
		}
	}
	return ::GetTickCount();
}

/**
 * ���[�h�����ݒ�
 */
void TickCounter::SetMode(int mode)
{
	if (mode==TCMODE_DEFAULT)
	{
		mode = TCMODE_PERFORMANCECOUNTER;
	}
	if (mode==TCMODE_PERFORMANCECOUNTER && m_nFreq.QuadPart==0)
	{
		mode = TCMODE_TIMEGETTIME;
	}
	switch(mode)
	{
	case TCMODE_GETTICKCOUNT:
		m_mode = mode;
		m_dwLastTick = ::GetTickCount();
		break;

	case TCMODE_TIMEGETTIME:
		m_mode = mode;
		m_dwLastTick = ::timeGetTime();
		break;

	case TCMODE_PERFORMANCECOUNTER:
		m_mode = mode;
		::QueryPerformanceCounter(&m_nLast);
		break;
	}
}


// ---- C �C���^�t�F�C�X

//! �J�E���^ �C���X�^���X
static TickCounter s_tick;

/**
 * �J�E���^�𓾂�
 * @return TICK
 */
DWORD GetTickCounter()
{
	return s_tick.Get();
}

/**
 * �J�E���^�𓾂�
 * @return TICK
 */
void SetTickCounterMode(int mode)
{
	s_tick.SetMode(mode);
}