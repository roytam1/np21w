/**
 * @file	pmescp.h
 * @brief	ESC/P系印刷クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include "pmbase.h"
#include "cmdparser.h"

/**
 * @brief ESC/P系印刷クラス
 */
class CPrintESCP : public CPrintBase
{
public:
	CPrintESCP();
	virtual ~CPrintESCP();

	virtual void StartPrint(HDC hdc, int offsetXPixel, int offsetYPixel, int widthPixel, int heightPixel, float dpiX, float dpiY, float dotscale, bool rectdot);
	
	virtual void EndPrint();

	virtual bool Write(UINT8 data);

	virtual PRINT_COMMAND_RESULT DoCommand();

	virtual bool HasRenderingCommand();
protected:
};
