/**
 * @file	pmbase.cpp
 * @brief	印刷基底クラスの動作の実装を行います
 */

#include "compiler.h"
#include "pmbase.h"

/**
 * コンストラクタ
 */
CPrintBase::CPrintBase()
{
	// nothing to do
}

/**
 * デストラクタ
 */
CPrintBase::~CPrintBase()
{
	// nothing to do
}

void CPrintBase::StartPrint(HDC hdc, int offsetXPixel, int offsetYPixel, int widthPixel, int heightPixel, float dpiX, float dpiY, float dotscale, bool rectdot)
{
	m_hdc = hdc;
	m_offsetXPixel = offsetXPixel;
	m_offsetYPixel = offsetYPixel;
	m_widthPixel = widthPixel;
	m_heightPixel = heightPixel;
	m_dpiX = dpiX;
	m_dpiY = dpiY;
	m_dotscale = dotscale;
	m_rectdot = rectdot;
}

void CPrintBase::EndPrint()
{
	// nothing to do
}

bool CPrintBase::Write(UINT8 data)
{
	// nothing to do
	return false;
}

PRINT_COMMAND_RESULT CPrintBase::DoCommand()
{
	// nothing to do
	return PRINT_COMMAND_RESULT_OK;
}

bool CPrintBase::HasRenderingCommand()
{
	// nothing to do
	return false;
}
