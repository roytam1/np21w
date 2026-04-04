/**
 * @file	npdisp_mem.c
 * @brief	Implementation of the Neko Project II Display Adapter Memory Helper
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>

#include	"pccore.h"
#include	"cpucore.h"

#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_mem.h"
#include	"npdisp_palette.h"
#include	"wab.h"

extern NPDISP_WINDOWS	npdispwin;

#if 0
static void trace_fmt_exF(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUTF(s)	trace_fmt_exF s
#else
#define	TRACEOUTF(s)	(void)s
#endif	/* 1 */

static std::vector<UINT8> npdisp_memread_buf; // リクエストされてから読み込み完了しているデータを表す
static UINT32 npdisp_memwrite_bufwpos = 0; // リクエストされてから書き込み完了している位置を表す

static UINT32 npdisp_memread_curpos = 0; // リクエストされてからのデータ読み取りバイト数
static UINT32 npdisp_memread_preloadcount = 0; // データプリロードバイト数
static UINT32 npdisp_memwrite_curpos = 0; // リクエストされてからのデータ書き込みバイト数

static UINT16 npdisp_selector_cache = 0; // 最後に使用したセレクタ
static UINT32 npdisp_seg_cache = 0; // 最後に使用したセレクタに対応するセグメント

static UINT32 npdisp_exception_eip = 0; // 例外発生時のEIPレジスタ

static sigjmp_buf npdisp_jmpbuf_bak; // 例外発生の捕捉用jmpbuf

static UINT32 last_npdisp_memread_bufsize;
static UINT32 last_npdisp_memwrite_bufwpos;

/// <summary>
/// 先読みバッファや例外フラグ等を全てクリアする
/// </summary>
void npdisp_memory_clearpreload()
{
	npdisp.longjmpnum = 0;
	npdisp_memwrite_bufwpos = 0;
	npdisp_memread_buf.clear();
	
	npdisp_memory_resetposition();
}
/// <summary>
/// メモリ読み書き開始位置を先頭へ戻す
/// </summary>
void npdisp_memory_resetposition()
{
	npdisp_exception_eip = CPU_EIP;
	npdisp.longjmpnum = 0;
	npdisp_memread_curpos = 0;
	npdisp_memwrite_curpos = 0;
	npdisp_memread_preloadcount = 0;
	npdisp_selector_cache = 0;
	npdisp_seg_cache = 0;

	// バッファのサイズを先に取得しておく データ読み書きが進んでいるかの確認用
	last_npdisp_memread_bufsize = npdisp_memread_buf.size();
	last_npdisp_memwrite_bufwpos = npdisp_memwrite_bufwpos;
}
/// <summary>
/// 前回のnpdisp_memory_resetposition実行時から新たなデータ読み書きがあった場合は0以外を返す
/// </summary>
int npdisp_memory_hasNewCacheData()
{
	return last_npdisp_memread_bufsize != npdisp_memread_buf.size() || last_npdisp_memwrite_bufwpos != npdisp_memwrite_bufwpos;
}
/// <summary>
/// バッファの読み取りデータサイズを返す
/// </summary>
int npdisp_memory_getTotalReadSize()
{
	return npdisp_memread_buf.size();
}
/// <summary>
/// バッファの書き込みデータサイズを返す
/// </summary>
int npdisp_memory_getTotalWriteSize()
{
	return npdisp_memwrite_bufwpos;
}
/// <summary>
/// 読み取り開始時のEIPレジスタを返す
/// </summary>
UINT32 npdisp_memory_getLastEIP()
{
	return npdisp_exception_eip;
}

/***
 * セレクタ:オフセット形式のメモリを読み取るために使う関数群
 * セレクタ→セグメント→ページング→実メモリアドレス　の流れ
 ***/

/// <summary>
/// セレクタとオフセットからリニアアドレスを計算
/// </summary>
/// <param name="selector">セレクタ</param>
/// <param name="offset">オフセット</param>
/// <param name="lplAddr">リニアアドレス</param>
/// <returns>成功したら0以外、失敗したら0</returns>
static UINT32 selector_to_linear(UINT16 selector, UINT32 offset, UINT32 *lplAddr)
{
	selector_t sel;
	int rv;

	// 高速化 前回読み取りと同じセレクタなら使い回す
	if (selector == npdisp_selector_cache) {
		*lplAddr = npdisp_seg_cache + offset;
		return 1;
	}

	memset(&sel, 0, sizeof(sel));

	// セレクタを読み取り
	rv = parse_selector(&sel, selector);
	if (rv == 0) {
		// OK
		npdisp_selector_cache = selector;
		npdisp_seg_cache = sel.desc.u.seg.segbase; // セグメントを取得
		*lplAddr = sel.desc.u.seg.segbase + offset;
		return 1;
	}
	// Fail
	return 0;
}

