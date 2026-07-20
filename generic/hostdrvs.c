/**
 * @file	hostdrvs.c
 * @brief	Implementation of host-drive
 */

#include "compiler.h"
#include "hostdrvs.h"

#if defined(SUPPORT_HOSTDRV)

#if defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
#include "oemtext.h"
#endif
#include "pccore.h"

#include <shlwapi.h>

/* 性能上最適化で優先しない方がいいコードなのでわざと別セグメントに置く */
/* #pragma code_seg(".MISCCODE") */

/*! ルート情報 */
static const HDRVFILE s_hddroot = {{' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '}, 0, 0, 0x10, {0}, {0}};

/*! 自分 */
static const char s_self[11] = {'.',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};

/*! 親 */
static const char s_parent[11] = {'.','.',' ',' ',' ',' ',' ',' ',' ',' ',' '};

static void GetHostRootPath(OEMCHAR *lpPath, UINT cchPath);

/*! DOSで許可されるキャラクタ */
static const UINT8 s_cDosCharacters[] =
{
	0xfa, 0x23,		/* '&%$#"!  /.-,+*)( */
	0xff, 0x03,		/* 76543210 ?>=<;:98 */
	0xff, 0xff,		/* GFEDCBA@ ONMLKJIH */
	0xff, 0xef,		/* WVUTSRQP _^]\[ZYX */
	0x01, 0x00,		/* gfedcba` onmlkjih */
	0x00, 0x40		/* wvutsrqp ~}|{zyx  */
};

static UINT s_nShortNameMode = HOSTDRV_SHORTNAME_DEFAULT;

/**
 * Short File Nameのつけ方ルール設定
 */
void hostdrvs_setshortnamemode(UINT nMode)
{
	switch (nMode)
	{
		case HOSTDRV_SHORTNAME_LEGACY:
		case HOSTDRV_SHORTNAME_TILDE:
			s_nShortNameMode = nMode;
			break;

		default:
			s_nShortNameMode = HOSTDRV_SHORTNAME_DEFAULT;
			break;
	}
}

/**
 * Short File Nameのつけ方ルール取得
 */
UINT hostdrvs_getshortnamemode(void)
{
	return s_nShortNameMode;
}

/**
 * パスを FCB に変換
 * @param[out] lpFcbname FCB
 * @param[in] cchFcbname FCB バッファ サイズ
 * @param[in] lpPath パス
 */
static void RealPath2FcbSub(char *lpFcbname, UINT cchFcbname, const char *lpPath)
{
	REG8 c;

	while (cchFcbname)
	{
		c = (UINT8)*lpPath++;
		if (c == 0)
		{
			break;
		}
#if defined(OSLANG_SJIS) || defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
		if ((((c ^ 0x20) - 0xa1) & 0xff) < 0x3c)
		{
			if (lpPath[0] == '\0')
			{
				break;
			}
			if (cchFcbname < 2)
			{
				break;
			}
			lpFcbname[0] = c;
			lpFcbname[1] = *lpPath++;
			lpFcbname += 2;
			cchFcbname -= 2;
		}
		else if (((c - 0x20) & 0xff) < 0x60)
		{
			if (((c - 'a') & 0xff) < 26)
			{
				c -= 0x20;
			}
			if (s_cDosCharacters[(c >> 3) - (0x20 >> 3)] & (1 << (c & 7)))
			{
				*lpFcbname++ = c;
				cchFcbname--;
			}
		}
		else if (((c - 0xa0) & 0xff) < 0x40)
		{
			*lpFcbname++ = c;
			cchFcbname--;
		}
#else
		if (((c - 0x20) & 0xff) < 0x60)
		{
			if (((c - 'a') & 0xff) < 26)
			{
				c -= 0x20;
			}
			if (s_cDosCharacters[(c >> 3) - (0x20 >> 3)] & (1 << (c & 7)))
			{
				*lpFcbname++ = c;
				cchFcbname--;
			}
		}
		else if (c >= 0x80)
		{
			*lpFcbname++ = c;
			cchFcbname--;
		}
#endif
	}
}

/**
 * パスを FCB に変換
 * @param[out] lpFcbname FCB
 * @param[in] lpPath パス
 */
static void RealName2Fcb(char *lpFcbname, const OEMCHAR *lpPath)
{
	OEMCHAR	*ext;
#if defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
	char sjis[MAX_PATH];
#endif
	OEMCHAR szFilename[MAX_PATH];

	FillMemory(lpFcbname, 11, ' ');

	ext = file_getext(lpPath);
#if defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
	oemtext_oemtosjis(sjis, NELEMENTS(sjis), ext, (UINT)-1);
	RealPath2FcbSub(lpFcbname + 8, 3, sjis);
#else
	RealPath2FcbSub(lpFcbname + 8, 3, ext);
#endif

	file_cpyname(szFilename, lpPath, NELEMENTS(szFilename));
	file_cutext(szFilename);
#if defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
	oemtext_oemtosjis(sjis, NELEMENTS(sjis), szFilename, (UINT)-1);
	RealPath2FcbSub(lpFcbname + 0, 8, sjis);
#else
	RealPath2FcbSub(lpFcbname + 0, 8, szFilename);
#endif
}

