/**
 * @file	hostdrvnt.h
 * @brief	Interface of host drive for Windows NT
 */

#pragma once

#if defined(SUPPORT_HOSTDRVNT)

#include "statsave.h"

#define NP2HOSTDRVNT_FILES_MAX	65536

typedef struct
{
	WCHAR* fileName;
	WCHAR* hostFileName;
	UINT8 isRoot;
	UINT8 isDirectory;
	HANDLE hFindFile;
	HANDLE hFile;

	UINT32 hostdrvWinAPIDesiredAccess;
	UINT32 hostdrvShareAccess;
	UINT32 hostdrvWinAPICreateDisposition;
	UINT32 hostdrvFileAttributes;

	UINT8 deleteOnClose;
	UINT8 allowDeleteChild;
	UINT16 extendLength; // �㑱�̊g���̈�̒���
} NP2HOSTDRVNT_FILEINFO;

typedef struct
{
	int version; // �o�[�W����
	int cmdBaseVersion; // I/O�R�}���h��{�o�[�W����
	int cmdInvokePos; // I/O�R�}���h������̈ʒu
	UINT32 dataAddr; // I/O�R�}���h�̃f�[�^�������A�h���X
	NP2HOSTDRVNT_FILEINFO files[NP2HOSTDRVNT_FILES_MAX];
} HOSTDRVNT;

#ifdef __cplusplus
extern "C" {
#endif

	extern	HOSTDRVNT		hostdrvNT;

	void hostdrvNT_initialize(void);
	void hostdrvNT_deinitialize(void);
	void hostdrvNT_reset(void);
	void hostdrvNT_bind(void);

	void hostdrvNT_updateHDrvRoot(void);

	int hostdrvNT_sfsave(STFLAGH sfh, const SFENTRY* tbl);
	int hostdrvNT_sfload(STFLAGH sfh, const SFENTRY* tbl);

#ifdef __cplusplus
}
#endif

#endif