/// <summary>
/// 指定したリニアアドレスを読み取って先読みバッファへ送る。先にページフォールトの発生を確認するために使用。
/// </summary>
/// <param name="vaddr">リニアアドレス</param>
/// <param name="size">読み取りサイズ</param>
/// <returns>成功は0以外、ページフォールトが発生した場合は0を返す</returns>
int npdisp_preloadLMemory(UINT32 vaddr, UINT32 size)
{
	static UINT8 npdisp_memBuf[CPU_PAGE_SIZE];
	UINT32 readaddr = vaddr;
	UINT32 readsize = size;
	if (npdisp.longjmpnum) return 0;
	memcpy(npdisp_jmpbuf_bak, exec_1step_jmpbuf, sizeof(exec_1step_jmpbuf)); // 現在のsetjmpを退避
	npdisp.longjmpnum = sigsetjmp(exec_1step_jmpbuf, 1); // 新しい位置にセット
	if (npdisp.longjmpnum == 0) {
		// 既に読み取り済みの範囲ならそれを返す
		while (readsize > 0 && npdisp_memread_curpos + npdisp_memread_preloadcount < npdisp_memread_buf.size()) {
			readsize--;
			readaddr++;
			npdisp_memread_preloadcount++;
		}

		// ページ単位で読みとり
		while (readsize > 0) {
			UINT32 inPageSize = CPU_PAGE_SIZE - (readaddr & CPU_PAGE_MASK);
			inPageSize = min(inPageSize, readsize);
			cpu_lmemoryreads(readaddr, npdisp_memBuf, inPageSize, CPU_PAGE_READ_DATA | CPU_MODE_SUPERVISER);
			npdisp_memread_buf.insert(npdisp_memread_buf.end(), npdisp_memBuf, npdisp_memBuf + inPageSize);
			readsize -= inPageSize;
			readaddr += inPageSize;
			npdisp_memread_preloadcount += inPageSize;
		}
	}
	else {
		TRACEOUTF(("EXCEPTION Jump!"));
	}
	memcpy(exec_1step_jmpbuf, npdisp_jmpbuf_bak, sizeof(exec_1step_jmpbuf)); // setjmpを元に戻す
	return !npdisp.longjmpnum;
}
/// <summary>
/// 指定したリニアアドレスを読み取って指定したバッファへ送る。既に読み取り済みのデータや先読みデータがある場合はそこから読み取る。
/// </summary>
/// <param name="vaddr">リニアアドレス</param>
/// <param name="buffer">読み取ったデータを格納するバッファ</param>
/// <param name="size">読み取りサイズ</param>
/// <returns>成功は0以外、ページフォールトが発生した場合は0を返す</returns>
int npdisp_readLMemory(UINT32 vaddr, void* buffer, UINT32 size)
{
	int inCurPos = npdisp_memread_curpos;
	UINT32 readaddr = vaddr;
	UINT32 readsize = size;
	UINT8* readptr = (UINT8*)buffer;
	if (npdisp.longjmpnum) return 0;
	memcpy(npdisp_jmpbuf_bak, exec_1step_jmpbuf, sizeof(exec_1step_jmpbuf)); // 現在のsetjmpを退避
	npdisp.longjmpnum = sigsetjmp(exec_1step_jmpbuf, 1); // 新しい位置にセット
	if (npdisp.longjmpnum == 0) {
		// 既に読み取り済みの範囲ならそれを返す
		while (readsize > 0 && npdisp_memread_curpos < npdisp_memread_buf.size()) {
			*readptr = npdisp_memread_buf[npdisp_memread_curpos];
			readsize--;
			readptr++;
			readaddr++;
			npdisp_memread_curpos++;
			if (npdisp_memread_preloadcount > 0) npdisp_memread_preloadcount--;
		}

		// ページ単位で読みとり
		while (readsize > 0) {
			UINT32 inPageSize = CPU_PAGE_SIZE - (readaddr & CPU_PAGE_MASK);
			inPageSize = min(inPageSize, readsize);
			cpu_lmemoryreads(readaddr, readptr, inPageSize, CPU_PAGE_READ_DATA | CPU_MODE_SUPERVISER);
			npdisp_memread_buf.insert(npdisp_memread_buf.end(), readptr, readptr + inPageSize);
			npdisp_memread_curpos += inPageSize;
			readsize -= inPageSize;
			readptr += inPageSize;
			readaddr += inPageSize;
			if (npdisp_memread_preloadcount > inPageSize) {
				npdisp_memread_preloadcount -= inPageSize;
			}
			else {
				npdisp_memread_preloadcount = 0;
			}
		}
	}
	else {
		TRACEOUTF(("EXCEPTION Jump!"));
	}
	memcpy(exec_1step_jmpbuf, npdisp_jmpbuf_bak, sizeof(exec_1step_jmpbuf)); // setjmpを元に戻す
	return !npdisp.longjmpnum;
}
/// <summary>
/// 指定したリニアアドレスをへデータを書き込む。既に書き込み済みの場合はスキップする。
/// </summary>
/// <param name="vaddr">リニアアドレス</param>
/// <param name="buffer">書き込むデータ</param>
/// <param name="size">読み取りサイズ</param>
/// <returns>成功は0以外、ページフォールトが発生した場合は0を返す</returns>
int npdisp_writeLMemory(UINT32 vaddr, void* buffer, UINT32 size)
{
	UINT32 writeaddr = vaddr;
	UINT32 writesize = size;
	UINT8* writeptr = (UINT8*)buffer;
	if (npdisp.longjmpnum) return 0;
	memcpy(npdisp_jmpbuf_bak, exec_1step_jmpbuf, sizeof(exec_1step_jmpbuf)); // 現在のsetjmpを退避
	npdisp.longjmpnum = sigsetjmp(exec_1step_jmpbuf, 1); // 新しい位置にセット
	if (npdisp.longjmpnum == 0) {
		// 既に書き込み済みの範囲ならスキップ
		UINT32 wnsize = npdisp_memwrite_bufwpos - npdisp_memwrite_curpos;
		if (wnsize >= size) {
			// 全部書き込み済み
			npdisp_memwrite_curpos += size;
		}
		else {
			// 書き込み済み分があればスキップ
			writesize -= wnsize;
			writeptr += wnsize;
			writeaddr += wnsize;
			npdisp_memwrite_curpos += wnsize;

			// ページ単位で書き込み
			while (writesize > 0) {
				UINT32 inPageSize = CPU_PAGE_SIZE - (writeaddr & CPU_PAGE_MASK);
				inPageSize = min(inPageSize, writesize);
				cpu_lmemorywrites(writeaddr, writeptr, inPageSize, CPU_PAGE_WRITE_DATA | CPU_MODE_SUPERVISER);
				npdisp_memwrite_bufwpos += inPageSize;
				npdisp_memwrite_curpos += inPageSize;
				writesize -= inPageSize;
				writeptr += inPageSize;
				writeaddr += inPageSize;
			}
		}
	}
	else {
		TRACEOUTF(("EXCEPTION Jump!"));
	}
	memcpy(exec_1step_jmpbuf, npdisp_jmpbuf_bak, sizeof(exec_1step_jmpbuf)); // setjmpを元に戻す
	return !npdisp.longjmpnum;
}