/**
 * FCB名をDOS表示名へ変換
 */
static void Fcb2DosName(char *lpDosName, const char *lpFcbname)
{
	char *p;
	UINT i;

	p = lpDosName;
	for (i = 0; (i < 8) && (lpFcbname[i] != ' '); i++)
	{
		*p++ = lpFcbname[i];
	}
	if (lpFcbname[8] != ' ')
	{
		*p++ = '.';
		for (i = 8; (i < 11) && (lpFcbname[i] != ' '); i++)
		{
			*p++ = lpFcbname[i];
		}
	}
	*p = '\0';
}

/**
 * FCB名をOEMCHAR変換.
 */
static void Fcb2OemName(OEMCHAR *lpOemName, UINT cchOemName, const char *lpFcbname)
{
	char szDosName[16];

	Fcb2DosName(szDosName, lpFcbname);
#if defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
	oemtext_sjistooem(lpOemName, cchOemName, szDosName, (UINT)-1);
#else
	file_cpyname(lpOemName, szDosName, cchOemName);
#endif
}

/**
 * ホストのファイル名がShort File Name互換ならTrue
 */
static BOOL IsExact83Name(const OEMCHAR *lpFilename, const char *lpFcbname)
{
	OEMCHAR szRoundTrip[64];

	Fcb2OemName(szRoundTrip, NELEMENTS(szRoundTrip), lpFcbname);
	return (_tcsicmp(szRoundTrip, lpFilename) == 0);
}

/**
 * FCB 名が一致するか?
 * @param[in] phdf ファイル情報
 * @param[in] lpMask マスク
 * @param[in] nAttr アトリビュート マスク
 * @retval TRUE 一致
 * @retval FALSE 不一致
 */
