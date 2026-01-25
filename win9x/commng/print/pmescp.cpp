/**
 * @file	pmescp.cpp
 * @brief	ESC/P系印刷クラスの動作の実装を行います
 */

#include "compiler.h"
#include "pmescp.h"

/**
 * コンストラクタ
 */
CPrintESCP::CPrintESCP()
	: CPrintBase()
{
	// nothing to do
}

/**
 * デストラクタ
 */
CPrintESCP::~CPrintESCP()
{
	// nothing to do
}

void CPrintESCP::StartPrint(HDC hdc, int offsetXPixel, int offsetYPixel, int widthPixel, int heightPixel, float dpiX, float dpiY, float dotscale, bool rectdot)
{
	CPrintBase::StartPrint(hdc, offsetXPixel, offsetYPixel, widthPixel, heightPixel, dpiX, dpiY, dotscale, rectdot);
}

void CPrintESCP::EndPrint()
{
	CPrintBase::EndPrint();
}

bool CPrintESCP::Write(UINT8 data)
{
	return false;
}

PRINT_COMMAND_RESULT CPrintESCP::DoCommand()
{
	return CPrintBase::DoCommand();
}

bool CPrintESCP::HasRenderingCommand()
{
	return CPrintBase::HasRenderingCommand();
}