int npdisp_preloadMemoryWith32Offset(UINT16 selector, UINT32 offset, int size)
{
	UINT16 seg = selector;
	UINT32 linearAddr;
	if (!selector) return 0;
	if (npdisp.longjmpnum) return 0;
	// 既に読み取り済みの範囲ならそれを返す
	if (npdisp_memread_buf.size() - (int)(npdisp_memread_curpos + npdisp_memread_preloadcount) >= size) {
		npdisp_memread_preloadcount += size;
		return !npdisp.longjmpnum;
	}
	if (selector_to_linear(seg, offset, &linearAddr)) { // offsetを32bitで扱う
		return npdisp_preloadLMemory(linearAddr, size);
	}
	return 0;
}
int npdisp_preloadMemory(UINT32 lpAddr, int size)
{
	UINT16 seg = (lpAddr >> 16) & 0xffff;
	UINT16 ofs = lpAddr & 0xffff;
	UINT32 linearAddr;
	if (!lpAddr) return 0;
	if (npdisp.longjmpnum) return 0;
	// 既に読み取り済みの範囲ならそれを返す
	if (npdisp_memread_buf.size() - (int)(npdisp_memread_curpos + npdisp_memread_preloadcount) >= size) {
		npdisp_memread_preloadcount += size;
		return !npdisp.longjmpnum;
	}
	if (selector_to_linear(seg, ofs, &linearAddr)) {
		return npdisp_preloadLMemory(linearAddr, size);
	}
	return 0;
}
int npdisp_readMemoryWith32Offset(void* dst, UINT16 selector, UINT32 offset, int size)
{
	UINT16 seg = selector;
	UINT32 linearAddr;
	if (!selector) return 0;
	if (npdisp.longjmpnum) return 0;
	// 既に読み取り済みの範囲ならそれを返す
	if (npdisp_memread_buf.size() - (int)npdisp_memread_curpos >= size) {
		UINT8* readptr = (UINT8*)dst;
		memcpy(readptr, &(npdisp_memread_buf[npdisp_memread_curpos]), size);
		npdisp_memread_curpos += size;
		if (npdisp_memread_preloadcount > size) {
			npdisp_memread_preloadcount -= size;
		}
		else {
			npdisp_memread_preloadcount = 0;
		}
		return !npdisp.longjmpnum;
	}
	if (selector_to_linear(seg, offset, &linearAddr)) { // offsetを32bitで扱う
		return npdisp_readLMemory(linearAddr, dst, size);
	}
	return 0;
}
int npdisp_readMemory(void* dst, UINT32 lpAddr, int size) 
{
	UINT16 seg = (lpAddr >> 16) & 0xffff;
	UINT16 ofs = lpAddr & 0xffff;
	UINT32 linearAddr;
	if (!lpAddr) return 0;
	if (npdisp.longjmpnum) return 0;
	// 既に読み取り済みの範囲ならそれを返す
	if (npdisp_memread_buf.size() - (int)npdisp_memread_curpos >= size) {
		UINT8* readptr = (UINT8*)dst;
		memcpy(readptr, &(npdisp_memread_buf[npdisp_memread_curpos]), size);
		npdisp_memread_curpos += size;
		if (npdisp_memread_preloadcount > size) {
			npdisp_memread_preloadcount -= size;
		}
		else {
			npdisp_memread_preloadcount = 0;
		}
		return !npdisp.longjmpnum;
	}
	if (selector_to_linear(seg, ofs, &linearAddr)) {
		return npdisp_readLMemory(linearAddr, dst, size);
	}
	return 0;
}
int npdisp_writeMemory(void* src, UINT32 lpAddr, int size) 
{
	UINT16 seg = (lpAddr >> 16) & 0xffff;
	UINT16 ofs = lpAddr & 0xffff;
	UINT32 linearAddr;
	if (!lpAddr) return 0;
	if (npdisp.longjmpnum) return 0;
	// 既に書き込み済みの範囲なら何もしない
	if ((int)npdisp_memwrite_bufwpos - (int)npdisp_memwrite_curpos >= size) {
		npdisp_memwrite_curpos += size;
		return !npdisp.longjmpnum;
	}
	if (selector_to_linear(seg, ofs, &linearAddr)) 
	{
		return npdisp_writeLMemory(linearAddr, src, size);
	}
	return 0;
}
UINT8 npdisp_readMemory8With32Offset(UINT16 selector, UINT32 offset)
{
	UINT8 dst = 0;
	npdisp_readMemoryWith32Offset(&dst, selector, offset, 1);
	return dst;
}
UINT8 npdisp_readMemory8(UINT32 lpAddr) 
{
	UINT8 dst = 0;
	npdisp_readMemory(&dst, lpAddr, 1);
	return dst;
}
UINT16 npdisp_readMemory16(UINT32 lpAddr) 
{
	UINT16 dst = 0;
	npdisp_readMemory(&dst, lpAddr, 2);
	return dst;
}
UINT32 npdisp_readMemory32(UINT32 lpAddr) 
{
	UINT32 dst = 0;
	npdisp_readMemory(&dst, lpAddr, 4);
	return dst;
}
int npdisp_writeMemory8(UINT8 value, UINT32 lpAddr) 
{
	return npdisp_writeMemory(&value, lpAddr, 1);
}
int npdisp_writeMemory16(UINT16 value, UINT32 lpAddr) 
{
	return npdisp_writeMemory(&value, lpAddr, 2);
}
int npdisp_writeMemory32(UINT32 value, UINT32 lpAddr) 
{
	return npdisp_writeMemory(&value, lpAddr, 4);
}
void npdisp_writeReturnCode(NPDISP_REQUEST *lpReq, UINT32 dataAddr, UINT16 retCode) 
{
	lpReq->returnCode = retCode;
	npdisp_writeMemory((UINT8*)lpReq + 4, npdisp.dataAddr + 4, 2); // ReturnCode書き込み
}