static BOOL IsMatchFcb(const HDRVFILE *phdf, const char *lpMask, UINT nAttr)
{
	UINT i;

	if ((phdf->attr & (~nAttr)) & 0x16)
	{
		return FALSE;
	}
	if (lpMask != NULL)
	{
		for (i = 0; i < 11; i++)
		{
			if ((phdf->fcbname[i] != lpMask[i]) && (lpMask[i] != '?'))
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

/**
 * FCB 名が一致するか
 * @param[in] vpItem アイテム
 * @param[in] vpArg ユーザ引数
 * @retval TRUE 一致
 * @retval FALSE 不一致
 */
static BOOL IsMatchName(void *vpItem, void *vpArg)
{
	return IsMatchFcb((HDRVFILE *)vpItem, (char *)vpArg, 0x16);
}

static BOOL IsFcbUsed(LISTARRAY lst, const char *lpFcbname)
{
	return (listarray_enum(lst, IsMatchName, (void *)lpFcbname) != NULL);
}

static BOOL IsSjisLeadByte(UINT8 c)
{
#if defined(OSLANG_SJIS) || defined(OSLANG_EUC) || defined(OSLANG_UTF8) || defined(OSLANG_UCS2)
	return ((((c ^ 0x20) - 0xa1) & 0xff) < 0x3c);
#else
	(void)c;
	return FALSE;
#endif
}

static UINT DecimalDigits(UINT32 nValue)
{
	UINT nDigits;

	nDigits = 1;
	while (nValue >= 10)
	{
		nValue /= 10;
		nDigits++;
	}
	return nDigits;
}

static void UIntToDecimal(char *lpBuffer, UINT32 nValue, UINT nDigits)
{
	UINT i;

	for (i = 0; i < nDigits; i++)
	{
		lpBuffer[nDigits - i - 1] = (char)('0' + (nValue % 10));
		nValue /= 10;
	}
	lpBuffer[nDigits] = '\0';
}

/**
 * DOS互換名のベースになる部分を取得　SJISも考慮
 */
static UINT CopyStemPrefix(char *lpDst, UINT nLimit, const char *lpLegacy)
{
	UINT src;
	UINT dst;

	src = 0;
	dst = 0;
	while ((src < 8) && (lpLegacy[src] != ' ') && (dst < nLimit))
	{
		UINT nCharSize;

		nCharSize = 1;
		if (IsSjisLeadByte((UINT8)lpLegacy[src]) &&
			(src + 1 < 8) && (lpLegacy[src + 1] != ' '))
		{
			nCharSize = 2;
		}
		if (dst + nCharSize > nLimit)
		{
			break;
		}
		memcpy(lpDst + dst, lpLegacy + src, nCharSize);
		src += nCharSize;
		dst += nCharSize;
	}
	return dst;
}

/**
 * チルダ番号で保護したSFNを生成する
 */
static BOOL MakeTildeFcb(char *lpFcbname, const OEMCHAR *lpFilename, LISTARRAY used)
{
	char legacy[11];
	char digits[16];
	UINT32 nNumber;

	RealName2Fcb(legacy, lpFilename);
	for (nNumber = 1; nNumber <= 9999999UL; nNumber++)
	{
		UINT nDigits;
		UINT nPrefixLimit;
		UINT nPos;

		nDigits = DecimalDigits(nNumber);
		if (nDigits >= 8)
		{
			break;
		}
		nPrefixLimit = 8 - nDigits - 1;

		FillMemory(lpFcbname, 11, ' ');
		nPos = CopyStemPrefix(lpFcbname, nPrefixLimit, legacy);
		if (nPos == 0)
		{
			// 1文字も有効でないときは適当に"FILE"とする
			static const char s_fallback[] = "FILE";
			UINT nFallback;

			nFallback = min(nPrefixLimit, (UINT)(sizeof(s_fallback) - 1));
			memcpy(lpFcbname, s_fallback, nFallback);
			nPos = nFallback;
		}
		lpFcbname[nPos++] = '~';
		UIntToDecimal(digits, nNumber, nDigits);
		memcpy(lpFcbname + nPos, digits, nDigits);
		memcpy(lpFcbname + 8, legacy + 8, 3);

		if (!IsFcbUsed(used, lpFcbname))
		{
			return TRUE;
		}
	}
	return FALSE;
}


static int CompareRawByName(const void *vp1, const void *vp2)
{
	const HDRVSFNENTRY *p1;
	const HDRVSFNENTRY *p2;
	int r;

	p1 = (const HDRVSFNENTRY *)vp1;
	p2 = (const HDRVSFNENTRY *)vp2;
	r = _tcsicmp(p1->szFilename, p2->szFilename);
	if (r == 0)
	{
		r = _tcscmp(p1->szFilename, p2->szFilename);
	}
	if (r == 0)
	{
		if (p1->nOrder < p2->nOrder)
		{
			r = -1;
		}
		else if (p1->nOrder > p2->nOrder)
		{
			r = 1;
		}
	}
	return r;
}

static int CompareRawByOrder(const void *vp1, const void *vp2)
{
	const HDRVSFNENTRY *p1;
	const HDRVSFNENTRY *p2;

	p1 = (const HDRVSFNENTRY *)vp1;
	p2 = (const HDRVSFNENTRY *)vp2;
	if (p1->nOrder < p2->nOrder)
	{
		return -1;
	}
	if (p1->nOrder > p2->nOrder)
	{
		return 1;
	}
	return 0;
}

static BRESULT AddFileToLists(LISTARRAY ret, LISTARRAY used,
							  const HDRVFILE *phdf, const OEMCHAR *lpFilename,
							  const char *lpMask, UINT nAttr, BOOL bVisible)
{
	HDRVLST hdd;

	if (listarray_append(used, phdf) == NULL)
	{
		return FAILURE;
	}
	if (bVisible && IsMatchFcb(phdf, lpMask, nAttr))
	{
		hdd = (HDRVLST)listarray_append(ret, NULL);
		if (hdd == NULL)
		{
			return FAILURE;
		}
		hdd->file = *phdf;
		file_cpyname(hdd->szFilename, lpFilename, NELEMENTS(hdd->szFilename));
	}
	return SUCCESS;
}

static BOOL IsUsableMappedFcb(const char *lpFcbname, LISTARRAY used)
{
	OEMCHAR szName[32];
	char roundTrip[11];

	if ((lpFcbname == NULL) || (lpFcbname[0] == ' '))
	{
		return FALSE;
	}
	Fcb2OemName(szName, NELEMENTS(szName), lpFcbname);
	RealName2Fcb(roundTrip, szName);
	if (memcmp(roundTrip, lpFcbname, 11) != 0)
	{
		return FALSE;
	}
	return !IsFcbUsed(used, lpFcbname);
}

static BRESULT AppendUsedName(LISTARRAY used, HDRVSFNENTRY *pEntry,
							  const char *lpFcbname)
{
	memcpy(pEntry->file.fcbname, lpFcbname, 11);
	if (listarray_append(used, &pEntry->file) == NULL)
	{
		return FAILURE;
	}
	pEntry->bAssigned = TRUE;
	return SUCCESS;
}

static BRESULT GatherRawEntries(const OEMCHAR *lpPath, HDRVSFNENTRY **ppEntries,
								UINT *pnEntries, UINT *pnCapacity)
{
	FLISTH flh;
	FLINFO fli;

	flh = file_list1st(lpPath, &fli);
	if (flh == FLISTH_INVALID)
	{
		return SUCCESS;
	}
	do
	{
		HDRVSFNENTRY *p;
		HDRVSFNENTRY *pNew;
		UINT nCapacity;
		OEMCHAR szEntryPath[MAX_PATH];

		// .と..は無視
		if ((_tcscmp(fli.path, OEMTEXT(".")) == 0) || (_tcscmp(fli.path, OEMTEXT("..")) == 0))
		{
			continue;
		}

		// シンボリックリンクも不可
		file_cpyname(szEntryPath, lpPath, NELEMENTS(szEntryPath));
		file_setseparator(szEntryPath, NELEMENTS(szEntryPath));
		file_catname(szEntryPath, fli.path, NELEMENTS(szEntryPath));
		if (file_islink(szEntryPath))
		{
			continue;
		}

		// SFNマップ格納領域が足りなければ拡張
		if (*pnEntries >= *pnCapacity)
		{
			nCapacity = (*pnCapacity == 0) ? 64 : *pnCapacity * 2;
			pNew = (HDRVSFNENTRY *)realloc(*ppEntries,
										 nCapacity * sizeof(HDRVSFNENTRY));
			if (pNew == NULL)
			{
				file_listclose(flh);
				return FAILURE;
			}
			*ppEntries = pNew;
			*pnCapacity = nCapacity;
		}

		// SFN登録
		p = &(*ppEntries)[*pnEntries];
		ZeroMemory(p, sizeof(*p));
		p->file.caps = fli.caps;
		p->file.size = fli.size;
		p->file.attr = fli.attr;
		p->file.date = fli.date;
		p->file.time = fli.time;
		file_cpyname(p->szFilename, fli.path, NELEMENTS(p->szFilename));
		file_cpyname(p->szShortFilename, fli.shortpath,
					 NELEMENTS(p->szShortFilename));
		if (p->szShortFilename[0] == '\0')
		{
			OEMCHAR szFullPath[MAX_PATH];

			file_cpyname(szFullPath, lpPath, NELEMENTS(szFullPath));
			file_setseparator(szFullPath, NELEMENTS(szFullPath));
			file_catname(szFullPath, fli.path, NELEMENTS(szFullPath));
			file_getshortname(szFullPath, p->szShortFilename,
							  NELEMENTS(p->szShortFilename));
		}

		p->nOrder = *pnEntries;
		(*pnEntries)++;
	} while (file_listnext(flh, &fli) == SUCCESS);
	file_listclose(flh);
	return SUCCESS;
}

static BRESULT AssignHostAndExactNames(HDRVSFNENTRY *pEntries, UINT nEntries,
									   LISTARRAY used)
{
	UINT i;
	char candidate[11];

	/* Host OS aliases are authoritative when the file system supplies one. */
	for (i = 0; i < nEntries; i++)
	{
		HDRVSFNENTRY *p;

		p = &pEntries[i];
		if (p->szShortFilename[0] != '\0')
		{
			RealName2Fcb(candidate, p->szShortFilename);
			if (IsExact83Name(p->szShortFilename, candidate) &&
				IsUsableMappedFcb(candidate, used))
			{
				if (AppendUsedName(used, p, candidate) != SUCCESS)
				{
					return FAILURE;
				}
			}
		}
	}

	/* Preserve real names that already are valid, unique 8.3 names. */
	for (i = 0; i < nEntries; i++)
	{
		HDRVSFNENTRY *p;

		p = &pEntries[i];
		if (!p->bAssigned)
		{
			RealName2Fcb(candidate, p->szFilename);
			if (IsExact83Name(p->szFilename, candidate) &&
				IsUsableMappedFcb(candidate, used))
			{
				if (AppendUsedName(used, p, candidate) != SUCCESS)
				{
					return FAILURE;
				}
			}
		}
	}
	return SUCCESS;
}

static BRESULT AssignGeneratedNames(HDRVSFNENTRY *pEntries, UINT nEntries,
									LISTARRAY used)
{
	UINT i;
	char candidate[11];

	for (i = 0; i < nEntries; i++)
	{
		HDRVSFNENTRY *p;

		p = &pEntries[i];
		if (p->bAssigned)
		{
			continue;
		}
		if (!MakeTildeFcb(candidate, p->szFilename, used))
		{
			return FAILURE;
		}
		if (AppendUsedName(used, p, candidate) != SUCCESS)
		{
			return FAILURE;
		}
	}
	return SUCCESS;
}

static BRESULT AssignLegacyNames(HDRVSFNENTRY *pEntries, UINT nEntries, LISTARRAY used)
{
	UINT i;
	char candidate[11];

	for (i = 0; i < nEntries; i++)
	{
		HDRVSFNENTRY *p;

		p = &pEntries[i];
		RealName2Fcb(candidate, p->szFilename);
		if ((candidate[0] == ' ') || IsFcbUsed(used, candidate))
		{
			continue;
		}
		if (AppendUsedName(used, p, candidate) != SUCCESS)
		{
			return FAILURE;
		}
	}
	return SUCCESS;
}

/// <summary>
/// 短いファイル名のマップを作る
/// s_nShortNameModeがHOSTDRV_SHORTNAME_LEGACYの場合、旧np2互換の単純切り捨て重複無視での生成
/// s_nShortNameModeがHOSTDRV_SHORTNAME_TILDEの場合、Windows標準のチルダ番号方式での生成
/// </summary>
/// <param name="lpPath">SFNマップを作るパス</param>
/// <param name="ppEntries">SFNマップ</param>
/// <param name="pnEntries">SFNマップエントリ数</param>
/// <returns></returns>
BRESULT hostdrvs_getshortnamemap(const OEMCHAR *lpPath, HDRVSFNENTRY **ppEntries, UINT *pnEntries)
{
	HDRVSFNENTRY *entries;
	UINT nEntries;
	UINT nCapacity;
	LISTARRAY used;
	BRESULT r;

	// パスがNULLは不可
	if ((lpPath == NULL) || (ppEntries == NULL) || (pnEntries == NULL))
	{
		return FAILURE;
	}
	*ppEntries = NULL;
	*pnEntries = 0;
	entries = NULL;
	nEntries = 0;
	nCapacity = 0;

	// 
	if (GatherRawEntries(lpPath, &entries, &nEntries, &nCapacity) != SUCCESS)
	{
		free(entries);
		return FAILURE;
	}
	if (nEntries == 0)
	{
		free(entries);
		return SUCCESS;
	}

	used = listarray_new(sizeof(HDRVFILE), 64);
	if (used == NULL)
	{
		free(entries);
		return FAILURE;
	}
	{
		HDRVFILE special;

		ZeroMemory(&special, sizeof(special));
		memcpy(special.fcbname, s_self, 11);
		if (listarray_append(used, &special) == NULL)
		{
			listarray_destroy(used);
			free(entries);
			return FAILURE;
		}
		memcpy(special.fcbname, s_parent, 11);
		if (listarray_append(used, &special) == NULL)
		{
			listarray_destroy(used);
			free(entries);
			return FAILURE;
		}
	}

	if (s_nShortNameMode == HOSTDRV_SHORTNAME_LEGACY)
	{
		r = AssignLegacyNames(entries, nEntries, used);
	}
	else
	{
		qsort(entries, nEntries, sizeof(HDRVSFNENTRY), CompareRawByName);
		r = AssignHostAndExactNames(entries, nEntries, used);
		if (r == SUCCESS)
		{
			r = AssignGeneratedNames(entries, nEntries, used);
		}
	}

	listarray_destroy(used);
	if (r != SUCCESS)
	{
		free(entries);
		return FAILURE;
	}

	qsort(entries, nEntries, sizeof(HDRVSFNENTRY), CompareRawByOrder);
	*ppEntries = entries;
	*pnEntries = nEntries;
	return SUCCESS;
}

void hostdrvs_freeshortnamemap(HDRVSFNENTRY *pEntries)
{
	free(pEntries);
}

BOOL hostdrvs_lookupshortname(const HDRVSFNENTRY *pEntries, UINT nEntries,
							  const OEMCHAR *lpFilename, OEMCHAR *lpShortName, UINT cchShortName)
{
	UINT i;

	if ((lpFilename == NULL) || (lpShortName == NULL) || (cchShortName == 0))
	{
		return FALSE;
	}
	for (i = 0; i < nEntries; i++)
	{
		if (pEntries[i].bAssigned && (_tcsicmp(pEntries[i].szFilename, lpFilename) == 0))
		{
			Fcb2OemName(lpShortName, cchShortName, pEntries[i].file.fcbname);
			return TRUE;
		}
	}
	lpShortName[0] = '\0';
	return FALSE;
}

BOOL hostdrvs_lookuplongname(const HDRVSFNENTRY *pEntries, UINT nEntries,
							 const OEMCHAR *lpShortName, OEMCHAR *lpFilename, UINT cchFilename,
							 UINT32 *lpAttr)
{
	UINT i;
	OEMCHAR szCandidate[16];

	if ((lpShortName == NULL) || (lpFilename == NULL) || (cchFilename == 0))
	{
		return FALSE;
	}
	for (i = 0; i < nEntries; i++)
	{
		if (!pEntries[i].bAssigned)
		{
			continue;
		}
		Fcb2OemName(szCandidate, NELEMENTS(szCandidate), pEntries[i].file.fcbname);
		if (_tcsicmp(szCandidate, lpShortName) == 0)
		{
			file_cpyname(lpFilename, pEntries[i].szFilename, cchFilename);
			if (lpAttr != NULL)
			{
				*lpAttr = pEntries[i].file.attr;
			}
			return TRUE;
		}
	}
	lpFilename[0] = '\0';
	return FALSE;
}

/// <summary>
/// 与えられたパスがHOSTDRVルートか
/// </summary>
static BOOL HostPathIsRoot(const OEMCHAR *lpPath)
{
	OEMCHAR root[MAX_PATH];

	if (lpPath == NULL) return FALSE;
	GetHostRootPath(root, NELEMENTS(root));
	return (_tcscmp(lpPath, root) == 0) ? TRUE : FALSE;
}

/// <summary>
/// 親ディレクトリへ移動する。HOSTDRVルートより上にはいかないようにする
/// </summary>
static BRESULT HostPathGoParent(OEMCHAR *lpPath, UINT cchPath)
{
	OEMCHAR root[MAX_PATH];
	UINT rootLen;
	UINT pathLen;

	if (lpPath == NULL || cchPath == 0) return FAILURE;
	GetHostRootPath(root, NELEMENTS(root));
	if (_tcscmp(lpPath, root) == 0) return FAILURE;

	file_cutseparator(lpPath);
	file_cutname(lpPath);
	file_cutseparator(lpPath);

	rootLen = (UINT)_tcslen(root);
	pathLen = (UINT)_tcslen(lpPath);
	if (pathLen < rootLen || _tcsncmp(lpPath, root, rootLen) != 0 ||
		(pathLen > rootLen &&
		 !(rootLen > 0 && (root[rootLen - 1] == '\\' || root[rootLen - 1] == '/')) &&
		 lpPath[rootLen] != '\\' && lpPath[rootLen] != '/'))
	{
		file_cpyname(lpPath, root, cchPath);
	}
	return SUCCESS;
}

/// <summary>
/// ファイル一覧を取得
/// </summary>
static LISTARRAY GetPathListCommon(const HDRVPATH *phdp, const char *lpMask, UINT nAttr)
{
	LISTARRAY ret;
	LISTARRAY used;
	HDRVSFNENTRY *entries;
	UINT nEntries;
	UINT i;
	HDRVFILE special;
	int isRoot;

	ret = listarray_new(sizeof(_HDRVLST), 64);
	used = listarray_new(sizeof(HDRVFILE), 64);
	entries = NULL;
	nEntries = 0;
	if ((ret == NULL) || (used == NULL))
	{
		if (ret != NULL) listarray_destroy(ret);
		if (used != NULL) listarray_destroy(used);
		return NULL;
	}

	isRoot = HostPathIsRoot(phdp->szPath);
	if (phdp->file.attr & 0x10)
	{
		special = phdp->file;
		memcpy(special.fcbname, s_self, 11);
		if (AddFileToLists(ret, used, &special, OEMTEXT("."),
						   lpMask, nAttr, !isRoot) != SUCCESS) goto memory_error;

		special = phdp->file;
		memcpy(special.fcbname, s_parent, 11);
		if (AddFileToLists(ret, used, &special, OEMTEXT(".."),
						   lpMask, nAttr, !isRoot) != SUCCESS) goto memory_error;
	}

	if (hostdrvs_getshortnamemap(phdp->szPath, &entries, &nEntries) != SUCCESS)
	{
		goto memory_error;
	}
	for (i = 0; i < nEntries; i++)
	{
		HDRVSFNENTRY *p;
		HDRVLST hdd;

		p = &entries[i];
		if (!p->bAssigned || !IsMatchFcb(&p->file, lpMask, nAttr)) continue;
		hdd = (HDRVLST)listarray_append(ret, NULL);
		if (hdd == NULL) goto memory_error;
		hdd->file = p->file;
		file_cpyname(hdd->szFilename, p->szFilename, NELEMENTS(hdd->szFilename));
	}

	hostdrvs_freeshortnamemap(entries);
	listarray_destroy(used);
	if (listarray_getitems(ret) == 0)
	{
		listarray_destroy(ret);
		ret = NULL;
	}
	return ret;

memory_error:
	hostdrvs_freeshortnamemap(entries);
	listarray_destroy(used);
	listarray_destroy(ret);
	return NULL;
}

/**
 * ファイル一覧を取得
 * @param[in] phdp パス
 * @param[in] lpMask マスク
 * @param[in] nAttr アトリビュート
 * @return ファイル一覧
 */
LISTARRAY hostdrvs_getpathlist(const HDRVPATH *phdp, const char *lpMask, UINT nAttr)
{
	return GetPathListCommon(phdp, lpMask, nAttr);
}

/* ---- */

/**
 * DOS 名を FCB に変換
 * @param[out] lpFcbname FCB
 * @param[in] cchFcbname FCB バッファ サイズ
 * @param[in] lpDosPath DOS パス
 * @return 次の DOS パス
 */
static const char *DosPath2FcbSub(char *lpFcbname, UINT cchFcbname, const char *lpDosPath)
{
	char c;

	while (cchFcbname)
	{
		c = lpDosPath[0];
		if ((c == 0) || (c == '.') || (c == '\\'))
		{
			break;
		}
		if ((((c ^ 0x20) - 0xa1) & 0xff) < 0x3c)
		{
			if (lpDosPath[1] == '\0')
			{
				break;
			}
			if (cchFcbname < 2)
			{
				break;
			}
			lpDosPath++;
			lpFcbname[0] = c;
			lpFcbname[1] = *lpDosPath;
			lpFcbname += 2;
			cchFcbname -= 2;
		}
		else
		{
			*lpFcbname++ = c;
			cchFcbname--;
		}
		lpDosPath++;
	}
	return lpDosPath;
}

/**
 * DOS 名を FCB に変換
 * @param[out] lpFcbname FCB
 * @param[in] lpDosPath DOS パス
 * @return 次の DOS パス
 */
static const char *DosPath2Fcb(char *lpFcbname, const char *lpDosPath)
{
	FillMemory(lpFcbname, 11, ' ');
	lpDosPath = DosPath2FcbSub(lpFcbname, 8, lpDosPath);
	if (lpDosPath[0] == '.')
	{
		lpDosPath = DosPath2FcbSub(lpFcbname + 8, 3, lpDosPath + 1);
	}
	return lpDosPath;
}

/**
 * パス検索
 *
 * 一覧生成と同じ短名割り当て処理を経由するため、FindFirstで見えた短名は
 * open/delete/rename/chdirでも必ず同じ実ファイルへ解決される。
 */
static BRESULT FindSinglePath(HDRVPATH *phdp, const char *lpFcbname)
{
	LISTARRAY lst;
	HDRVLST hdd;
	UINT nIndex;
	BRESULT r;

	r = FAILURE;
	lst = hostdrvs_getpathlist(phdp, lpFcbname, 0x37);
	if (lst != NULL)
	{
		nIndex = 0;
		while (TRUE)
		{
			hdd = (HDRVLST)listarray_getitem(lst, nIndex++);
			if (hdd == NULL)
			{
				break;
			}
			if (memcmp(hdd->file.fcbname, lpFcbname, 11) == 0)
			{
				// 親への移動はHOSTDRVルートより上にいかないようにする
				if (_tcscmp(hdd->szFilename, OEMTEXT(".")) == 0)
				{
					r = SUCCESS;
					break;
				}
				if (_tcscmp(hdd->szFilename, OEMTEXT("..")) == 0)
				{
					if (HostPathGoParent(phdp->szPath, NELEMENTS(phdp->szPath)) != SUCCESS)
					{
						r = FAILURE;
						break;
					}
					phdp->file = HostPathIsRoot(phdp->szPath) ? s_hddroot : hdd->file;
					r = SUCCESS;
					break;
				}

				phdp->file = hdd->file;
				file_setseparator(phdp->szPath, NELEMENTS(phdp->szPath));
				file_catname(phdp->szPath, hdd->szFilename, NELEMENTS(phdp->szPath));
				
				// シンボリックリンクは拒否
				if (file_islink(phdp->szPath))
				{
					r = FAILURE;
					break;
				}
				r = SUCCESS;
				break;
			}
		}
		listarray_destroy(lst);
	}
	return r;
}

/**
 * ディレクトリを得る
 * @param[out] phdp HostDrv パス
 * @param[out] lpFcbname FCB 名
 * @param[in] lpDosPath DOS パス
 * @return DOS エラー コード
 */
static void GetHostRootPath(OEMCHAR *lpPath, UINT cchPath)
{
	if (PathIsRelative(np2cfg.hdrvroot))
	{
		TCHAR pathbuf[MAX_PATH+1];
		TCHAR *pathtmp;
		initgetfile(pathbuf, _countof(pathbuf));
		pathtmp = _tcsrchr(pathbuf, '\\');
		if (pathtmp)
		{
			*(pathtmp+1) = 0;
		}
		else
		{
			pathbuf[0] = 0;
		}
		_tcscat(pathbuf, np2cfg.hdrvroot);
		file_cpyname(lpPath, pathbuf, cchPath);
	}
	else
	{
		file_cpyname(lpPath, np2cfg.hdrvroot, cchPath);
	}
}

BOOL hostdrvs_isroot(const HDRVPATH *phdp)
{
	return (phdp != NULL) ? HostPathIsRoot(phdp->szPath) : FALSE;
}

// パスの安全性確認 lpPathはアクセス先ホストパス
BOOL hostdrvs_issafehostpath(const OEMCHAR *lpPath)
{
	OEMCHAR root[MAX_PATH];
	OEMCHAR current[MAX_PATH];
	const OEMCHAR *p;
	UINT rootLen;

	// パスがNULLは駄目
	if (lpPath == NULL) return FALSE;
	
	// HOSTDRVルートが空なら駄目
	GetHostRootPath(root, NELEMENTS(root));
	if (root[0] == '\0') return FALSE;

	// パス先頭の一致確認
	rootLen = (UINT)_tcslen(root);
	if (_tcsncmp(lpPath, root, rootLen) != 0) return FALSE;
	if (lpPath[rootLen] != '\0' &&
		!(rootLen > 0 && (root[rootLen - 1] == '\\' || root[rootLen - 1] == '/')) &&
		lpPath[rootLen] != '\\' && lpPath[rootLen] != '/') return FALSE;

	// パス階層別に確認
	file_cpyname(current, root, NELEMENTS(current));
	p = lpPath + rootLen;
	while (*p == '\\' || *p == '/') p++;
	while (*p != '\0')
	{
		OEMCHAR component[MAX_PATH];
		UINT len = 0;
		while (p[len] != '\0' && p[len] != '\\' && p[len] != '/') len++;

		// 長すぎるものは不可
		if (len == 0 || len >= NELEMENTS(component)) return FALSE;
		memcpy(component, p, len * sizeof(OEMCHAR));
		component[len] = '\0';

		// .と..は不可
		if ((_tcscmp(component, OEMTEXT(".")) == 0) ||
			(_tcscmp(component, OEMTEXT("..")) == 0)) return FALSE;
		
		// シンボリックリンク等は不可
		file_setseparator(current, NELEMENTS(current));
		file_catname(current, component, NELEMENTS(current));
		if (file_islink(current)) return FALSE;

		p += len;
		while (*p == '\\' || *p == '/') p++;
	}
	return TRUE;
}

UINT hostdrvs_getrealdir(HDRVPATH *phdp, char *lpFcbname, const char *lpDosPath)
{
	phdp->file = s_hddroot;
	GetHostRootPath(phdp->szPath, NELEMENTS(phdp->szPath));

	if (lpDosPath[0] == '\\')
	{
		lpDosPath++;
	}
	else if (lpDosPath[0] != '\0')
	{
		return ERR_PATHNOTFOUND;
	}
	while (TRUE)
	{
		lpDosPath = DosPath2Fcb(lpFcbname, lpDosPath);
		if (lpDosPath[0] != '\\')
		{
			break;
		}
		if ((FindSinglePath(phdp, lpFcbname) != SUCCESS) || ((phdp->file.attr & 0x10) == 0))
		{
			return FAILURE;
		}
		lpDosPath++;
	}
	return (lpDosPath[0] == '\0') ? ERR_NOERROR : ERR_PATHNOTFOUND;
}

/**
 * パスを結合する
 * @param[in,out] phdp HostDrv パス
 * @param[in] lpFcbname FCB 名
 * @return DOS エラー コード
 */
UINT hostdrvs_appendname(HDRVPATH *phdp, const char *lpFcbname)
{
	OEMCHAR oemname[64];

	if (lpFcbname[0] == ' ')
	{
		return ERR_PATHNOTFOUND;
	}
	else if (FindSinglePath(phdp, lpFcbname) == SUCCESS)
	{
		return ERR_NOERROR;
	}
	else
	{
		memset(&phdp->file, 0, sizeof(phdp->file));
		memcpy(phdp->file.fcbname, lpFcbname, 11);
		file_setseparator(phdp->szPath, NELEMENTS(phdp->szPath));
		Fcb2OemName(oemname, NELEMENTS(oemname), lpFcbname);
		file_catname(phdp->szPath, oemname, NELEMENTS(phdp->szPath));
		// すでにシンボリックリンクなど特殊ファイルがある場合拒否
		if (file_islink(phdp->szPath)) return ERR_ACCESSDENIED;
		return ERR_FILENOTFOUND;
	}
}

/**
 * パスを得る
 * @param[out] phdp HostDrv パス
 * @param[in] lpDosPath DOS パス
 * @return DOS エラー コード
 */
UINT hostdrvs_getrealpath(HDRVPATH *phdp, const char *lpDosPath)
{
	char fcbname[11];
	UINT nResult;

	if (lpDosPath[0] == '\0' || (lpDosPath[0] == '\\' && lpDosPath[1] == '\0'))
	{
		phdp->file = s_hddroot;
		GetHostRootPath(phdp->szPath, NELEMENTS(phdp->szPath));
		return ERR_NOERROR;
	}
	nResult = hostdrvs_getrealdir(phdp, fcbname, lpDosPath);
	if (nResult == ERR_NOERROR)
	{
		nResult = hostdrvs_appendname(phdp, fcbname);
	}
	return nResult;
}

/* ---- */

/**
 * ファイルハンドルをクローズするコールバック
 * @param[in] vpItem アイテム
 * @param[in] vpArg ユーザ引数
 * @retval FALSE 継続
 */
static BOOL CloseFileHandle(void *vpItem, void *vpArg)
{
	INTPTR fh;

	fh = ((HDRVHANDLE)vpItem)->hdl;
	if (fh != (INTPTR)FILEH_INVALID)
	{
		((HDRVHANDLE)vpItem)->hdl = (INTPTR)FILEH_INVALID;
		file_close((FILEH)fh);
	}
	(void)vpArg;
	return FALSE;
}

/**
 * すべてクローズ
 * @param[in] fileArray ファイル リスト ハンドル
 */
void hostdrvs_fhdlallclose(LISTARRAY fileArray)
{
	listarray_enum(fileArray, CloseFileHandle, NULL);
}

/**
 * 空ハンドルを見つけるコールバック
 * @param[in] vpItem アイテム
 * @param[in] vpArg ユーザ引数
 * @retval TRUE 見つかった
 * @retval FALSE 見つからなかった
 */
static BOOL IsHandleInvalid(void *vpItem, void *vpArg)
{
	if (((HDRVHANDLE)vpItem)->hdl == (INTPTR)FILEH_INVALID)
	{
		return TRUE;
	}
	(void)vpArg;
	return FALSE;
}

/**
 * 新しいハンドルを得る
 * @param[in] fileArray ファイル リスト ハンドル
 * @return 新しいハンドル
 */
HDRVHANDLE hostdrvs_fhdlsea(LISTARRAY fileArray)
{
	HDRVHANDLE ret;

	if (fileArray == NULL)
	{
		TRACEOUT(("hostdrvs_fhdlsea hdl == NULL"));
	}
	ret = (HDRVHANDLE)listarray_enum(fileArray, IsHandleInvalid, NULL);
	if (ret == NULL)
	{
		ret = (HDRVHANDLE)listarray_append(fileArray, NULL);
		if (ret != NULL)
		{
			ret->hdl = (INTPTR)FILEH_INVALID;
		}
	}
	return ret;
}

/* #pragma code_seg() */

#endif
