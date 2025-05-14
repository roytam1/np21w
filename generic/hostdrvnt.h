/**
 * @file	hostdrvnt.h
 * @brief	Interface of host drive for Windows NT
 */

#pragma once

#if defined(SUPPORT_HOSTDRVNT)

#include "statsave.h"

#define NP2HOSTDRVNT_FILES_MAX	2048

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

	UINT32 deleteOnClose;
} NP2HOSTDRVNT_FILEINFO;

typedef struct
{
	int version; // バージョン
	int cmdBaseVersion; // I/Oコマンド基本バージョン
	int cmdInvokePos; // I/Oコマンド文字列の位置
	UINT32 dataAddr; // I/Oコマンドのデータメモリアドレス
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