char* npdisp_readMemoryString(UINT32 lpAddr) 
{
	char *strBuf;
	int addr;
	int len;
	if (!lpAddr) return NULL;
	for (addr = lpAddr; ((addr ^ lpAddr) & 0xffff0000) == 0 && npdisp_readMemory8(addr); addr++); // NULL文字がでるか、セグメントが変わる(=異常)まで回す
	if (((addr ^ lpAddr) & 0xffff0000) != 0) return NULL; // セグメントにめり込むサイズは異常

	len = addr - lpAddr + 1; // 長さ計算 NULL文字も含む

	strBuf = (char*)malloc(len);
	if (!strBuf) return NULL;
	if (!npdisp_readMemory(strBuf, lpAddr, len)) {
		free(strBuf);
		return NULL;
	}
	return strBuf;
}
char* npdisp_readMemoryStringWithCount(UINT32 lpAddr, int count)
{
	char* strBuf;
	int len;
	if (!lpAddr) return NULL;
	if (count <= 0) return NULL;

	strBuf = (char*)malloc(count + 1);
	if (!strBuf) return NULL;
	if (!npdisp_readMemory(strBuf, lpAddr, count)) {
		free(strBuf);
		return NULL;
	}
	strBuf[count] = '\0';
	return strBuf;
}

