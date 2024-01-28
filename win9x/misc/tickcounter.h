/**
 * @file	tickcounter.h
 * @brief	TICK �J�E���^�̐錾����уC���^�[�t�F�C�X�̒�`�����܂�
 */

#pragma once

enum {
	TCMODE_DEFAULT = 0,
	TCMODE_GETTICKCOUNT = 1,
	TCMODE_TIMEGETTIME = 2,
	TCMODE_PERFORMANCECOUNTER = 3,
};

#ifdef __cplusplus
extern "C"
{
#endif	// __cplusplus

DWORD GetTickCounter();
void SetTickCounterMode(int mode);

#ifdef __cplusplus
}
#endif	// __cplusplus