//static int lastPreloadB = 0;
//static int lastPreload = 0;
//static int lastPreload_memread_curpos;
//static int lastPreload_memread_curpos2;
//static int lastPreload_memread_size;
//static int lastPreload_memread_size2;
//static int lastPreload_imgsize;

// メモリ先読み
// 注意：これを呼んだ後にnpdisp_MakeBitmapFromPBITMAPをすぐに呼ぶこと。間に別のreadを噛ませてはいけない。
// 　　　また、複数npdisp_PreloadBitmapFromPBITMAPを呼んで複数npdisp_MakeBitmapFromPBITMAPしても構わないが、引数や呼ぶ順番を変えてはならない
void npdisp_PreloadBitmapFromPBITMAP(NPDISP_PBITMAP* srcPBmp, int dcIdx, int beginLine, int numLines) {
	if (npdisp.longjmpnum != 0) return;

	//lastPreloadB = npdisp_memread_preloadcount;
	//lastPreload_memread_curpos = npdisp_memread_curpos;
	//lastPreload_memread_size = npdisp_memread_buf.size();
	int i, j;
	int bpp = srcPBmp->bmPlanes * srcPBmp->bmBitsPixel;
	int	srcstride = srcPBmp->bmWidthBytes;
	int	dststride = ((srcPBmp->bmWidth * bpp + 31) / 32) * 4;
	if (numLines == -1 || numLines > srcPBmp->bmHeight) numLines = srcPBmp->bmHeight;
	if (beginLine + numLines > srcPBmp->bmHeight) numLines = srcPBmp->bmHeight - beginLine;
	if (beginLine >= srcPBmp->bmHeight) {
		beginLine = 0;
		numLines = 0;
	}
	int endLine = beginLine + numLines;
	//lastPreload_imgsize = srcstride * numLines;
	// 先に読み取り
	if (srcPBmp->bmSegmentIndex != 0) {
		// 64KB超え転送
		UINT16 seg = (srcPBmp->bmBitsAddr >> 16) & 0xffff;
		UINT16 ofs = srcPBmp->bmBitsAddr & 0xffff;
		int remain = srcPBmp->bmHeight;
		int segBeginLine = beginLine / srcPBmp->bmScanSegment * srcPBmp->bmScanSegment;
		int segEndLine = (endLine + srcPBmp->bmScanSegment - 1) / srcPBmp->bmScanSegment * srcPBmp->bmScanSegment;
		seg += srcPBmp->bmSegmentIndex * (segBeginLine / srcPBmp->bmScanSegment);
		remain -= segBeginLine;
		// 1ラインずつ転送
		for (j = segBeginLine; j < segEndLine; j += srcPBmp->bmScanSegment) {
			UINT16 srcOfs = ofs;
			int looplen = srcPBmp->bmScanSegment < remain ? srcPBmp->bmScanSegment : remain;
			for (i = 0; i < looplen; i++) {
				npdisp_preloadMemory(((UINT32)seg << 16) | srcOfs, srcstride);
				srcOfs += srcstride;
			}
			seg += srcPBmp->bmSegmentIndex;
			remain -= looplen;
		}
	}
	else {
		// 64KB未満転送
		UINT16 seg = (srcPBmp->bmBitsAddr >> 16) & 0xffff;
		UINT16 ofs = srcPBmp->bmBitsAddr & 0xffff;
		if (dststride == srcstride) {
			// アライメントが一致しているので一括転送可能
			UINT16 srcOfs = ofs + srcstride * beginLine;
			npdisp_preloadMemory(((UINT32)seg << 16) | srcOfs, srcstride * numLines);
		}
		else {
			// アライメント合わせのために1ラインずつ転送
			UINT16 srcOfs = ofs;
			srcOfs += srcstride * beginLine;
			for (i = beginLine; i < endLine; i++) {
				npdisp_preloadMemory(((UINT32)seg << 16) | srcOfs, srcstride);
				srcOfs += srcstride;
			}
		}
	}
	//lastPreload = npdisp_memread_preloadcount;
	//lastPreload_memread_curpos2 = npdisp_memread_curpos;
	//lastPreload_memread_size2 = npdisp_memread_buf.size();
}

int npdisp_MakeBitmapFromPBITMAP(NPDISP_PBITMAP* srcPBmp, NPDISP_WINDOWS_BMPHDC* bmpHDC, int dcIdx, int beginLine, int numLines, UINT16* transTable) {
	int i, j;
	int bpp = srcPBmp->bmPlanes * srcPBmp->bmBitsPixel;
	int	srcstride = srcPBmp->bmWidthBytes;
	BITMAPINFO* lpbi = NULL;

	if (npdisp.longjmpnum != 0) return 0;

	if (bpp <= 8) {
		lpbi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << bpp));
		if (lpbi) {
			if (bpp == 1) {
				// 2色パレットセット
				for (i = 0; i < NELEMENTS(npdisp_palette_rgb2); i++) {
					lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb2[i].r;
					lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb2[i].g;
					lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb2[i].b;
					lpbi->bmiColors[i].rgbReserved = 0;
				}
			}
			else if (bpp == 4) {
				// 16色パレットセット
				for (i = 0; i < NELEMENTS(npdisp_palette_rgb16); i++) {
					lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb16[i].r;
					lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb16[i].g;
					lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb16[i].b;
					lpbi->bmiColors[i].rgbReserved = 0;
				}
			}
			else if (bpp == 8) {
				// 256色パレットセット
				if (npdisp.usePalette) {
					if (transTable) {
						// パレット番号変換の上転送
						for (i = 0; i < NELEMENTS(npdisp_palette_rgb256); i++) {
							lpbi->bmiColors[i].rgbRed = transTable[i] & 0xff;
							lpbi->bmiColors[i].rgbGreen = transTable[i] & 0xff;
							lpbi->bmiColors[i].rgbBlue = transTable[i] & 0xff;
							lpbi->bmiColors[i].rgbReserved = 0;
						}
					}
					else {
						// 仮想パレット番号
						for (i = 0; i < NELEMENTS(npdisp_palette_rgb256); i++) {
							lpbi->bmiColors[i].rgbRed = i;
							lpbi->bmiColors[i].rgbGreen = i;
							lpbi->bmiColors[i].rgbBlue = i;
							lpbi->bmiColors[i].rgbReserved = 0;
						}
					}
				}
				else {
					for (i = 0; i < NELEMENTS(npdisp_palette_rgb256); i++) {
						lpbi->bmiColors[i].rgbRed = npdisp_palette_rgb256[i].r;
						lpbi->bmiColors[i].rgbGreen = npdisp_palette_rgb256[i].g;
						lpbi->bmiColors[i].rgbBlue = npdisp_palette_rgb256[i].b;
						lpbi->bmiColors[i].rgbReserved = 0;
					}
				}
			}
		}
	}
	else {
		lpbi = (BITMAPINFO*)malloc(sizeof(BITMAPINFO));
	}
	if (lpbi) {
		//HDC hdcScreen = GetDC(NULL);
		bmpHDC->hdc = npdispwin.hdcCache[dcIdx];
		if (bmpHDC->hdc) {
			lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			lpbi->bmiHeader.biWidth = srcPBmp->bmWidth;
			lpbi->bmiHeader.biHeight = -srcPBmp->bmHeight;
			lpbi->bmiHeader.biPlanes = srcPBmp->bmPlanes;
			lpbi->bmiHeader.biBitCount = srcPBmp->bmBitsPixel;
			lpbi->bmiHeader.biCompression = BI_RGB;
			lpbi->bmiHeader.biSizeImage = 0;
			lpbi->bmiHeader.biXPelsPerMeter = 0;
			lpbi->bmiHeader.biYPelsPerMeter = 0;
			lpbi->bmiHeader.biClrUsed = 1 << srcPBmp->bmBitsPixel;
			lpbi->bmiHeader.biClrImportant = lpbi->bmiHeader.biClrUsed;
			bmpHDC->hBmp = CreateDIBSection(bmpHDC->hdc, lpbi, DIB_RGB_COLORS, &bmpHDC->pBits, NULL, 0);
			if (bmpHDC->hBmp) {
				HBITMAP hbmpSrcOld;
				bmpHDC->stride = ((srcPBmp->bmWidth * bpp + 31) / 32) * 4;
				if (numLines == -1 || numLines > srcPBmp->bmHeight) numLines = srcPBmp->bmHeight;
				if (beginLine + numLines > srcPBmp->bmHeight) numLines = srcPBmp->bmHeight - beginLine;
				if (beginLine >= srcPBmp->bmHeight) {
					beginLine = 0;
					numLines = 0;
				}
				int endLine = beginLine + numLines;
				if (srcPBmp->bmSegmentIndex != 0) {
					// 64KB超え転送
					UINT16 seg = (srcPBmp->bmBitsAddr >> 16) & 0xffff;
					UINT16 ofs = srcPBmp->bmBitsAddr & 0xffff;
					int remain = srcPBmp->bmHeight;
					int segBeginLine = beginLine / srcPBmp->bmScanSegment * srcPBmp->bmScanSegment;
					int segEndLine = (endLine + srcPBmp->bmScanSegment - 1) / srcPBmp->bmScanSegment * srcPBmp->bmScanSegment;
					seg += srcPBmp->bmSegmentIndex * (segBeginLine / srcPBmp->bmScanSegment);
					remain -= segBeginLine;
					char* dstPtr = (char*)(bmpHDC->pBits) + bmpHDC->stride * segBeginLine;
					// 1ラインずつ転送
					for (j = segBeginLine; j < segEndLine; j += srcPBmp->bmScanSegment) {
						UINT16 srcOfs = ofs;
						int looplen = srcPBmp->bmScanSegment < remain ? srcPBmp->bmScanSegment : remain;
						for (i = 0; i < looplen; i++) {
							npdisp_readMemory(dstPtr, ((UINT32)seg << 16) | srcOfs, srcstride);
							srcOfs += srcstride;
							dstPtr += bmpHDC->stride;
						}
						seg += srcPBmp->bmSegmentIndex;
						remain -= looplen;
					}
				}
				else {
					// 64KB未満転送
					UINT16 seg = (srcPBmp->bmBitsAddr >> 16) & 0xffff;
					UINT16 ofs = srcPBmp->bmBitsAddr & 0xffff;
					if (bmpHDC->stride == srcstride) {
						// アライメントが一致しているので一括転送可能
						char* dstPtr = (char*)(bmpHDC->pBits);
						UINT16 srcOfs = ofs + srcstride * beginLine;
						dstPtr += bmpHDC->stride * beginLine;
						npdisp_readMemory(dstPtr, ((UINT32)seg << 16) | srcOfs, srcstride * numLines);
					}
					else {
						// アライメント合わせのために1ラインずつ転送
						char* dstPtr = (char*)(bmpHDC->pBits);
						UINT16 srcOfs = ofs;
						srcOfs += srcstride * beginLine;
						dstPtr += bmpHDC->stride * beginLine;
						for (i = beginLine; i < endLine; i++) {
							npdisp_readMemory(dstPtr, ((UINT32)seg << 16) | srcOfs, srcstride);
							srcOfs += srcstride;
							dstPtr += bmpHDC->stride;
						}
					}
				}
				//char* dstPtr2 = (char*)(bmpHDC->pBits);
				//for (i = beginLine; i < beginLine+2; i++) {
				//	for (int x = 0; x < bmpHDC->stride; x++) {
				//		dstPtr2[x] = x;
				//	}
				//	dstPtr2 += bmpHDC->stride;
				//}

				if (npdisp.longjmpnum == 0) {
					bmpHDC->hOldBmp = SelectObject(bmpHDC->hdc, bmpHDC->hBmp);
					bmpHDC->lpbi = lpbi;
					//BitBlt(np2wabwnd.hDCBuf, npdisp.width / 2, 0, npdisp.width / 2, npdisp.height, bmpHDC->hdc, 0, 0, SRCCOPY);
				}
				else {
					DeleteObject(bmpHDC->hBmp);
					bmpHDC->hdc = NULL;
					free(lpbi);
					bmpHDC->lpbi = NULL;
				}
			}
			else {
				bmpHDC->hdc = NULL;
				free(lpbi);
				bmpHDC->lpbi = NULL;
			}
		}
		else {
			bmpHDC->hdc = NULL;
			free(lpbi);
			bmpHDC->lpbi = NULL;
		}
		//ReleaseDC(NULL, hdcScreen); // もういらない
	}

	return bmpHDC->hdc != NULL;
}
void npdisp_WriteBitmapToPBITMAP(NPDISP_PBITMAP* dstPBmp, NPDISP_WINDOWS_BMPHDC* bmpHDC, int beginLine, int numLines) {
	if (!bmpHDC) return;

	if (npdisp.longjmpnum != 0) return;

	if (bmpHDC->pBits && bmpHDC->lpbi) {
		int i, j;
		int bpp = dstPBmp->bmPlanes * dstPBmp->bmBitsPixel;
		int	dststride = dstPBmp->bmWidthBytes;
		if (numLines == -1 || numLines > dstPBmp->bmHeight) numLines = dstPBmp->bmHeight;
		if (beginLine + numLines > dstPBmp->bmHeight) numLines = dstPBmp->bmHeight - beginLine;
		if (beginLine >= dstPBmp->bmHeight) {
			beginLine = 0;
			numLines = 0;
		}
		int endLine = beginLine + numLines;

		//if (bpp == 1) {
		//	UINT16 seg = (dstPBmp->bmBitsAddr >> 16) & 0xffff;
		//	UINT16 ofs = dstPBmp->bmBitsAddr & 0xffff;
		//	void* buf = malloc(dststride * dstPBmp->bmHeight);
		//	if (buf) {
		//		memset(buf, 0xf0, dststride * dstPBmp->bmHeight);
		//		npdisp_writeMemory(buf, ((UINT32)seg << 16) | ofs, dststride * dstPBmp->bmHeight);
		//		free(buf);
		//	}
		//}
		//else {
		if (dstPBmp->bmSegmentIndex != 0) {
			// 64KB超え転送
			UINT16 seg = (dstPBmp->bmBitsAddr >> 16) & 0xffff;
			UINT16 ofs = dstPBmp->bmBitsAddr & 0xffff;
			int remain = dstPBmp->bmHeight;
			int segBeginLine = beginLine / dstPBmp->bmScanSegment * dstPBmp->bmScanSegment;
			int segEndLine = (endLine + dstPBmp->bmScanSegment - 1) / dstPBmp->bmScanSegment * dstPBmp->bmScanSegment;
			seg += dstPBmp->bmSegmentIndex * (segBeginLine / dstPBmp->bmScanSegment);
			remain -= segBeginLine;
			char* srcPtr = (char*)(bmpHDC->pBits) + bmpHDC->stride * segBeginLine;
			// 1ラインずつ転送
			for (j = segBeginLine; j < segEndLine; j += dstPBmp->bmScanSegment) {
				UINT16 dstOfs = ofs;
				int looplen = dstPBmp->bmScanSegment < remain ? dstPBmp->bmScanSegment : remain;
				for (i = 0; i < looplen; i++) {
					npdisp_writeMemory(srcPtr, ((UINT32)seg << 16) | dstOfs, dststride);
					dstOfs += dststride;
					srcPtr += bmpHDC->stride;
				}
				seg += dstPBmp->bmSegmentIndex;
				remain -= looplen;
			}
		}
		else {
			// 64KB未満転送
			UINT16 seg = (dstPBmp->bmBitsAddr >> 16) & 0xffff;
			UINT16 ofs = dstPBmp->bmBitsAddr & 0xffff;
			if (bmpHDC->stride == dststride) {
				char* srcPtr = (char*)(bmpHDC->pBits);
				UINT16 dstOfs = ofs + dststride * beginLine;
				srcPtr += bmpHDC->stride * beginLine;
				npdisp_writeMemory(srcPtr, ((UINT32)seg << 16) | dstOfs, dststride * numLines);
			}
			else {
				// アライメント合わせのために1ラインずつ転送
				char* srcPtr = (char*)(bmpHDC->pBits);
				UINT16 dstOfs = ofs;
				dstOfs += dststride * beginLine;
				srcPtr += bmpHDC->stride * beginLine;
				for (i = beginLine; i < endLine; i++) {
					npdisp_writeMemory(srcPtr, ((UINT32)seg << 16) | dstOfs, dststride);
					dstOfs += dststride;
					srcPtr += bmpHDC->stride;
				}
			}
		}
		//}
	}
}
void npdisp_FreeBitmap(NPDISP_WINDOWS_BMPHDC* bmpHDC) {
	if (!bmpHDC) return;

	if (bmpHDC->hdc) {
		if (bmpHDC->lpbi) {
			free(bmpHDC->lpbi);
			bmpHDC->lpbi = NULL;
		}
		if (bmpHDC->hBmp) {
			SelectObject(bmpHDC->hdc, bmpHDC->hOldBmp);
			DeleteObject(bmpHDC->hBmp);
			bmpHDC->hBmp = NULL;
			bmpHDC->pBits = NULL;
		}
		bmpHDC->hdc = NULL;
	}
}

#endif