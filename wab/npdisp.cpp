/**
 * @file	npdisp.c
 * @brief	Implementation of the Neko Project II Display Adapter
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>

#include	"pccore.h"
#include	"wab.h"
#include	"npdispdef.h"
#include	"npdisp.h"
#include	"dosio.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"soundmng.h"

#if defined(SUPPORT_IA32_HAXM)
#include "i386hax/haxfunc.h"
#include "i386hax/haxcore.h"
#endif

#if 0
#undef	TRACEOUT
static void trace_fmt_ex(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUT(s)	trace_fmt_ex s
#endif	/* 1 */
#if 0
static void trace_fmt_ex2(const char* fmt, ...)
{
	char stmp[2048];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(stmp, fmt, ap);
	strcat(stmp, "\n");
	va_end(ap);
	OutputDebugStringA(stmp);
}
#define	TRACEOUT2(s)	trace_fmt_ex2 s
#else
#define	TRACEOUT2(s)	(void)s
#endif	/* 1 */
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


static void npdisp_releaseScreen(void);
static void npdisp_createScreen(void);

NPDISP npdisp = { 0 };

static std::vector<UINT8> npdisp_memread_buf; // リクエストされてから読み込み完了しているデータを表す
static UINT32 npdisp_memwrite_bufwpos = 0; // リクエストされてから書き込み完了している位置を表す

static UINT32 npdisp_memread_curpos = 0; // リクエストされてからのデータ読み取りバイト数
static UINT32 npdisp_memread_preloadcount = 0; // データプリロードバイト数
static UINT32 npdisp_memwrite_curpos = 0; // リクエストされてからのデータ書き込みバイト数

static UINT16 npdisp_selector_cache = 0;
static UINT32 npdisp_seg_cache = 0;

static sigjmp_buf npdisp_jmpbuf_bak;

typedef struct {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD          bmiColors[256];
} BITMAPINFO_8BPP;

typedef struct {
	BITMAPINFO_8BPP bi;
	HDC hdc;
	void* pBits;
	HBITMAP hBmp;
	HPALETTE hPalette;
	HGDIOBJ hOldBmp;
	HGDIOBJ hOldPen;
	HGDIOBJ hOldBrush;
	HPALETTE hOldPalette;
	UINT32 stride;
	HFONT hFont;

	HDC hdcShadow;
	void* pBitsShadow;
	HBITMAP hBmpShadow;
	HGDIOBJ hOldBmpShadow;
	RECT rectShadow;

	HDC hdcCursor;
	HBITMAP hBmpCursor;
	HBITMAP hOldBmpCursor;
	HDC hdcCursorMask;
	HBITMAP hBmpCursorMask;
	HBITMAP hOldBmpCursorMask;

	HDC hdcCache[2];

	UINT32 pensIdx;
	std::map<UINT32, NPDISP_HOSTPEN> pens;
	UINT32 brushesIdx;
	std::map<UINT32, NPDISP_HOSTBRUSH> brushes;
} NPDISP_WINDOWS;

NPDISP_WINDOWS npdispwin = { 0 };

typedef struct {
	HDC hdc;
	void* pBits;
	HBITMAP hBmp;
	HGDIOBJ hOldBmp;
	UINT32 stride;
	BITMAPINFO* lpbi;
} NPDISP_WINDOWS_BMPHDC;

static int npdisp_cs_initialized = 0;
static CRITICAL_SECTION npdisp_cs;
static CRITICAL_SECTION npdisp_cs_exception;
static int npdisp_cs_execflag = 0;

void npdispcs_enter_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	EnterCriticalSection(&npdisp_cs);
}
BOOL npdispcs_tryenter_criticalsection(void)
{
	if (!npdisp_cs_initialized) return FALSE;
	return TryEnterCriticalSection(&npdisp_cs);
}
void npdispcs_leave_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	LeaveCriticalSection(&npdisp_cs);
}
void npdispcs_enter_exception_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	EnterCriticalSection(&npdisp_cs_exception);
}
void npdispcs_leave_exception_criticalsection(void)
{
	if (!npdisp_cs_initialized) return;
	LeaveCriticalSection(&npdisp_cs_exception);
}

void npdispcs_initialize(void)
{
	/* クリティカルセクション準備 */
	if (!npdisp_cs_initialized)
	{
		memset(&npdisp_cs, 0, sizeof(npdisp_cs));
		InitializeCriticalSection(&npdisp_cs);
		InitializeCriticalSection(&npdisp_cs_exception);
		npdisp_cs_initialized = 1;
	}
}
void npdispcs_shutdown(void)
{
	/* クリティカルセクション破棄 */
	if (npdisp_cs_initialized)
	{
		memset(&npdisp_cs, 0, sizeof(npdisp_cs));
		DeleteCriticalSection(&npdisp_cs_exception);
		DeleteCriticalSection(&npdisp_cs);
		npdisp_cs_initialized = 0;
	}
}


typedef struct {
	UINT8 r;
	UINT8 g;
	UINT8 b;
} NPDISP_RGB3;

static NPDISP_RGB3 s_npdisp_rgb2[2] = {
	{0x00,0x00,0x00}, /* 0 black */
	{0xFF,0xFF,0xFF}  /* 1 white */
};

static NPDISP_RGB3 s_npdisp_rgb16[16] = {
	{0x00,0x00,0x00}, /* 0 black */
	{0x80,0x00,0x00}, /* 1 dark red */
	{0x00,0x80,0x00}, /* 2 dark green */
	{0x80,0x80,0x00}, /* 3 dark yellow */
	{0x00,0x00,0x80}, /* 4 dark blue */
	{0x80,0x00,0x80}, /* 5 dark magenta */
	{0x00,0x80,0x80}, /* 6 dark cyan */
	{0x80,0x80,0x80}, /* 7 dark gray */
	{0xC0,0xC0,0xC0}, /* 8 light gray */
	{0xFF,0x00,0x00}, /* 9 red */
	{0x00,0xFF,0x00}, /* 10 green */
	{0xFF,0xFF,0x00}, /* 11 yellow */
	{0x00,0x00,0xFF}, /* 12 blue */
	{0xFF,0x00,0xFF}, /* 13 magenta */
	{0x00,0xFF,0xFF}, /* 14 cyan */
	{0xFF,0xFF,0xFF}  /* 15 white */
};

static NPDISP_RGB3 s_npdisp_rgb256[256] = {0};

// 強引にリニアアドレスを計算
static UINT32 selector_to_linear(UINT16 selector, UINT32 offset, UINT32 *lplAddr)
{
	selector_t sel;
	int rv;

	if (selector == npdisp_selector_cache) {
		*lplAddr = npdisp_seg_cache + offset;
		return 1;
	}

	memset(&sel, 0, sizeof(sel));

	rv = parse_selector(&sel, selector);
	if (rv == 0) {
		// OK
		npdisp_selector_cache = selector;
		npdisp_seg_cache = sel.desc.u.seg.segbase;
		*lplAddr = sel.desc.u.seg.segbase + offset;
		return 1;
	}
	// Fail
	return 0;
}

static UINT8 npdisp_memBuf[CPU_PAGE_SIZE];

// リードしてキャッシュにためるだけ
static int npdisp_preloadLMemory(UINT32 vaddr, UINT32 size)
{
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
static int npdisp_readLMemory(UINT32 vaddr, void* buffer, UINT32 size)
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
static int npdisp_writeLMemory(UINT32 vaddr, void* buffer, UINT32 size)
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
				cpu_lmemorywrites(writeaddr, writeptr, inPageSize, CPU_PAGE_READ_DATA | CPU_MODE_SUPERVISER);
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

//static int npdisp_readMemory2(void* dst, UINT32 lpAddr, int size)
//{
//	int i;
//	UINT16 seg = (lpAddr >> 16) & 0xffff;
//	UINT16 ofs = lpAddr & 0xffff;
//	UINT32 linearAddr;
//	UINT8* p = (UINT8*)dst;
//	for (i = 0; i < size; i++) {
//		if (selector_to_linear(seg, ofs, &linearAddr)) {
//			npdisp_readLMemory(linearAddr, p, 1);
//		}
//		ofs++;
//		p++;
//	}
//	return 0;
//}
static int npdisp_preloadMemoryWith32Offset(UINT16 selector, UINT32 offset, int size)
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
static int npdisp_preloadMemory(UINT32 lpAddr, int size)
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
static int npdisp_readMemoryWith32Offset(void* dst, UINT16 selector, UINT32 offset, int size)
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
static int npdisp_readMemory(void* dst, UINT32 lpAddr, int size) 
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
static int npdisp_writeMemory(void* dst, UINT32 lpAddr, int size) 
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
		return npdisp_writeLMemory(linearAddr, dst, size);
	}
	return 0;
}
static UINT8 npdisp_readMemory8With32Offset(UINT16 selector, UINT32 offset)
{
	UINT8 dst = 0;
	npdisp_readMemoryWith32Offset(&dst, selector, offset, 1);
	return dst;
}
static UINT8 npdisp_readMemory8(UINT32 lpAddr) 
{
	UINT8 dst = 0;
	npdisp_readMemory(&dst, lpAddr, 1);
	return dst;
}
static UINT16 npdisp_readMemory16(UINT32 lpAddr) 
{
	UINT16 dst = 0;
	npdisp_readMemory(&dst, lpAddr, 2);
	return dst;
}
static UINT32 npdisp_readMemory32(UINT32 lpAddr) 
{
	UINT32 dst = 0;
	npdisp_readMemory(&dst, lpAddr, 4);
	return dst;
}
static int npdisp_writeMemory8(UINT8 value, UINT32 lpAddr) 
{
	return npdisp_writeMemory(&value, lpAddr, 1);
}
static int npdisp_writeMemory16(UINT16 value, UINT32 lpAddr) 
{
	return npdisp_writeMemory(&value, lpAddr, 2);
}
static int npdisp_writeMemory32(UINT32 value, UINT32 lpAddr) 
{
	return npdisp_writeMemory(&value, lpAddr, 4);
}
static void npdisp_writeReturnCode(NPDISP_REQUEST *lpReq, UINT32 dataAddr, UINT16 retCode) 
{
	lpReq->returnCode = retCode;
	npdisp_writeMemory((UINT8*)lpReq + 4, npdisp.dataAddr + 4, 2); // ReturnCode書き込み
}

static char* npdisp_readMemoryString(UINT32 lpAddr) 
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
static char* npdisp_readMemoryStringWithCount(UINT32 lpAddr, int count)
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

static int npdisp_FindNearest2(UINT8 r, UINT8 g, UINT8 b)
{
	//return ((UINT16)r + g + b >700) ? 1 : 0;
	int i;
	int best = 0;
	long bestDist = 0x7FFFFFFFL;
	for (i = 0; i < NELEMENTS(s_npdisp_rgb2); i++) {
		long dr = (long)r - s_npdisp_rgb2[i].r;
		long dg = (long)g - s_npdisp_rgb2[i].g;
		long db = (long)b - s_npdisp_rgb2[i].b;
		long dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	return best;
}
static int npdisp_FindNearest16(UINT8 r, UINT8 g, UINT8 b)
{
	int i;
	int best = 0;
	long bestDist = 0x7FFFFFFFL;
	for (i = 0; i < NELEMENTS(s_npdisp_rgb16); i++) {
		long dr = (long)r - s_npdisp_rgb16[i].r;
		long dg = (long)g - s_npdisp_rgb16[i].g;
		long db = (long)b - s_npdisp_rgb16[i].b;
		long dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	return best;
}
static int npdisp_FindNearest256(UINT8 r, UINT8 g, UINT8 b)
{
	int i;
	int best = 0;
	long bestDist = 0x7FFFFFFFL;
	// システムカラー（固定色）優先で探す
	for (i = 246; i < 256; i++) {
		long dr = (long)r - s_npdisp_rgb256[i].r;
		long dg = (long)g - s_npdisp_rgb256[i].g;
		long db = (long)b - s_npdisp_rgb256[i].b;
		long dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	for (i = 0; i < 10; i++) {
		long dr = (long)r - s_npdisp_rgb256[i].r;
		long dg = (long)g - s_npdisp_rgb256[i].g;
		long db = (long)b - s_npdisp_rgb256[i].b;
		long dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	// 無ければ自由色で
	for (i = 10; i < 246; i++) {
		long dr = (long)r - s_npdisp_rgb256[i].r;
		long dg = (long)g - s_npdisp_rgb256[i].g;
		long db = (long)b - s_npdisp_rgb256[i].b;
		long dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	return best;
}

static int lastPreloadB = 0;
static int lastPreload = 0;
static int lastPreload_memread_curpos;
static int lastPreload_memread_curpos2;
static int lastPreload_memread_size;
static int lastPreload_memread_size2;
static int lastPreload_imgsize;

// メモリ先読み
// 注意：これを呼んだ後にnpdisp_MakeBitmapFromPBITMAPをすぐに呼ぶこと。間に別のreadを噛ませてはいけない。
// 　　　また、複数npdisp_PreloadBitmapFromPBITMAPを呼んで複数npdisp_MakeBitmapFromPBITMAPしても構わないが、引数や呼ぶ順番を変えてはならない
static void npdisp_PreloadBitmapFromPBITMAP(NPDISP_PBITMAP* srcPBmp, int dcIdx, int beginLine = 0, int numLines = -1) {
	if (npdisp.longjmpnum != 0) return;

	lastPreloadB = npdisp_memread_preloadcount;
	lastPreload_memread_curpos = npdisp_memread_curpos;
	lastPreload_memread_size = npdisp_memread_buf.size();
	int i, j;
	int bpp = srcPBmp->bmPlanes * srcPBmp->bmBitsPixel;
	int	srcstride = srcPBmp->bmWidthBytes;
	int	dststride = ((srcPBmp->bmWidth * bpp + 31) / 32) * 4;
	if (numLines == -1 || numLines > srcPBmp->bmHeight) numLines = srcPBmp->bmHeight;
	int endLine = beginLine + numLines;
	lastPreload_imgsize = srcstride * numLines;
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
	lastPreload = npdisp_memread_preloadcount;
	lastPreload_memread_curpos2 = npdisp_memread_curpos;
	lastPreload_memread_size2 = npdisp_memread_buf.size();
}

static int npdisp_MakeBitmapFromPBITMAP(NPDISP_PBITMAP* srcPBmp, NPDISP_WINDOWS_BMPHDC* bmpHDC, int dcIdx, int beginLine = 0, int numLines = -1) {
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
				for (i = 0; i < NELEMENTS(s_npdisp_rgb2); i++) {
					lpbi->bmiColors[i].rgbRed = s_npdisp_rgb2[i].r;
					lpbi->bmiColors[i].rgbGreen = s_npdisp_rgb2[i].g;
					lpbi->bmiColors[i].rgbBlue = s_npdisp_rgb2[i].b;
					lpbi->bmiColors[i].rgbReserved = 0;
				}
			}
			else if (bpp == 4) {
				// 16色パレットセット
				for (i = 0; i < NELEMENTS(s_npdisp_rgb16); i++) {
					lpbi->bmiColors[i].rgbRed = s_npdisp_rgb16[i].r;
					lpbi->bmiColors[i].rgbGreen = s_npdisp_rgb16[i].g;
					lpbi->bmiColors[i].rgbBlue = s_npdisp_rgb16[i].b;
					lpbi->bmiColors[i].rgbReserved = 0;
				}
			}
			else if (bpp == 8) {
				// 256色パレットセット
				for (i = 0; i < NELEMENTS(s_npdisp_rgb256); i++) {
					lpbi->bmiColors[i].rgbRed = s_npdisp_rgb256[i].r;
					lpbi->bmiColors[i].rgbGreen = s_npdisp_rgb256[i].g;
					lpbi->bmiColors[i].rgbBlue = s_npdisp_rgb256[i].b;
					lpbi->bmiColors[i].rgbReserved = 0;
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
			bmpHDC->hBmp = CreateDIBSection(npdispwin.hdc, lpbi, DIB_RGB_COLORS, &bmpHDC->pBits, NULL, 0);
			if (bmpHDC->hBmp) {
				HBITMAP hbmpSrcOld;
				bmpHDC->stride = ((srcPBmp->bmWidth * bpp + 31) / 32) * 4;
				if (numLines == -1 || numLines > srcPBmp->bmHeight) numLines = srcPBmp->bmHeight;
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

				if (npdisp.longjmpnum == 0) {
					bmpHDC->hOldBmp = SelectObject(bmpHDC->hdc, bmpHDC->hBmp);
					bmpHDC->lpbi = lpbi;
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
static void npdisp_WriteBitmapToPBITMAP(NPDISP_PBITMAP* dstPBmp, NPDISP_WINDOWS_BMPHDC* bmpHDC, int beginLine = 0, int numLines = -1) {
	if (!bmpHDC) return;

	if (npdisp.longjmpnum != 0) return;

	if (bmpHDC->pBits && bmpHDC->lpbi) {
		int i, j;
		int bpp = dstPBmp->bmPlanes * dstPBmp->bmBitsPixel;
		int	dststride = dstPBmp->bmWidthBytes;
		if (numLines == -1 || numLines > dstPBmp->bmHeight) numLines = dstPBmp->bmHeight;
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
static void npdisp_FreeBitmap(NPDISP_WINDOWS_BMPHDC* bmpHDC) {
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

static UINT16 npdisp_func_Enable_PDEVICE(NPDISP_PDEVICE *lpDevInfo, UINT16 wStyle, const char* lpDestDevType, const char* lpOutputFile, const NPDISP_DEVMODE* lpData) 
{
	memset(lpDevInfo, 0, sizeof(NPDISP_PDEVICE));

	lpDevInfo->bmp.bmType = 0x1003;
	lpDevInfo->bmp.bmWidth = npdisp.width;
	lpDevInfo->bmp.bmHeight = npdisp.height;
	lpDevInfo->bmp.bmBitsPixel = 1;
	lpDevInfo->bmp.bmPlanes = 4;
	npdisp.devType = lpDevInfo->bmp.bmType;
	return 1;
}
static UINT16 npdisp_func_Enable_GDIINFO(NPDISP_GDIINFO *lpDevInfo, UINT16 wStyle, const char* lpDestDevType, const char* lpOutputFile, const NPDISP_DEVMODE* lpData) 
{
	//memset(lpDevInfo, 0, sizeof(NPDISP_GDIINFO));
	
	lpDevInfo->dpVersion = 0x030A;
	lpDevInfo->dpTechnology = 1; // DT_RASDISPLAY
	lpDevInfo->dpHorzSize = 240;
	lpDevInfo->dpVertSize = 150;
	lpDevInfo->dpHorzRes = npdisp.width;
	lpDevInfo->dpVertRes = npdisp.height;
	lpDevInfo->dpNumBrushes = -1;
	lpDevInfo->dpNumPens = -1;// 16 * 5;
	lpDevInfo->futureuse = 0;
	lpDevInfo->dpNumFonts = 0;
	lpDevInfo->dpDEVICEsize = 64;// sizeof(NPDISP_PDEVICE);
	lpDevInfo->dpCurves = 0;
	lpDevInfo->dpLines = 0;
	lpDevInfo->dpPolygonals = 0;
	lpDevInfo->dpText = TC_RA_ABLE;// 0x0004 | 0x2000;
	lpDevInfo->dpClip = 1;
	lpDevInfo->dpRaster = RC_BITBLT | RC_BITMAP64 | RC_DI_BITMAP | RC_BIGFONT | RC_SAVEBITMAP; // 0x4699; // RC_BITBLT | RC_BITMAP64 | RC_SAVEBITMAP | RC_GDI20_OUTPUT | RC_DI_BITMAP | RC_DIBTODEV | RC_OP_DX_OUTPUT;
	lpDevInfo->dpAspectX = 71;
	lpDevInfo->dpAspectY = 71;
	lpDevInfo->dpAspectXY = 100;
	lpDevInfo->dpStyleLen = lpDevInfo->dpAspectXY * 2;
	lpDevInfo->dpLogPixelsX = 96;
	lpDevInfo->dpLogPixelsY = 96;
	lpDevInfo->dpDCManage = 0x0004;
	lpDevInfo->dpMLoWin.x = 2400;
	lpDevInfo->dpMLoWin.y = 1500;
	lpDevInfo->dpMLoVpt.x = 640;
	lpDevInfo->dpMLoVpt.y = -400;
	lpDevInfo->dpMHiWin.x = 24000;
	lpDevInfo->dpMHiWin.y = 15000;
	lpDevInfo->dpMHiVpt.x = 640;
	lpDevInfo->dpMHiVpt.y = -400;
	lpDevInfo->dpELoWin.x = 375;
	lpDevInfo->dpELoWin.y = 188;
	lpDevInfo->dpELoVpt.x = 254;
	lpDevInfo->dpELoVpt.y = -127;
	lpDevInfo->dpEHiWin.x = 3750;
	lpDevInfo->dpEHiWin.y = 1875;
	lpDevInfo->dpEHiVpt.x = 254;
	lpDevInfo->dpEHiVpt.y = -127;
	lpDevInfo->dpTwpWin.x = 5400;
	lpDevInfo->dpTwpWin.y = 2700;
	lpDevInfo->dpTwpVpt.x = 254;
	lpDevInfo->dpTwpVpt.y = -127;

	//lpDevInfo->dpVersion = 0x030A;
	//lpDevInfo->dpTechnology = 1; // DT_RASDISPLAY
	//lpDevInfo->dpHorzRes = npdisp.width;
	//lpDevInfo->dpVertRes = npdisp.height;
	//lpDevInfo->dpNumBrushes = -1;
	//lpDevInfo->dpNumPens = -1;
	//lpDevInfo->dpNumFonts = 0;
	//lpDevInfo->dpDEVICEsize = sizeof(NPDISP_PDEVICE);
	//lpDevInfo->dpCurves = 0;
	//lpDevInfo->dpLines = 0;
	//lpDevInfo->dpPolygonals = 0;
	//lpDevInfo->dpText = 0;
	//lpDevInfo->dpClip = 0;
	//lpDevInfo->dpRaster = 0x0001 | 0x0008 | 0x0200;// | 0x0080; // RC_BITBLT | RC_BITMAP64 | RC_DIBTODEV | RC_DI_BITMAP
	//lpDevInfo->dpAspectX = 71;
	//lpDevInfo->dpAspectY = 71;
	//lpDevInfo->dpAspectXY = 100;
	//lpDevInfo->dpStyleLen = lpDevInfo->dpAspectXY * 2;
	//lpDevInfo->dpLogPixelsX = 96;
	//lpDevInfo->dpLogPixelsY = 96;
	//lpDevInfo->dpHorzSize = npdisp.width * 254 / lpDevInfo->dpLogPixelsX / 10;
	//lpDevInfo->dpVertSize = npdisp.height * 254 / lpDevInfo->dpLogPixelsY / 10;
	//lpDevInfo->dpDCManage = 0x0004;
	//lpDevInfo->dpMLoWin.x = 254;
	//lpDevInfo->dpMLoWin.y = 254;
	//lpDevInfo->dpMLoVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpMLoVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpMHiWin.x = 2540;
	//lpDevInfo->dpMHiWin.y = 2540;
	//lpDevInfo->dpMHiVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpMHiVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpELoWin.x = 100;
	//lpDevInfo->dpELoWin.y = 100;
	//lpDevInfo->dpELoVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpELoVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpEHiWin.x = 1000;
	//lpDevInfo->dpEHiWin.y = 1000;
	//lpDevInfo->dpEHiVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpEHiVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;
	//lpDevInfo->dpTwpWin.x = 1440;
	//lpDevInfo->dpTwpWin.y = 1440;
	//lpDevInfo->dpTwpVpt.x = (SINT32)lpDevInfo->dpLogPixelsX;
	//lpDevInfo->dpTwpVpt.y = -(SINT32)lpDevInfo->dpLogPixelsY;

	switch (npdisp.bpp) {
	case 1:
		// 2色
		lpDevInfo->dpBitsPixel = 1;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 1;
		break;
	case 4:
		// 16色
		lpDevInfo->dpBitsPixel = 4;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 16;
		break;
	case 8:
		// 256色
		lpDevInfo->dpBitsPixel = 8;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 256; // 20;
		// lpDevInfo->dpRaster |= 0x0100; // RC_PALETTE
		break;
	case 16:
		// 64k色
		lpDevInfo->dpBitsPixel = 16;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 4096;
		break;
	case 24:
		// 16M色(24bit)
		lpDevInfo->dpBitsPixel = 24;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 4096;
		break;
	case 32:
		// 16M色(32bit)
		lpDevInfo->dpBitsPixel = 32;
		lpDevInfo->dpPlanes = 1;
		lpDevInfo->dpNumColors = 4096;
		break;
	}

	return sizeof(NPDISP_GDIINFO); // ドキュメントに書かれていないがサイズを返さないと駄目
}

static UINT32 npdisp_func_ColorInfo(NPDISP_PDEVICE* lpDestDev, UINT32 dwColorin, UINT32* lpPColor)
{
	int idx;
	UINT32 rgb;

	if (npdisp.bpp > 8) {
		*lpPColor = dwColorin;
		rgb = dwColorin;
	}
	else {
		if (lpPColor) {
			// 論理カラー値を最も近い物理デバイスカラー値へ変換　dwColorinは論理カラー値（RGB値）
			UINT8 r, g, b;
			r = (UINT8)(dwColorin & 0xFF);
			g = (UINT8)((dwColorin >> 8) & 0xFF);
			b = (UINT8)((dwColorin >> 16) & 0xFF);
			if (npdisp.bpp == 8) {
				idx = npdisp_FindNearest256(r, g, b);
			}
			else if (npdisp.bpp == 4) {
				idx = npdisp_FindNearest16(r, g, b);
			}
			else {
				idx = npdisp_FindNearest2(r, g, b);
			}
			*lpPColor = (UINT32)idx;
		}
		else {
			// 物理デバイスカラー値を論理カラー値へ変換　dwColorinは物理デバイスカラー値（パレット番号など）
			idx = dwColorin;
			if (idx < 0 || (1 << npdisp.bpp) <= idx) {
				return 0;
			}
		}

		// 求めたカラー値のRGB表現
		if (npdisp.bpp == 2) {
			// 2色パレットセット
			rgb = ((UINT32)s_npdisp_rgb2[idx].r) | ((UINT32)s_npdisp_rgb2[idx].g << 8) | ((UINT32)s_npdisp_rgb2[idx].b << 16);
		}
		else if (npdisp.bpp == 4) {
			// 16色パレットセット
			rgb = ((UINT32)s_npdisp_rgb16[idx].r) | ((UINT32)s_npdisp_rgb16[idx].g << 8) | ((UINT32)s_npdisp_rgb16[idx].b << 16);
		}
		else if (npdisp.bpp == 8) {
			// 256色パレットセット
			rgb = ((UINT32)s_npdisp_rgb256[idx].r) | ((UINT32)s_npdisp_rgb256[idx].g << 8) | ((UINT32)s_npdisp_rgb256[idx].b << 16);
		}
	}

	return rgb;
}

void npdisp_exec(void) {
	npdisp.longjmpnum = 0;
	npdisp_memread_curpos = 0;
	npdisp_memwrite_curpos = 0;
	npdisp_memread_preloadcount = 0;
	npdisp_selector_cache = 0;
	npdisp_seg_cache = 0;
	UINT16 version = npdisp_readMemory16(npdisp.dataAddr); // バージョンだけ取得
	npdispcs_enter_criticalsection();
	npdisp_cs_execflag = 1;
	UINT32 last_npdisp_memread_bufsize = npdisp_memread_buf.size();
	UINT32 last_npdisp_memwrite_bufwpos = npdisp_memwrite_bufwpos;

	//CPU_REMCLOCK -= 60000;
	if (version >= 1) {
		UINT16 retCode = NPDISP_RETCODE_SUCCESS;
		NPDISP_REQUEST req;
		npdisp_readMemory(&req, npdisp.dataAddr, sizeof(req)); // 全体読み込み
		switch (req.funcOrder) {
		case NPDISP_FUNCORDER_NP2INITIALIZE:
		{
			// 初期化
			npdisp.enabled = 0;
			np2wab.relaystateext = 0;
			np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			npdisp_releaseScreen();

			if (req.parameters.init.width) npdisp.width = req.parameters.init.width;
			if (req.parameters.init.height) npdisp.height = req.parameters.init.height;
			if (npdisp.width > WAB_MAX_WIDTH) npdisp.width = WAB_MAX_WIDTH;
			if (npdisp.height > WAB_MAX_HEIGHT) npdisp.height = WAB_MAX_HEIGHT;
			if (npdisp.width < 160) npdisp.width = 160;
			if (npdisp.height < 100) npdisp.height = 100;
			if (req.parameters.init.bpp) npdisp.bpp = req.parameters.init.bpp;
			if (npdisp.bpp <= 1) npdisp.bpp = 1;
			else if (npdisp.bpp <= 4) npdisp.bpp = 4;
			else if (npdisp.bpp <= 8) npdisp.bpp = 8;
			else if (npdisp.bpp <= 16) npdisp.bpp = 16;
			else if (npdisp.bpp <= 24) npdisp.bpp = 24;
			else if (npdisp.bpp <= 32) npdisp.bpp = 32;
			if (req.parameters.init.dpiX) npdisp.dpiX = req.parameters.init.dpiX;
			if (req.parameters.init.dpiY) npdisp.dpiY = req.parameters.init.dpiY;

			version = 1; // バージョンを返す
			npdisp_writeMemory16(version, npdisp.dataAddr);
			break;
		}
		case NPDISP_FUNCORDER_Enable:
		{
			UINT16 retValue = 0;
			TRACEOUT(("Enable"));
			retValue = npdisp_readMemory16(req.parameters.enable.lpRetValueAddr);
			if (req.parameters.enable.lpDevInfoAddr) {
				char* lpDestDevType;
				char* lpOutputFile;
				NPDISP_DEVMODE data;
				lpDestDevType = npdisp_readMemoryString(req.parameters.enable.lpDestDevTypeAddr);
				lpOutputFile = npdisp_readMemoryString(req.parameters.enable.lpOutputFileAddr);
				if (req.parameters.enable.lpDataAddr) {
					npdisp_readMemory(&data, req.parameters.enable.lpDataAddr, sizeof(data));
				}
				switch (req.parameters.enable.wStyle & 0x7fff) {
				case 0:
				{
					NPDISP_PDEVICE devInfo;
					npdisp_readMemory(&devInfo, req.parameters.enable.lpDevInfoAddr, sizeof(devInfo));
					retValue = npdisp_func_Enable_PDEVICE(&devInfo, req.parameters.enable.wStyle, lpDestDevType, lpOutputFile, req.parameters.enable.lpDataAddr ? &data : NULL);
					npdisp_writeMemory(&devInfo, req.parameters.enable.lpDevInfoAddr, 2);
					npdisp_createScreen();
					npdisp.enabled = 1;
					np2wab.realWidth = npdisp.width;
					np2wab.realHeight = npdisp.height;
					np2wab.relaystateext = 3;
					np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
					npdisp.updated = 1;
					TRACEOUT(("Enable PDEVICE"));
					break;
				}
				case 1:
				{
					NPDISP_GDIINFO gdiInfo;
					npdisp_readMemory(&gdiInfo, req.parameters.enable.lpDevInfoAddr, sizeof(gdiInfo));
					retValue = npdisp_func_Enable_GDIINFO(&gdiInfo, req.parameters.enable.wStyle, lpDestDevType, lpOutputFile, req.parameters.enable.lpDataAddr ? &data : NULL);
					npdisp_writeMemory(&gdiInfo, req.parameters.enable.lpDevInfoAddr, sizeof(gdiInfo));
					TRACEOUT(("Enable GDIINFO"));
					break;
				}
				}
				if (lpDestDevType) free(lpDestDevType);
				if (lpOutputFile) free(lpOutputFile);
			}
			npdisp_writeMemory16(retValue, req.parameters.enable.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Disable:
		{
			TRACEOUT(("Disable"));
			if (req.parameters.disable.lpDestDevAddr) {
				NPDISP_PDEVICE destDev;
				npdisp_readMemory(&destDev, req.parameters.disable.lpDestDevAddr, sizeof(destDev));
				req.parameters.disable.lpDestDevAddr = 0;
				npdisp.enabled = 0;
				np2wab.relaystateext = 0;
				np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			}
			break;
		}
		case NPDISP_FUNCORDER_GetDriverResourceID:
		{
			TRACEOUT(("GetDriverResourceID"));
			// DPI毎のリソース変換？
			SINT16 iResId = req.parameters.GetDriverResourceID.iResId;
			if (req.parameters.GetDriverResourceID.lpResTypeAddr) {
				if (req.parameters.GetDriverResourceID.lpResTypeAddr & 0xffff0000) {
					char* lpResType;
					lpResType = npdisp_readMemoryString(req.parameters.GetDriverResourceID.lpResTypeAddr);
					if (lpResType) free(lpResType);
				}
				else {
					// 上位が0の時はただの値
					SINT16 iResType = req.parameters.GetDriverResourceID.lpResTypeAddr;
				}
			}
			if (npdisp.dpiX >= 96 && (iResId > 32647 || iResId == 1 || iResId == 3)) {
				iResId += 2000;
			}
			npdisp_writeMemory16(iResId, req.parameters.GetDriverResourceID.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_ColorInfo:
		{
			//TRACEOUT(("ColorInfo"));
			// 色の変換
			UINT32 retValue = 0;
			NPDISP_PDEVICE devInfo;
			UINT32 pcolor;
			npdisp_readMemory(&retValue, req.parameters.ColorInfo.lpRetValueAddr, 4);
			npdisp_readMemory(&devInfo, req.parameters.ColorInfo.lpDestDevAddr, sizeof(devInfo));
			npdisp_readMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
			retValue = npdisp_func_ColorInfo(&devInfo, req.parameters.ColorInfo.dwColorin, &pcolor);
			npdisp_writeMemory(&pcolor, req.parameters.ColorInfo.lpPColorAddr, sizeof(pcolor));
			npdisp_writeMemory32(retValue, req.parameters.ColorInfo.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_RealizeObject:
		{
			TRACEOUT(("RealizeObject"));
			// オブジェクト生成と破棄
			UINT32 retValue = 0;
			NPDISP_PDEVICE destDev;
			npdisp_readMemory(&destDev, req.parameters.RealizeObject.lpDestDevAddr, sizeof(destDev));
			if ((SINT16)req.parameters.RealizeObject.wStyle < 0) {
				// 削除
				retValue = 1; // 常に成功したことにする
				switch (-((SINT16)req.parameters.RealizeObject.wStyle)) {
				case 1: // OBJ_PEN
				{
					if (req.parameters.RealizeObject.lpInObjAddr) {
						// 指定されたキーのペンを削除
						NPDISP_PEN pen = { {0}, {NPDISP_PEN_STYLE_SOLID, {1, 0}, 0} };
						npdisp_readMemory(&pen, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_PEN));
						if (pen.key != 0) {
							auto it = npdispwin.pens.find(pen.key);
							if (it != npdispwin.pens.end()) {
								NPDISP_HOSTPEN value = it->second;
								npdispwin.pens.erase(it);
								if (value.pen) {
									DeleteObject(value.pen);
								}
							}
						}
						//memset(&pen, 0, sizeof(NPDISP_PEN));
						//npdisp_writeMemory(&pen, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_PEN));
					}
					TRACEOUT(("RealizeObject Release OBJ_PEN"));
					// サイズを返す
					retValue = sizeof(NPDISP_PEN);
					break;
				}
				case 2: // OBJ_BRUSH
				{
					if (req.parameters.RealizeObject.lpInObjAddr) {
						// 指定されたキーのブラシを削除
						NPDISP_BRUSH brush = { {0}, {NPDISP_BRUSH_STYLE_SOLID, 15, NPDISP_BRUSH_HATCH_HORIZONTAL, 15} };
						npdisp_readMemory(&brush, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_BRUSH));
						if (brush.key != 0) {
							auto it = npdispwin.brushes.find(brush.key);
							if (it != npdispwin.brushes.end()) {
								NPDISP_HOSTBRUSH value = it->second;
								npdispwin.brushes.erase(it);
								if (value.brs) {
									DeleteObject(value.brs);
								}
							}
						}
						//memset(&brush, 0, sizeof(NPDISP_BRUSH));
						//npdisp_writeMemory(&brush, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_BRUSH));
					}
					TRACEOUT(("RealizeObject Release OBJ_BRUSH"));
					// サイズを返す
					retValue = sizeof(NPDISP_BRUSH);
					break;
				}
				case 3: // OBJ_FONT
				{
					TRACEOUT(("RealizeObject Release OBJ_FONT"));
					// サイズを返す
					retValue = 0;
					break;
				}
				case 5: // OBJ_PBITMAP
				{
					TRACEOUT(("RealizeObject Release OBJ_PBITMAP"));
					// サイズを返す
					retValue = 0;
					break;
				}
				default:
				{
					TRACEOUT(("RealizeObject Release UNKNOWN"));
					break;
				}
				}
			}
			else {
				switch (req.parameters.RealizeObject.wStyle) {
				case 1: // OBJ_PEN
				{
					TRACEOUT(("RealizeObject Create OBJ_PEN"));
					if (req.parameters.RealizeObject.lpOutObjAddr) {
						// 作成
						NPDISP_PEN pen = { {0}, {NPDISP_PEN_STYLE_SOLID, {1, 0}, 0} };
						NPDISP_HOSTPEN hostpen = { 0 };
						if (req.parameters.RealizeObject.lpInObjAddr) {
							// 指定した設定で作る
							npdisp_readMemory(&(pen.lpen), req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_LPEN));
						}
						hostpen.lpen = pen.lpen;
						hostpen.pen = CreatePen(pen.lpen.opnStyle, pen.lpen.lopnWidth.x, pen.lpen.lopnColor);
						pen.key = npdispwin.pensIdx;
						npdispwin.pensIdx++;
						if (npdispwin.pensIdx == 0) npdispwin.pensIdx++; // 0は使わないことにする
						npdispwin.pens[pen.key] = hostpen;
						// 書き込み
						npdisp_writeMemory(&pen, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_PEN));
					}
					// サイズを返す
					retValue = sizeof(NPDISP_PEN);
					break;
				}
				case 2: // OBJ_BRUSH
				{
					TRACEOUT(("RealizeObject Create OBJ_BRUSH"));
					if (req.parameters.RealizeObject.lpOutObjAddr) {
						// 作成
						NPDISP_BRUSH brush = { {0}, {NPDISP_BRUSH_STYLE_SOLID, 15, NPDISP_BRUSH_HATCH_HORIZONTAL, 15} };
						NPDISP_HOSTBRUSH hostbrush = { 0 };
						if (req.parameters.RealizeObject.lpInObjAddr) {
							// 指定した設定で作る
							npdisp_readMemory(&(brush.lbrush), req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_LBRUSH));
						}
						hostbrush.lbrush = brush.lbrush;
						if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_SOLID) {
							// 単色ブラシ生成
							//NPDISP_RGB3 npColor = s_npdisp_rgb16[brush.lbrush.lbColor & 0xf];
							//hostbrush.brs = CreateSolidBrush(npColorPat.r | ((UINT32)npColorPat.g << 8) | ((UINT32)npColorPat.b << 16));
							//if (brush.lbrush.lbColor == 0x800000) {
							//	brush.lbrush.lbColor = 0x800000;
							//}
							hostbrush.brs = CreateSolidBrush(brush.lbrush.lbColor);
							if (!hostbrush.brs) {
								TRACEOUT2(("RealizeObject Create OBJ_BRUSH SOLID ERROR!!!!!!!!!!!!!!"));
							}
						}
						else if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
							// ハッチブラシ生成
							hostbrush.brs = CreateHatchBrush(brush.lbrush.lbHatch, brush.lbrush.lbColor);
							if (!hostbrush.brs) {
								TRACEOUT2(("RealizeObject Create OBJ_BRUSH HATCHED ERROR!!!!!!!!!!!!!!"));
							}
						}
						else if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
							// パターンブラシ生成
							NPDISP_PBITMAP patternBmp = { 0 };
							if (npdisp_readMemory(&patternBmp, brush.lbrush.lbColor, sizeof(patternBmp))) {
								NPDISP_WINDOWS_BMPHDC patternBmphdc = { 0 };
								if (npdisp_MakeBitmapFromPBITMAP(&patternBmp, &patternBmphdc, 0)) {
									hostbrush.brs = CreatePatternBrush(patternBmphdc.hBmp);
									npdisp_FreeBitmap(&patternBmphdc);
								}
								else {
									hostbrush.brs = CreateSolidBrush(brush.lbrush.lbColor);
								}
							}
							else {
								hostbrush.brs = CreateSolidBrush(brush.lbrush.lbColor);
							}
							if (!hostbrush.brs) {
								TRACEOUT2(("RealizeObject Create OBJ_BRUSH PATTERN ERROR!!!!!!!!!!!!!!"));
							}
						}
						else if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HOLLOW) {
							// 何もしないブラシ生成
							hostbrush.brs = NULL;
						}
						brush.key = npdispwin.brushesIdx;
						npdispwin.brushesIdx++;
						if (npdispwin.brushesIdx == 0) npdispwin.brushesIdx++; // 0は使わないことにする
						npdispwin.brushes[brush.key] = hostbrush;
						// 書き込み
						npdisp_writeMemory(&brush, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_BRUSH));
					}
					// サイズを返す
					retValue = sizeof(NPDISP_BRUSH);
					break;
				}
				case 3: // OBJ_FONT
				{
					TRACEOUT(("RealizeObject Create OBJ_FONT"));
					//if (req.parameters.RealizeObject.lpOutObjAddr) {
					//	// 作成
						// 失敗ということにして0を返す
					retValue = 0;
					//}
					//else {
					//	// サイズを返す
					//	retValue = sizeof(NPDISP_FONT);
					//}
					break;
				}
				case 5: // OBJ_PBITMAP
				{
					TRACEOUT(("RealizeObject Create OBJ_PBITMAP"));
					//if (req.parameters.RealizeObject.lpOutObjAddr) {
					//	// 作成
					//	NPDISP_PBITMAP inBmp = { 0 };
					//	NPDISP_PBITMAP outBmp = { 0 };
					//	if (req.parameters.RealizeObject.lpInObjAddr) {
					//		npdisp_readMemory(&inBmp, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_PBITMAP));
					//		npdisp_readMemory(&outBmp, req.parameters.RealizeObject.lpInObjAddr, sizeof(NPDISP_PBITMAP));
					//	}
					//	// 書き込み
					//	npdisp_writeMemory(&inBmp, req.parameters.RealizeObject.lpOutObjAddr, sizeof(NPDISP_PBITMAP));
					//}
					//// サイズを返す
					//retValue = sizeof(NPDISP_PBITMAP);
					retValue = 0; // デバイス作成不可
					break;
				}
				default:
				{
					retValue = 0; // デバイス作成不可
					TRACEOUT(("RealizeObject Create UNKNOWN"));
					break;
				}
				}
			}
			npdisp_writeMemory32(retValue, req.parameters.RealizeObject.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Control:
		{
			TRACEOUT(("Control"));
			UINT16 retValue = 0;
			//if (req.parameters.Control.lpDestDevAddr) {
			//	NPDISP_PDEVICE destDev;
			//	npdisp_readMemory(&destDev, req.parameters.Control.lpDestDevAddr, sizeof(destDev));
			//	if (req.parameters.Control.wFunction == 8) {
			//		// QUERYESCSUPPORT
			//		UINT16 escNum = npdisp_readMemory16(req.parameters.Control.lpInDataAddr);
			//		if (escNum == 8) retValue = 1; // QUERYESCSUPPORTは必ずサポート
			//	}
			//	// 必要ならサポート
			//}
			npdisp_writeMemory16(retValue, req.parameters.Control.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_BitBlt:
		{
			//TRACEOUT(("BitBlt"));
			int dstDevType = 0;
			int srcDevType = 0;
			int hasDstDev = 0;
			int hasSrcDev = 0;
			UINT16 retValue = 0;
			if (req.parameters.BitBlt.lpDestDevAddr) {
				if (npdisp_readMemory(&dstDevType, req.parameters.BitBlt.lpDestDevAddr, 2)) {
					hasDstDev = 1;
				}
			}
			if (req.parameters.BitBlt.lpSrcDevAddr) {
				if (npdisp_readMemory(&srcDevType, req.parameters.BitBlt.lpSrcDevAddr, 2)) {
					hasSrcDev = 1;
				}
			}
			if (req.parameters.BitBlt.lpDestDevAddr == 0x112f0000) {
				TRACEOUT2(("CHECK"));
			}
			if (npdisp.longjmpnum == 0) {
				if (dstDevType != 0 && srcDevType != 0) {
					// VRAM -> VRAM
					if (req.parameters.BitBlt.lpPBrushAddr) {
						// ブラシがあれば選択
						NPDISP_BRUSH brush = { 0 };
						if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
							if (brush.key != 0) {
								auto it = npdispwin.brushes.find(brush.key);
								if (it != npdispwin.brushes.end()) {
									NPDISP_HOSTBRUSH value = it->second;
									if (value.brs) {
										if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
											TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
										}
										NPDISP_DRAWMODE drawMode = { 0 };
										int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
										if (npdisp.bpp == 1) {
											drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
											drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
										}
										SelectObject(npdispwin.hdc, value.brs);
										if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
											SetBkColor(npdispwin.hdc, brush.lbrush.lbBkColor);
										}
										if (hasDrawMode) {
												SetBkColor(npdispwin.hdc, drawMode.LbkColor);
												SetTextColor(npdispwin.hdc, drawMode.LTextColor);
											if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
											}
											SetBkMode(npdispwin.hdc, drawMode.bkMode);
											SetROP2(npdispwin.hdc, drawMode.Rop2);
										}
									}
								}
							}
						}
					}
					if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
						TRACEOUT2(("BitBlt Check VRAM => VRAM ROP:%08x", req.parameters.BitBlt.Rop3));
					}
					switch (req.parameters.BitBlt.Rop3) {
					case 0x00CC0020: // SRCCOPY
					case 0x00EE0086: // SRCPAINT
					case 0x008800C6: // SRCAND
					case 0x00660046: // SRCINVERT
					case 0x00440328: // SRCERASE 
					case 0x00330008: // NOTSRCCOPY
					case 0x001100A6: // NOTSRCERASE
					case 0x00C000CA: // MERGECOPY
					case 0x00BB0226: // MERGEPAINT
					case 0x00F00021: // PATCOPY 
					case 0x00FB0A09: // PATPAINT 
					case 0x005A0049: // PATINVERT 
					case 0x00550009: // DSTINVERT
						if (hasDstDev && hasSrcDev) {
							BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
						}
						break;
					case 0x00000042: // BLACKNESS
						if (hasDstDev) {
							BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
						}
						break;
					case 0x00FF0062: // WHITENESS
						if (hasDstDev) {
							BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
						}
						break;
					default:
						BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
						break;
					}
					SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
					npdisp.updated = 1;
					retValue = 1; // 成功
				}
				else if (dstDevType != 0 && srcDevType == 0) {
					// MEM -> VRAM
					//TRACEOUT(("BitBlt MEM -> VRAM"));
					NPDISP_PBITMAP srcPBmp;
					if (req.parameters.BitBlt.lpSrcDevAddr && npdisp_readMemory(&srcPBmp, req.parameters.BitBlt.lpSrcDevAddr, sizeof(NPDISP_PBITMAP))) {
						NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
						npdisp_PreloadBitmapFromPBITMAP(&srcPBmp, 0);
						if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&srcPBmp, &bmphdc, 0)) {
							//if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
								TRACEOUT2(("BitBlt Check %08x => VRAM ROP:%08x", req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.Rop3));
							//}
							if (req.parameters.BitBlt.lpPBrushAddr) {
								// ブラシがあれば選択
								NPDISP_BRUSH brush = { 0 };
								if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
									if (brush.key != 0) {
										auto it = npdispwin.brushes.find(brush.key);
										if (it != npdispwin.brushes.end()) {
											NPDISP_HOSTBRUSH value = it->second;
											if (value.brs) {
												if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
													TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
												}
												NPDISP_DRAWMODE drawMode = { 0 };
												int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												if (npdisp.bpp == 1) {
													drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
													drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
												}
												SelectObject(npdispwin.hdc, value.brs);
												if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
													SetBkColor(npdispwin.hdc, brush.lbrush.lbBkColor);
												}
												if (hasDrawMode) {
														SetBkColor(npdispwin.hdc, drawMode.LbkColor);
														SetTextColor(npdispwin.hdc, drawMode.LTextColor);
													if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
													}
													SetBkMode(npdispwin.hdc, drawMode.bkMode);
													SetROP2(npdispwin.hdc, drawMode.Rop2);
												}
											}
										}
									}
								}
							}
							switch (req.parameters.BitBlt.Rop3) {
							case 0x00CC0020: // SRCCOPY
							case 0x00EE0086: // SRCPAINT
							case 0x008800C6: // SRCAND
							case 0x00660046: // SRCINVERT
							case 0x00440328: // SRCERASE 
							case 0x00330008: // NOTSRCCOPY
							case 0x001100A6: // NOTSRCERASE
							case 0x00C000CA: // MERGECOPY
							case 0x00BB0226: // MERGEPAINT
							case 0x00F00021: // PATCOPY 
							case 0x00FB0A09: // PATPAINT 
							case 0x005A0049: // PATINVERT 
							case 0x00550009: // DSTINVERT
								if (hasDstDev) {
									//BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
									//if (req.parameters.BitBlt.lpSrcDevAddr == 0x112f0000) {
									//	//char test[16];
									//	BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
									//	//sprintf(test, "%08x", req.parameters.BitBlt.lpSrcDevAddr);
									//	//SetBkColor(npdispwin.hdc, 0xffffff);
									//	//SetTextColor(npdispwin.hdc, 0x000000);
									//	//SetBkMode(npdispwin.hdc, OPAQUE);
									//	//TextOutA(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, test, strlen(test));
									//}
									//else {
										BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);

									//}
									//if (req.parameters.BitBlt.lpSrcDevAddr == 0x1f870000) {
									//	BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
									//}
								}
								break;
							case 0x00000042: // BLACKNESS
								if (hasDstDev) {
									BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
								}
								break;
							case 0x00FF0062: // WHITENESS
								if (hasDstDev) {
									BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
								}
								break;
							default:
								if (hasDstDev) {
									BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, bmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
								}
								break;
							}
							SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
							npdisp.updated = 1;
							retValue = 1; // 成功

							npdisp_FreeBitmap(&bmphdc);
						}
					}
					else if (req.parameters.BitBlt.lpPBrushAddr) {
						NPDISP_BRUSH brush = { 0 };
						if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
							if (brush.key != 0) {
								auto it = npdispwin.brushes.find(brush.key);
								if (it != npdispwin.brushes.end()) {
									NPDISP_HOSTBRUSH value = it->second;
									if (value.brs) {
										if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
											TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
										}
										NPDISP_DRAWMODE drawMode = { 0 };
										int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
										if (npdisp.bpp == 1) {
											drawMode.LbkColor = drawMode.bkColor ? 0xffffff : 0;
											drawMode.LTextColor = drawMode.TextColor ? 0xffffff : 0;
										}
										SelectObject(npdispwin.hdc, value.brs);
										if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
											SetBkColor(npdispwin.hdc, brush.lbrush.lbBkColor);
										}
										if (hasDrawMode) {
												SetBkColor(npdispwin.hdc, drawMode.LbkColor);
												SetTextColor(npdispwin.hdc, drawMode.LTextColor);
											//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
											//}
											SetBkMode(npdispwin.hdc, drawMode.bkMode);
											SetROP2(npdispwin.hdc, drawMode.Rop2);
										}
										//if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
										PatBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3);
										//}
										//else {
										//	Rectangle(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wDestX + req.parameters.BitBlt.wXext, req.parameters.BitBlt.wDestY + req.parameters.BitBlt.wYext);
										//}
										//BitBlt(npdispwin.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
										npdisp.updated = 1;

										SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
									}
								}
							}
						}
					}
					retValue = 1; // 成功
				}
				else if (dstDevType == 0 && srcDevType != 0) {
					// VRAM -> MEM
					//TRACEOUT(("BitBlt VRAM -> MEM"));
					NPDISP_PBITMAP dstPBmp;
					if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
						NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
						npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0);
						if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
							if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
								TRACEOUT2(("BitBlt Check VRAM BRUSH -> %08x", req.parameters.BitBlt.lpDestDevAddr));
							}
							if (req.parameters.BitBlt.lpPBrushAddr) {
								// ブラシがあれば選択
								NPDISP_BRUSH brush = { 0 };
								if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
									if (brush.key != 0) {
										auto it = npdispwin.brushes.find(brush.key);
										if (it != npdispwin.brushes.end()) {
											NPDISP_HOSTBRUSH value = it->second;
											if (value.brs) {
												if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
													TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
												}
												NPDISP_DRAWMODE drawMode = { 0 };
												int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												if (npdisp.bpp == 1) {
													drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
													drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
												}
												SelectObject(bmphdc.hdc, value.brs);
												if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
													SetBkColor(bmphdc.hdc, brush.lbrush.lbBkColor);
												}
												if (hasDrawMode) {
														SetBkColor(bmphdc.hdc, drawMode.LbkColor);
														SetTextColor(bmphdc.hdc, drawMode.LTextColor);
													if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
													}
													SetBkMode(bmphdc.hdc, drawMode.bkMode);
													SetROP2(bmphdc.hdc, drawMode.Rop2);
												}
											}
										}
									}
								}
							}
							switch (req.parameters.BitBlt.Rop3) {
							case 0x00CC0020: // SRCCOPY
							case 0x00EE0086: // SRCPAINT
							case 0x008800C6: // SRCAND
							case 0x00660046: // SRCINVERT
							case 0x00440328: // SRCERASE 
							case 0x00330008: // NOTSRCCOPY
							case 0x001100A6: // NOTSRCERASE
							case 0x00C000CA: // MERGECOPY
							case 0x00BB0226: // MERGEPAINT
							case 0x00F00021: // PATCOPY 
							case 0x00FB0A09: // PATPAINT 
							case 0x005A0049: // PATINVERT 
							case 0x00550009: // DSTINVERT
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
								}
								break;
							case 0x00000042: // BLACKNESS
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
								}
								break;
							case 0x00FF0062: // WHITENESS
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
								}
								break;
							default:
								if (hasDstDev) {
									BitBlt(bmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, npdispwin.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
								}
								break;
							}
							SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
							npdisp.updated = 1;
							retValue = 1; // 成功

							npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);

							npdisp_FreeBitmap(&bmphdc);
						}
					}
					retValue = 1; // 成功
				}
				else if (dstDevType == 0 && srcDevType == 0) {
					// MEM -> MEM
					//TRACEOUT(("BitBlt MEM -> MEM"));
					NPDISP_PBITMAP dstPBmp;
					retValue = 1;
					if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
						if (req.parameters.BitBlt.lpSrcDevAddr) {
							NPDISP_PBITMAP srcPBmp;
							if (npdisp_readMemory(&srcPBmp, req.parameters.BitBlt.lpSrcDevAddr, sizeof(NPDISP_PBITMAP))) {
								npdisp_PreloadBitmapFromPBITMAP(&srcPBmp, 0);
								npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 1);
								NPDISP_WINDOWS_BMPHDC srcbmphdc = { 0 };
								if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&srcPBmp, &srcbmphdc, 0)) {
									NPDISP_WINDOWS_BMPHDC dstbmphdc = { 0 };
									if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &dstbmphdc, 1)) {
										if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
											//BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
											TRACEOUT2(("BitBlt Check %08x -> %08x ROP:%08x", req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.lpDestDevAddr, req.parameters.BitBlt.Rop3));
										}
										if (req.parameters.BitBlt.lpPBrushAddr) {
											// ブラシがあれば選択
											NPDISP_BRUSH brush = { 0 };
											if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
												if (brush.key != 0) {
													auto it = npdispwin.brushes.find(brush.key);
													if (it != npdispwin.brushes.end()) {
														NPDISP_HOSTBRUSH value = it->second;
														if (value.brs) {
															if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
																TRACEOUT2(("BitBlt Check MEM BRUSH %08x => VRAM", req.parameters.BitBlt.lpSrcDevAddr));
															}
															NPDISP_DRAWMODE drawMode = { 0 };
															int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
															if (npdisp.bpp == 1) {
																drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
																drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
															}
															SelectObject(dstbmphdc.hdc, value.brs);
															if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
																SetBkColor(dstbmphdc.hdc, brush.lbrush.lbBkColor);
															}
															if (hasDrawMode) {
																	SetBkColor(dstbmphdc.hdc, drawMode.LbkColor);
																	SetTextColor(dstbmphdc.hdc, drawMode.LTextColor);
																if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {
																}
																SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
																SetROP2(dstbmphdc.hdc, drawMode.Rop2);
															}
														}
													}
												}
											}
										}
										//BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
										switch (req.parameters.BitBlt.Rop3) {
										case 0x00CC0020: // SRCCOPY
										case 0x00EE0086: // SRCPAINT
										case 0x008800C6: // SRCAND
										case 0x00660046: // SRCINVERT
										case 0x00440328: // SRCERASE 
										case 0x00330008: // NOTSRCCOPY
										case 0x001100A6: // NOTSRCERASE
										case 0x00C000CA: // MERGECOPY
										case 0x00BB0226: // MERGEPAINT
										case 0x00F00021: // PATCOPY 
										case 0x00FB0A09: // PATPAINT 
										case 0x005A0049: // PATINVERT 
										case 0x00550009: // DSTINVERT
											if (hasDstDev && hasSrcDev) {
												//	NPDISP_DRAWMODE drawMode = { 0 };
												//	int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												//if (req.parameters.BitBlt.Rop3 == 0x008800c6) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
												//}
												//if (req.parameters.BitBlt.lpSrcDevAddr == 0x16670000) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
												//	//char test[16];
												//	//SelectObject(dstbmphdc.hdc, npdispwin.hFont); // DEBUG
												//	//sprintf(test, "%08x", req.parameters.BitBlt.lpSrcDevAddr);
												//	//SetBkColor(dstbmphdc.hdc, 0xffffff);
												//	//SetTextColor(dstbmphdc.hdc, 0x000000);
												//	//SetBkMode(dstbmphdc.hdc, OPAQUE);
												//	//TextOutA(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, test, strlen(test));
												//}
												//else if (req.parameters.BitBlt.lpSrcDevAddr == 0x166f0000) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
												//	//char test[16];
												//	//SelectObject(dstbmphdc.hdc, npdispwin.hFont); // DEBUG
												//	//sprintf(test, "%08x", req.parameters.BitBlt.lpSrcDevAddr);
												//	//SetBkColor(dstbmphdc.hdc, 0xffffff);
												//	//SetTextColor(dstbmphdc.hdc, 0x000000);
												//	//SetBkMode(dstbmphdc.hdc, OPAQUE);
												//	//TextOutA(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, test, strlen(test));
												//}
												//else {
													BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3);
												//}
											}
											break;
										case 0x00000042: // BLACKNESS
											if (hasDstDev) {
												BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, BLACKNESS);
											}
											break;
										case 0x00FF0062: // WHITENESS
											if (hasDstDev) {
												BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, NULL, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
											}
											break;
										default:
											if (hasDstDev && hasSrcDev) {
												//	NPDISP_DRAWMODE drawMode = { 0 };
												//	int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												//if (req.parameters.BitBlt.Rop3 == 0x008800c6) {
												//	BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, WHITENESS);
												//}
												if (!BitBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, srcbmphdc.hdc, req.parameters.BitBlt.wSrcX, req.parameters.BitBlt.wSrcY, req.parameters.BitBlt.Rop3)) {
													TRACEOUT2(("BitBlt Error"));
												}
											}
											break;
										}
										SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
										npdisp.updated = 1;
										retValue = 1; // 成功

										npdisp_WriteBitmapToPBITMAP(&dstPBmp, &dstbmphdc);

										npdisp_FreeBitmap(&dstbmphdc);
									}
									npdisp_FreeBitmap(&srcbmphdc);
								}
							}
						}
						else if (req.parameters.BitBlt.lpPBrushAddr)
						{
							NPDISP_BRUSH brush = { 0 };
							if (npdisp_readMemory(&brush, req.parameters.BitBlt.lpPBrushAddr, sizeof(NPDISP_BRUSH))) {
								if (brush.key != 0) {
									auto it = npdispwin.brushes.find(brush.key);
									if (it != npdispwin.brushes.end()) {
										NPDISP_HOSTBRUSH value = it->second;
										if (value.brs) {
											NPDISP_WINDOWS_BMPHDC dstbmphdc = { 0 };
											npdisp_PreloadBitmapFromPBITMAP(&dstPBmp, 0);
											if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &dstbmphdc, 0)) {
												if (req.parameters.BitBlt.wXext == 32 && req.parameters.BitBlt.wYext == 32) {
													TRACEOUT2(("BitBlt Check MEM BRUSH %08x -> %08x", req.parameters.BitBlt.lpSrcDevAddr, req.parameters.BitBlt.lpDestDevAddr));
												}
												NPDISP_DRAWMODE drawMode = { 0 };
												int hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.BitBlt.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE));
												if (npdisp.bpp == 1) {
													drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
													drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
												}

												HBRUSH oldBrush = (HBRUSH)SelectObject(dstbmphdc.hdc, value.brs);
												if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_HATCHED) {
													SetBkColor(dstbmphdc.hdc, brush.lbrush.lbBkColor);
												}
												if (hasDrawMode) {
													SetBkColor(dstbmphdc.hdc, drawMode.LbkColor);
														SetTextColor(dstbmphdc.hdc, drawMode.LTextColor);
													if (brush.lbrush.lbStyle == NPDISP_BRUSH_STYLE_PATTERN) {

													}
													SetBkMode(dstbmphdc.hdc, drawMode.bkMode);
													SetROP2(dstbmphdc.hdc, drawMode.Rop2);
												}
												PatBlt(dstbmphdc.hdc, req.parameters.BitBlt.wDestX, req.parameters.BitBlt.wDestY, req.parameters.BitBlt.wXext, req.parameters.BitBlt.wYext, req.parameters.BitBlt.Rop3);
												SelectObject(dstbmphdc.hdc, oldBrush);
												npdisp.updated = 1;

												npdisp_WriteBitmapToPBITMAP(&dstPBmp, &dstbmphdc);

												npdisp_FreeBitmap(&dstbmphdc);
											}
										}
									}
								}
							}
						}
					}
				}
			}
			else {
				retValue = 1; // 成功したことにする
			}
			npdisp_writeMemory16(retValue, req.parameters.BitBlt.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_DeviceBitmapBits:
		{
			TRACEOUT(("DeviceBitmapBits"));
			UINT16 retValue = 0;
			if (req.parameters.DeviceBitmapBits.lpBitmapAddr) {
				NPDISP_PBITMAP tgtPBmp;
				if (npdisp_readMemory(&tgtPBmp, req.parameters.DeviceBitmapBits.lpBitmapAddr, sizeof(NPDISP_PBITMAP))) {
					BITMAPINFOHEADER biHeader = { 0 };
					npdisp_readMemory(&biHeader, req.parameters.DeviceBitmapBits.lpBitmapInfoAddr, sizeof(BITMAPINFOHEADER));
					if (biHeader.biPlanes == 1 && (biHeader.biBitCount == 1 || biHeader.biBitCount == 4 || biHeader.biBitCount == 8 || biHeader.biBitCount == 24 || biHeader.biBitCount == 32) && biHeader.biHeight > req.parameters.DeviceBitmapBits.iStart) {
						NPDISP_WINDOWS_BMPHDC tgtbmphdc = { 0 };
						int stride = ((biHeader.biWidth * biHeader.biBitCount + 31) / 32) * 4;
						int height = req.parameters.DeviceBitmapBits.cScans;
						int beginLine = biHeader.biHeight - height - req.parameters.DeviceBitmapBits.iStart;
						int lpbiLen = 0;
						if (biHeader.biBitCount <= 8) {
							lpbiLen = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << biHeader.biBitCount);
						}
						else {
							lpbiLen = sizeof(BITMAPINFO);
						}
						BITMAPINFO* lpbi;
						lpbi = (BITMAPINFO*)malloc(lpbiLen);
						if (lpbi) {
							npdisp_readMemory(lpbi, req.parameters.DeviceBitmapBits.lpBitmapInfoAddr, lpbiLen);
							npdisp_PreloadBitmapFromPBITMAP(&tgtPBmp, 0, beginLine, height);
							npdisp_preloadMemory(req.parameters.DeviceBitmapBits.lpDIBitsAddr, stride * height);
							if (npdisp.longjmpnum == 0 && npdisp_MakeBitmapFromPBITMAP(&tgtPBmp, &tgtbmphdc, 0, beginLine, height)) {
								void* pBits = NULL;
								if (req.parameters.DeviceBitmapBits.iStart + height > biHeader.biHeight) {
									height = biHeader.biHeight - req.parameters.DeviceBitmapBits.iStart;
								}
								lpbi->bmiHeader.biHeight = height;
								HBITMAP hBmp = CreateDIBSection(npdispwin.hdc, lpbi, DIB_RGB_COLORS, &pBits, NULL, 0);
								if (hBmp) {
									HDC hdc = npdispwin.hdcCache[1];
									HGDIOBJ hOldBmp = SelectObject(hdc, hBmp);
									npdisp_readMemory(pBits, req.parameters.DeviceBitmapBits.lpDIBitsAddr, stride * height);
									if (req.parameters.DeviceBitmapBits.fGet) {
										// Get Bits
										//BitBlt(hdc, 0, req.parameters.DeviceBitmapBits.iStart, biHeader.biWidth, req.parameters.DeviceBitmapBits.cScans, NULL, 0, 0, WHITENESS);
										BitBlt(hdc, 0, 0, biHeader.biWidth, height, tgtbmphdc.hdc, 0, biHeader.biHeight - height - req.parameters.DeviceBitmapBits.iStart, SRCCOPY);
										retValue = height;
										npdisp_writeMemory(pBits, req.parameters.DeviceBitmapBits.lpDIBitsAddr, stride * height);

										if (tgtbmphdc.lpbi->bmiHeader.biWidth == 32 && tgtbmphdc.lpbi->bmiHeader.biHeight == 32) {
											TRACEOUT2(("DeviceBitmapBits Check Get %08x", req.parameters.DeviceBitmapBits.lpBitmapAddr));
										}
									}
									else {
										// Set Bits
										//BitBlt(tgtbmphdc.hdc, 0, req.parameters.DeviceBitmapBits.iStart, biHeader.biWidth, req.parameters.DeviceBitmapBits.cScans, NULL, 0, 0, WHITENESS);
										BitBlt(tgtbmphdc.hdc, 0, biHeader.biHeight - height - req.parameters.DeviceBitmapBits.iStart, biHeader.biWidth, height, hdc, 0, 0, SRCCOPY);
										retValue = height;
										npdisp.updated = 1;
										npdisp_WriteBitmapToPBITMAP(&tgtPBmp, &tgtbmphdc, beginLine, height);

										if (tgtbmphdc.lpbi->bmiHeader.biWidth == 32 && tgtbmphdc.lpbi->bmiHeader.biHeight == 32) {
											TRACEOUT2(("DeviceBitmapBits Check Set %08x", req.parameters.DeviceBitmapBits.lpBitmapAddr));
										}
									}
									SelectObject(hdc, hOldBmp);
									DeleteObject(hBmp);
								}
								npdisp_FreeBitmap(&tgtbmphdc);
							}
							free(lpbi);
						}
						else {
							retValue = 0;
						}
					}
				}
			}
			npdisp_writeMemory16(retValue, req.parameters.DeviceBitmapBits.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_StrBlt:
		case NPDISP_FUNCORDER_ExtTextOut:
		{
			if (req.funcOrder == NPDISP_FUNCORDER_StrBlt) {
				TRACEOUT(("StrBlt"));
				req.parameters.extTextOut.lpCharWidthsAddr = 0;
				req.parameters.extTextOut.lpOpaqueRectAddr = 0;
				req.parameters.extTextOut.wOptions = 0;
			}
			else {
				TRACEOUT(("ExtTextOut"));
			}
			UINT32 retValue = 0;
			UINT8* lpText = (UINT8*)npdisp_readMemoryStringWithCount(req.parameters.extTextOut.lpStringAddr, req.parameters.extTextOut.wCount < 0 ? -req.parameters.extTextOut.wCount : req.parameters.extTextOut.wCount);
			if (lpText) {
				if (npdisp.longjmpnum == 0) {
					int i;
					RECT cliprect = { 0 };
					NPDISP_RECT rectTmp = { 0 };
					NPDISP_RECT opaquerect = { 0 };
					npdisp_readMemory(&rectTmp, req.parameters.extTextOut.lpClipRectAddr, sizeof(NPDISP_RECT));
					if (req.parameters.extTextOut.lpOpaqueRectAddr) npdisp_readMemory(&opaquerect, req.parameters.extTextOut.lpOpaqueRectAddr, sizeof(NPDISP_RECT));
					cliprect.top = rectTmp.top;
					cliprect.left = rectTmp.left;
					cliprect.bottom = rectTmp.bottom;
					cliprect.right = rectTmp.right;
					NPDISP_DRAWMODE drawMode = { 0 };
					int hasDrawMode = 0;
					if (req.parameters.extTextOut.lpDrawModeAddr) hasDrawMode = npdisp_readMemory(&drawMode, req.parameters.extTextOut.lpDrawModeAddr, sizeof(NPDISP_DRAWMODE)); 
					if (npdisp.bpp == 1) {
						drawMode.LbkColor = 0xffffff;// drawMode.bkColor ? 0xffffff : 0;
						drawMode.LTextColor = 0;// drawMode.TextColor ? 0xffffff : 0;
					}
					UINT32 bkColor = 0xffffff;
					UINT32 textColor = 0x000000;
					if (hasDrawMode) {
						bkColor = drawMode.LbkColor;
						textColor = drawMode.LTextColor;
					}
					//HGDIOBJ oldFont = SelectObject(tgtDC, npdispwin.hFont);
					NPDISP_FONTINFO fontInfo;
					if (npdisp_readMemory(&fontInfo, req.parameters.extTextOut.lpFontInfoAddr, sizeof(NPDISP_FONTINFO))) {
						SIZE sz = { 0, fontInfo.dfPixHeight };
						int loopLen = req.parameters.extTextOut.wCount >= 0 ? req.parameters.extTextOut.wCount : -req.parameters.extTextOut.wCount;
						for (i = 0; i < loopLen; i++) {
							NPDISP_FONTCHARINFO3 charInfo;
							int charIdx = (int)lpText[i] - fontInfo.dfFirstChar;
							if (fontInfo.dfLastChar < charIdx) {
								charIdx = fontInfo.dfDefaultChar;
							}
							if (req.parameters.extTextOut.lpCharWidthsAddr) {
								sz.cx += npdisp_readMemory16(req.parameters.extTextOut.lpCharWidthsAddr + i * 2);
							}
							else {
								if (npdisp_readMemory(&charInfo, req.parameters.extTextOut.lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
									sz.cx += charInfo.width;
								}
							}
						}
						//GetTextExtentPoint32A(tgtDC, "NEKO", strlen("NEKO"), &sz);
						retValue = (sz.cx) | (sz.cy << 16);
						if (req.parameters.extTextOut.wCount < 0) {
							// nothing to do
						}
						else if (req.parameters.extTextOut.wCount == 0) {
							// 塗りつぶし
							NPDISP_PBITMAP dstPBmp;
							NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
							int dstDevType = 0;
							HDC tgtDC = npdispwin.hdc;
							npdisp_readMemory(&dstDevType, req.parameters.extTextOut.lpDestDevAddr, 2);
							if (dstDevType == 0) {
								// memory 
								if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
									if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
										tgtDC = bmphdc.hdc;
									}
								}
							}
							if (npdisp.longjmpnum == 0) {
								RECT gdiopaquerect = { 0 };
								gdiopaquerect.top = opaquerect.top;
								gdiopaquerect.left = opaquerect.left;
								gdiopaquerect.bottom = opaquerect.bottom;
								gdiopaquerect.right = opaquerect.right;
								HBRUSH hBrush = CreateSolidBrush(drawMode.LbkColor);
								FillRect(tgtDC, &gdiopaquerect, hBrush);
								DeleteObject(hBrush);
								if (bmphdc.hdc) {
									// 書き戻し
									npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);
								}
							}
							if (bmphdc.hdc) {
								npdisp_FreeBitmap(&bmphdc);
							}
						}
						else {
							// 描画
							//ExtTextOutA(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, req.parameters.extTextOut.wOptions, NULL, "NEKO", 4, NULL);

							BITMAPINFO* lpbi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 2);
							if (lpbi) {
								HDC hdcText = CreateCompatibleDC(NULL);
								if (hdcText) {
									//lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
									//lpbi->bmiHeader.biWidth = sz.cx;
									//lpbi->bmiHeader.biHeight = -sz.cy;
									//lpbi->bmiHeader.biPlanes = 1;
									//lpbi->bmiHeader.biBitCount = 1;
									//lpbi->bmiHeader.biCompression = BI_RGB;
									//lpbi->bmiHeader.biSizeImage = 0;
									//lpbi->bmiHeader.biXPelsPerMeter = 0;
									//lpbi->bmiHeader.biYPelsPerMeter = 0;
									//lpbi->bmiHeader.biClrUsed = 0;
									//lpbi->bmiHeader.biClrImportant = 0;
									//lpbi->bmiColors[0].rgbRed = 0xff;
									//lpbi->bmiColors[0].rgbGreen = 0xff;
									//lpbi->bmiColors[0].rgbBlue = 0xff;
									//lpbi->bmiColors[0].rgbReserved = 0x00;
									//lpbi->bmiColors[1].rgbRed = 0x00;
									//lpbi->bmiColors[1].rgbGreen = 0x00;
									//lpbi->bmiColors[1].rgbBlue = 0x00;
									//lpbi->bmiColors[1].rgbReserved = 0x00;
									int stride = ((sz.cx + 7) / 8 + 1) / 2 * 2;
									void* pBits = (char*)malloc(stride * sz.cy);
									if (pBits) {
										HGDIOBJ hbmpOld;
										memset(pBits, 0x00, stride* sz.cy);
										// てんそう
										int baseXbyte = 0;
										int baseXbit = 0;
										for (i = 0; i < loopLen; i++) {
											NPDISP_FONTCHARINFO3 charInfo;
											int charIdx = (int)lpText[i] - fontInfo.dfFirstChar;
											if (fontInfo.dfLastChar < charIdx) {
												charIdx = fontInfo.dfDefaultChar;
											}
											if (npdisp_readMemory(&charInfo, req.parameters.extTextOut.lpFontInfoAddr + sizeof(NPDISP_FONTINFO) + sizeof(NPDISP_FONTCHARINFO3) * charIdx, sizeof(NPDISP_FONTCHARINFO3))) {
												int y, yx;
												int charXLen = (charInfo.width + 7) / 8;
												for (yx = 0; yx < charXLen; yx++) {
													int curWidth = charInfo.width - yx * 8;
													if (curWidth > 8) curWidth = 8;
													int bitMask = ((1 << curWidth) - 1) << (8 - curWidth);
													int dstBitMask1 = (bitMask >> baseXbit) & 0xff;
													int dstBitMask2 = (bitMask << (8 - baseXbit)) & 0xff;
													char* buf = (char*)pBits + baseXbyte + yx;
													for (y = 0; y < sz.cy; y++) {
														UINT8 data = npdisp_readMemory8With32Offset(fontInfo.dfBitsPointer >> 16, charInfo.offset + yx * sz.cy + y) & bitMask;
														UINT8 data1 = (data >> baseXbit) & 0xff;
														UINT8 data2 = (data << (8 - baseXbit)) & 0xff;
														*buf = (*buf & ~dstBitMask1) | (data1 & dstBitMask1);
														if (dstBitMask2) {
															*(buf + 1) = (*(buf + 1) & ~dstBitMask2) | (data2 & dstBitMask2);
														}
														buf += stride;
													}
												}
											}
											if (req.parameters.extTextOut.lpCharWidthsAddr) {
												int curCharWidth = npdisp_readMemory16(req.parameters.extTextOut.lpCharWidthsAddr + i * 2);
												baseXbyte += (baseXbit + curCharWidth) / 8;
												baseXbit = (baseXbit + curCharWidth) % 8;
											}
											else {
												baseXbyte += (baseXbit + charInfo.width) / 8;
												baseXbit = (baseXbit + charInfo.width) % 8;
											}
										}
										HBITMAP hBmp = CreateBitmap(sz.cx, sz.cy, 1, 1, pBits);
										if (hBmp) {
											HDC hdcTextInv = CreateCompatibleDC(NULL);
											if (hdcTextInv) {
												HBITMAP hInvBmp = CreateBitmap(sz.cx, sz.cy, 1, 1, NULL);
												if (hInvBmp) {
													HGDIOBJ hbmpOld;
													HGDIOBJ hbmpInvOld;
													hbmpOld = SelectObject(hdcText, hBmp);
													hbmpInvOld = SelectObject(hdcTextInv, hInvBmp);
													//SetBkColor(tgtDC, 0xffffff);
													//SetTextColor(tgtDC, 0x000000);
													//SetBkMode(tgtDC, TRANSPARENT);
													//BitBlt(hdcText, 0, 0, sz.cx, sz.cy, NULL, 0, 0, BLACKNESS);
													SetBkColor(hdcTextInv, 0xffffffff);
													SetTextColor(hdcTextInv, 0x00000000);
													SetBkColor(hdcText, bkColor);
													SetTextColor(hdcText, textColor);
													BitBlt(hdcTextInv, 0, 0, sz.cx, sz.cy, hdcText, 0, 0, NOTSRCCOPY);
													//SetBkMode(hdcText, TRANSPARENT);
													NPDISP_PBITMAP dstPBmp;
													NPDISP_WINDOWS_BMPHDC bmphdc = { 0 };
													int dstDevType = 0;
													HDC tgtDC = npdispwin.hdc;
													npdisp_readMemory(&dstDevType, req.parameters.extTextOut.lpDestDevAddr, 2);
													if (dstDevType == 0) {
														// memory 
														if (req.parameters.BitBlt.lpDestDevAddr && npdisp_readMemory(&dstPBmp, req.parameters.BitBlt.lpDestDevAddr, sizeof(NPDISP_PBITMAP))) {
															if (npdisp_MakeBitmapFromPBITMAP(&dstPBmp, &bmphdc, 0)) {
																tgtDC = bmphdc.hdc;
															}
														}
													}
													if (npdisp.longjmpnum == 0) {
														HRGN hRgn = req.parameters.extTextOut.lpClipRectAddr ? CreateRectRgn(cliprect.left, cliprect.top, cliprect.right, cliprect.bottom) : NULL;
														if (hRgn) {
															SelectClipRgn(tgtDC, hRgn);
														}
														if (req.parameters.extTextOut.lpOpaqueRectAddr && (req.parameters.extTextOut.wOptions & 2)) {
															RECT gdiopaquerect = { 0 };
															gdiopaquerect.top = opaquerect.top;
															gdiopaquerect.left = opaquerect.left;
															gdiopaquerect.bottom = opaquerect.bottom;
															gdiopaquerect.right = opaquerect.right;
															HBRUSH hBrush = CreateSolidBrush(bkColor);
															FillRect(tgtDC, &gdiopaquerect, hBrush);
															DeleteObject(hBrush);
														}
														SetROP2(tgtDC, drawMode.Rop2);
														if (drawMode.bkMode == 1) {
															// 背景透過
															SetBkMode(tgtDC, OPAQUE);
															SetBkColor(tgtDC, bkColor);
															SetTextColor(tgtDC, 0x000000);
															BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcTextInv, 0, 0, SRCAND);
															SetBkMode(tgtDC, OPAQUE);
															SetBkColor(tgtDC, textColor);
															SetTextColor(tgtDC, 0x000000);
															BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcText, 0, 0, SRCINVERT);
														}
														else {
															// 背景不透明
															SetBkColor(tgtDC, textColor);
															SetTextColor(tgtDC, bkColor);
															BitBlt(tgtDC, req.parameters.extTextOut.wDestXOrg, req.parameters.extTextOut.wDestYOrg, sz.cx, sz.cy, hdcText, 0, 0, SRCCOPY);
														}
														if (hRgn) {
															SelectClipRgn(tgtDC, NULL);
															DeleteObject(hRgn);
														}
														if (bmphdc.hdc) {
															// 書き戻し
															npdisp_WriteBitmapToPBITMAP(&dstPBmp, &bmphdc);
														}
													}
													if (bmphdc.hdc) {
														npdisp_FreeBitmap(&bmphdc);
													}
													SelectObject(hdcText, hbmpOld);
													SelectObject(hdcTextInv, hbmpInvOld);
													DeleteObject(hInvBmp);
												}
												DeleteDC(hdcTextInv);
											}
											DeleteObject(hBmp);
										}
										free(pBits);
									}
									DeleteDC(hdcText);
								}
								free(lpbi);
							}

							npdisp.updated = 1;
						}
					}
				}
				//SelectObject(npdispwin.hdc, oldFont);
				free(lpText);
			}
			npdisp_writeMemory32(retValue, req.parameters.extTextOut.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetDIBitsToDevice:
		{
			TRACEOUT(("SetDIBitsToDevice"));
			int dstDevType = 0;
			UINT16 retValue = 0;
			//if (req.parameters.SetDIBitsToDevice.lpDestDevAddr) {
			//	if (npdisp_readMemory(&dstDevType, req.parameters.SetDIBitsToDevice.lpDestDevAddr, 2)) {
			//		if (dstDevType) {
			//			dstDevType = req.parameters.SetDIBitsToDevice.cScans;
			//			npdisp.updated = 1;
			//		}
			//	}
			//}
			npdisp_writeMemory16(retValue, req.parameters.SetDIBitsToDevice.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SaveScreenBitmap:
		{
			TRACEOUT(("SaveScreenBitmap"));
			UINT16 retValue = 0;
			NPDISP_RECT rect = { 0 };
			if (npdisp_readMemory(&rect, req.parameters.SaveScreenBitmap.lpRect, sizeof(rect)) && npdisp.longjmpnum == 0) {
				if (req.parameters.SaveScreenBitmap.wCommand == 0) {
					BitBlt(npdispwin.hdcShadow, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, npdispwin.hdc, rect.left, rect.top, SRCCOPY);
					npdispwin.rectShadow.left = rect.left;
					npdispwin.rectShadow.right = rect.right;
					npdispwin.rectShadow.top = rect.top;
					npdispwin.rectShadow.bottom = rect.bottom;
					retValue = 1;
				}
				else if (req.parameters.SaveScreenBitmap.wCommand == 1) {
					if (npdispwin.rectShadow.left == rect.left && npdispwin.rectShadow.right == rect.right && npdispwin.rectShadow.top == rect.top && npdispwin.rectShadow.bottom == rect.bottom) {
						BitBlt(npdispwin.hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, npdispwin.hdcShadow, rect.left, rect.top, SRCCOPY);
						npdisp.updated = 1;
						retValue = 1;
					}
					else {
						retValue = 0;
					}
				}
				else if (req.parameters.SaveScreenBitmap.wCommand == 2) {
					BitBlt(npdispwin.hdcShadow, 0, 0, npdisp.width, npdisp.height, NULL, 0, 0, BLACKNESS);
					npdispwin.rectShadow.left = 0;
					npdispwin.rectShadow.right = 0;
					npdispwin.rectShadow.top = 0;
					npdispwin.rectShadow.bottom = 0;
					retValue = 1;
				}
			}
			npdisp_writeMemory16(retValue, req.parameters.SaveScreenBitmap.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_SetCursor:
		{
			TRACEOUT(("SetCursor"));
			NPDISP_CURSORSHAPE cursorShape = { 0 };
			if (npdisp_readMemory(&cursorShape, req.parameters.SetCursor.lpCursorShapeAddr, sizeof(cursorShape)) && npdisp.longjmpnum == 0) {
				int cursorDataStride = cursorShape.csWidthBytes;
				int stride = ((cursorShape.csWidth + 7) / 8 + 1) / 2 * 2;
				if (cursorShape.csWidth > 0 && cursorShape.csHeight > 0) {
					void* pBits = (char*)malloc(stride * cursorShape.csHeight);
					if (pBits) {
						int x, y;
						HBITMAP hBmp;
						UINT8* pBits8 = (UINT8*)pBits;
						UINT32 pixelBufAddr = req.parameters.SetCursor.lpCursorShapeAddr + sizeof(cursorShape);
						int minStride = cursorDataStride < stride ? cursorDataStride : stride;
						for (y = 0; y < cursorShape.csHeight; y++) {
							for (x = 0; x < minStride; x++) {
								UINT8 value = npdisp_readMemory8(pixelBufAddr);
								*pBits8 = value;
								pixelBufAddr++;
								pBits8++;
							}
							if (stride > cursorDataStride) {
								pBits8 += stride - cursorDataStride;
							}
							else if (stride < cursorDataStride) {
								pixelBufAddr += cursorDataStride - stride;
							}
						}
						hBmp = CreateBitmap(cursorShape.csWidth, cursorShape.csHeight, 1, 1, pBits);
						if (hBmp) {
							if (npdispwin.hBmpCursorMask) {
								SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursorMask);
								DeleteObject(npdispwin.hBmpCursorMask);
								npdispwin.hBmpCursorMask = NULL;
							}
							npdispwin.hOldBmpCursorMask = (HBITMAP)SelectObject(npdispwin.hdcCursorMask, hBmp);
							npdispwin.hBmpCursorMask = hBmp;
						}
						pBits8 = (UINT8*)pBits; // 戻す
						for (y = 0; y < cursorShape.csHeight; y++) {
							for (x = 0; x < minStride; x++) {
								UINT8 value = npdisp_readMemory8(pixelBufAddr);
								*pBits8 = value;
								pixelBufAddr++;
								pBits8++;
							}
							if (stride > cursorDataStride) {
								pBits8 += stride - cursorDataStride;
							}
							else if (stride < cursorDataStride) {
								pixelBufAddr += cursorDataStride - stride;
							}
						}
						hBmp = CreateBitmap(cursorShape.csWidth, cursorShape.csHeight, 1, 1, pBits);
						if (hBmp) {
							if (npdispwin.hBmpCursor) {
								SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
								DeleteObject(npdispwin.hBmpCursor);
								npdispwin.hBmpCursor = NULL;
							}
							npdispwin.hOldBmpCursor = (HBITMAP)SelectObject(npdispwin.hdcCursor, hBmp);
							npdispwin.hBmpCursor = hBmp;
						}

						npdisp.cursorHotSpotX = cursorShape.csHotX;
						npdisp.cursorHotSpotY = cursorShape.csHotY;
						npdisp.cursorWidth = cursorShape.csWidth;
						npdisp.cursorHeight = cursorShape.csHeight;

						free(pBits);
					}
				}
				else {
					// 破棄
					if (npdispwin.hBmpCursorMask) {
						SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursorMask);
						DeleteObject(npdispwin.hBmpCursorMask);
						npdispwin.hBmpCursorMask = NULL;
					}
					if (npdispwin.hBmpCursor) {
						SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
						DeleteObject(npdispwin.hBmpCursor);
						npdispwin.hBmpCursor = NULL;
					}
				}
			}
			break;
		}
		case NPDISP_FUNCORDER_MoveCursor:
		{
			//TRACEOUT(("MoveCursor"));
			npdisp.cursorX = req.parameters.MoveCursor.wAbsX;
			npdisp.cursorY = req.parameters.MoveCursor.wAbsY;
			npdisp.updated = 1;
			//retCode = NPDISP_RETCODE_FAILED;
			break;
		}
		case NPDISP_FUNCORDER_CheckCursor:
		{
			// 何もすることがない;
			break;
		}
		case NPDISP_FUNCORDER_FastBorder:
		{
			TRACEOUT(("FastBorder"));
			// 何もすることがない;
			//UINT16 retValue = 1;
			//npdisp_writeMemory16(retValue, req.parameters.FastB.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_Output:
		{
			UINT16 retValue = 0xffff;
			npdisp_writeMemory16(retValue, req.parameters.output.lpRetValueAddr);
			break;
		}
		case NPDISP_FUNCORDER_INT2Fh:
		{
			if (req.parameters.INT2Fh.ax == 0x4001) {
				// DOS窓全画面モード設定
				np2wab.relaystateext = 0;
				np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
				npdisp.enabled = 0;
			}
			else if (req.parameters.INT2Fh.ax == 0x4002) {
				// DOS窓全画面モード解除
				npdisp.enabled = 1;
				npdisp.updated = 1;
				np2wab.relaystateext = 3;
				np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			}
			break;
		}
		case NPDISP_FUNCORDER_WEP:
		{
			TRACEOUT(("WEP"));
			// Windows終了
			npdisp.enabled = 0;
			np2wab.relaystateext = 0;
			np2wab_setRelayState(np2wab.relaystateint | np2wab.relaystateext);
			npdisp_releaseScreen();
			break;
		}
		default:
			TRACEOUT(("Function %d", req.funcOrder));
			retCode = NPDISP_RETCODE_FAILED;
			break;
		}
		npdisp_writeReturnCode(&req, npdisp.dataAddr, retCode); // ReturnCode書き込み
	}
	npdisp_cs_execflag = 0;
	npdispcs_leave_criticalsection();
	if (npdisp.longjmpnum) {
		if (last_npdisp_memread_bufsize != npdisp_memread_buf.size() || last_npdisp_memwrite_bufwpos != npdisp_memwrite_bufwpos) {
			CPU_STAT_EXCEPTION_COUNTER_CLEAR(); // 読み書きが進んでいたら例外繰り返しではない
		}
		int longjmpnum = npdisp.longjmpnum;
		npdisp.longjmpnum = 0;
		//PICITEM pi = &pic.pi[1];
		//pi->imr |= PIC_INT6;
		//if (CPU_STAT_PM) {
		//	CPU_EFLAG |= VIF_FLAG;
		//}
		//else {
		//	CPU_EFLAG |= I_FLAG;
		//}
		siglongjmp(exec_1step_jmpbuf, longjmpnum); // 転送
	}
	// 例外発生せずに全部送れたらリセットする
	CPU_REMCLOCK -= 2 * (npdisp_memread_buf.size() + npdisp_memwrite_bufwpos); // メモリアクセスあたり2clock
	npdisp_memwrite_bufwpos = 0;
	npdisp_memread_buf.clear();
	//PICITEM pi = &pic.pi[1];
	//pi->imr &= ~PIC_INT6;
}

 // ---------- IO Ports

static int npdisp_debug_seqCounter = 0;

static char dbgBuf[32] = { 0 };
static int dbgBufIdx = 0;
static void IOOUTCALL npdisp_o7e7(UINT port, REG8 dat)
{
	dbgBuf[dbgBufIdx] = dat;
	dbgBufIdx++;
	if (dbgBufIdx >= sizeof(dbgBuf) - 1) {
		dbgBufIdx = 0;
	}
}

static int lastID = 0;
static void IOOUTCALL npdisp_o7e8(UINT port, REG8 dat)
{
	npdisp.dataAddr = (dat << 24) | (npdisp.dataAddr >> 8);
	if (npdisp_debug_seqCounter >= 4) {
		TRACEOUT(("ADDRESS ERROR! %d %08x %08x", npdisp_debug_seqCounter, CPU_SS, lastID));
	}
	else {
		//TRACEOUT(("ADDRESS %d %08x", npdisp_debug_seqCounter, CPU_SS));
	}
	lastID = CPU_SS;
	npdisp_debug_seqCounter++;
	(void)port;
}

static void IOOUTCALL npdisp_o7e9(UINT port, REG8 dat)
{
	int i;
	//npdispcs_enter_exception_criticalsection();
	//if (npdisp_cs_execflag) {
	//	npdisp_cs_execflag = 0;
	//	npdispcs_leave_criticalsection();
	//}
	//npdispcs_leave_exception_criticalsection();

	if (npdisp.cmdBuf != 0x3132504e || dat != '1') { // 例外復帰の再実行を認める
		npdisp.cmdBuf = (dat << 24) | (npdisp.cmdBuf >> 8);
	}
	else {
		//i = 0;
		TRACEOUTF(("EXCEPTION!!!!!!!!!!!!: %c", (char)dat));
		//npdisp.cmdBuf = (dat << 24) | (npdisp.cmdBuf >> 8);
	}
	if (npdisp_debug_seqCounter != 4) {
		TRACEOUT(("EXECUTE ERROR! %d %08x", npdisp_debug_seqCounter, CPU_SS));
	}
	if (npdisp.cmdBuf == 0x3132504e) {
		//TRACEOUT(("EXECUTE %d %08x", npdisp_debug_seqCounter, CPU_SS));
		npdisp_debug_seqCounter = 0;
		npdisp_exec();
	}

	(void)port;
}

static REG8 IOINPCALL npdisp_i7e8(UINT port)
{
	return(98);
}

static REG8 IOINPCALL npdisp_i7e9(UINT port)
{
	return(21);
}


int npdisp_drawGraphic(void) 
{
	UINT32 updated;
	HDC hdc = np2wabwnd.hDCBuf;

	if (!npdispwin.hdc) return 0;

	np2wab.realWidth = npdisp.width;
	np2wab.realHeight = npdisp.height;

	updated = npdisp.updated;
	npdisp.updated = 0;

	if (!updated) return 0;

	//if (!npdispcs_tryenter_criticalsection()) {
	//	npdisp.updated = 1;
	//	return 0;
	//}
	npdispcs_enter_criticalsection();
	BitBlt(hdc, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, SRCCOPY);
	if (npdispwin.hBmpCursorMask && npdispwin.hBmpCursor) {
		SetTextColor(npdispwin.hdcCursorMask, 0);
		SetBkColor(npdispwin.hdcCursorMask, 0xffffff);
		SetTextColor(npdispwin.hdcCursor, 0);
		SetBkColor(npdispwin.hdcCursor, 0xffffff);
		BitBlt(hdc, npdisp.cursorX - npdisp.cursorHotSpotX, npdisp.cursorY - npdisp.cursorHotSpotY, npdisp.cursorWidth, npdisp.cursorHeight, npdispwin.hdcCursorMask, 0, 0, SRCAND);
		BitBlt(hdc, npdisp.cursorX - npdisp.cursorHotSpotX, npdisp.cursorY - npdisp.cursorHotSpotY, npdisp.cursorWidth, npdisp.cursorHeight, npdispwin.hdcCursor, 0, 0, SRCINVERT);
	}
	else {
		//// Test用
		//BitBlt(hdc, npdisp.cursorX, npdisp.cursorY, 4, 4, NULL, 0, 0, BLACKNESS);
		//BitBlt(hdc, npdisp.cursorX + 1, npdisp.cursorY + 1, 2, 2, NULL, 0, 0, WHITENESS);
	}
	npdispcs_leave_criticalsection();

	return 1;
}

static void npdisp_releaseScreen(void) {
	if (npdispwin.hdc) {
		SelectObject(npdispwin.hdc, npdispwin.hOldPen);
		SelectObject(npdispwin.hdc, npdispwin.hOldBrush);
		for (auto it = npdispwin.pens.begin(); it != npdispwin.pens.end(); ++it) {
			if (it->second.pen) DeleteObject(it->second.pen);
		}
		npdispwin.pens.clear();
		npdispwin.pensIdx = 1;
		for (auto it = npdispwin.brushes.begin(); it != npdispwin.brushes.end(); ++it) {
			if (it->second.brs) DeleteObject(it->second.brs);
		}
		npdispwin.brushes.clear();
		npdispwin.brushesIdx = 1;
		SelectObject(npdispwin.hdc, npdispwin.hOldBmp);
		DeleteObject(npdispwin.hBmp);
		SelectObject(npdispwin.hdcShadow, npdispwin.hOldBmpShadow);
		DeleteObject(npdispwin.hBmpShadow);
		//SelectObject(npdispwin.hdc, npdispwin.hOldPalette);
		//DeleteObject(npdispwin.hPalette);
		DeleteObject(npdispwin.hFont);
		DeleteDC(npdispwin.hdc);
		DeleteDC(npdispwin.hdcShadow);
		npdispwin.hdc = NULL;
		npdispwin.hBmp = NULL;
		npdispwin.hOldBmp = NULL;
		npdispwin.pBits = NULL;
		npdispwin.hdcShadow = NULL;
		npdispwin.hBmpShadow = NULL;
		npdispwin.hOldBmpShadow = NULL;
		npdispwin.pBitsShadow = NULL;

		for (int i = 0; i < NELEMENTS(npdispwin.hdcCache); i++) {
			if (npdispwin.hdcCache[i]) {
				DeleteDC(npdispwin.hdcCache[i]);
				npdispwin.hdcCache[i] = NULL;
			}
		}

		if (npdispwin.hdcCursor) {
			if (npdispwin.hBmpCursor) {
				SelectObject(npdispwin.hdcCursor, npdispwin.hOldBmpCursor);
				DeleteObject(npdispwin.hBmpCursor);
				npdispwin.hBmpCursor = NULL;
			}
			DeleteDC(npdispwin.hdcCursor);
			npdispwin.hdcCursor = NULL;
		}
		if (npdispwin.hdcCursorMask) {
			if (npdispwin.hBmpCursorMask) {
				SelectObject(npdispwin.hdcCursorMask, npdispwin.hOldBmpCursor);
				DeleteObject(npdispwin.hBmpCursorMask);
				npdispwin.hBmpCursorMask = NULL;
			}
			DeleteDC(npdispwin.hdcCursorMask);
			npdispwin.hdcCursorMask = NULL;
		}
		npdisp.cursorHotSpotX = 0;
		npdisp.cursorHotSpotY = 0;
		npdisp.cursorWidth = 0;
		npdisp.cursorHeight = 0;
	}
}
static void npdisp_createScreen(void) {
	const int width = npdisp.width;
	const int height = npdisp.height;
	int i;

	npdisp_releaseScreen();

	HDC hdcScreen = GetDC(NULL);
	npdispwin.hdc = CreateCompatibleDC(hdcScreen);
	npdispwin.hdcShadow = CreateCompatibleDC(hdcScreen);

	//LOGPALETTE* lpPalette;
	int colors = (npdisp.bpp <= 8) ? (1 << npdisp.bpp) : 0;
	//lpPalette = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) + colors * sizeof(PALETTEENTRY));
	//lpPalette->palVersion = 0x0300;
	//lpPalette->palNumEntries = colors;
	//// 求めたカラー値のRGB表現
	//if (colors == 2) {
	//	// 2色パレットセット
	//	for (i = 0; i < NELEMENTS(s_npdisp_rgb2); i++) {
	//		lpPalette->palPalEntry[i].peRed = s_npdisp_rgb2[i & 0xf].r;
	//		lpPalette->palPalEntry[i].peGreen = s_npdisp_rgb2[i & 0xf].g;
	//		lpPalette->palPalEntry[i].peBlue = s_npdisp_rgb2[i & 0xf].b;
	//		lpPalette->palPalEntry[i].peFlags = 0;
	//	}
	//}
	//else if (colors == 16) {
	//	// 16色パレットセット
	//	for (i = 0; i < NELEMENTS(s_npdisp_rgb16); i++) {
	//		lpPalette->palPalEntry[i].peRed = s_npdisp_rgb16[i & 0xf].r;
	//		lpPalette->palPalEntry[i].peGreen = s_npdisp_rgb16[i & 0xf].g;
	//		lpPalette->palPalEntry[i].peBlue = s_npdisp_rgb16[i & 0xf].b;
	//		lpPalette->palPalEntry[i].peFlags = 0;
	//	}
	//}
	//else if (colors == 256) {
	//	// 256色パレットセット
	//	for (i = 0; i < NELEMENTS(s_npdisp_rgb256); i++) {
	//		lpPalette->palPalEntry[i].peRed = s_npdisp_rgb256[i & 0xf].r;
	//		lpPalette->palPalEntry[i].peGreen = s_npdisp_rgb256[i & 0xf].g;
	//		lpPalette->palPalEntry[i].peBlue = s_npdisp_rgb256[i & 0xf].b;
	//		lpPalette->palPalEntry[i].peFlags = 0;
	//	}
	//}
	//npdispwin.hPalette = CreatePalette(lpPalette);
	//free(lpPalette);
	//npdispwin.hOldPalette = SelectPalette(npdispwin.hdc, npdispwin.hPalette, FALSE);
	//RealizePalette(npdispwin.hdc);

	npdispwin.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	npdispwin.bi.bmiHeader.biWidth = width;
	npdispwin.bi.bmiHeader.biHeight = -height; 
	npdispwin.bi.bmiHeader.biPlanes = 1;
	npdispwin.bi.bmiHeader.biBitCount = npdisp.bpp;
	npdispwin.bi.bmiHeader.biCompression = BI_RGB;
	npdispwin.bi.bmiHeader.biSizeImage = 0;
	npdispwin.bi.bmiHeader.biXPelsPerMeter = 0;
	npdispwin.bi.bmiHeader.biYPelsPerMeter = 0;
	npdispwin.bi.bmiHeader.biClrUsed = colors;
	npdispwin.bi.bmiHeader.biClrImportant = colors;

	if (colors == 2) {
		// 2色パレットセット
		for (i = 0; i < NELEMENTS(s_npdisp_rgb2); i++) {
			npdispwin.bi.bmiColors[i].rgbRed = s_npdisp_rgb2[i & 0xf].r;
			npdispwin.bi.bmiColors[i].rgbGreen = s_npdisp_rgb2[i & 0xf].g;
			npdispwin.bi.bmiColors[i].rgbBlue = s_npdisp_rgb2[i & 0xf].b;
			npdispwin.bi.bmiColors[i].rgbReserved = 0;
		}
	}
	else if (colors == 16) {
		// 16色パレットセット
		for (i = 0; i < NELEMENTS(s_npdisp_rgb16); i++) {
			npdispwin.bi.bmiColors[i].rgbRed = s_npdisp_rgb16[i].r;
			npdispwin.bi.bmiColors[i].rgbGreen = s_npdisp_rgb16[i].g;
			npdispwin.bi.bmiColors[i].rgbBlue = s_npdisp_rgb16[i].b;
			npdispwin.bi.bmiColors[i].rgbReserved = 0;
		}
	}
	else if (colors == 256) {
		// 256色パレットセット
		for (i = 0; i < NELEMENTS(s_npdisp_rgb256); i++) {
			npdispwin.bi.bmiColors[i].rgbRed = s_npdisp_rgb256[i].r;
			npdispwin.bi.bmiColors[i].rgbGreen = s_npdisp_rgb256[i].g;
			npdispwin.bi.bmiColors[i].rgbBlue = s_npdisp_rgb256[i].b;
			npdispwin.bi.bmiColors[i].rgbReserved = 0;
		}
	}

	npdispwin.hBmp = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBits, NULL, 0);
	if (!npdispwin.hBmp || !npdispwin.pBits) {
		SelectPalette(npdispwin.hdc, npdispwin.hOldPalette, FALSE);
		DeleteDC(npdispwin.hdc);
		DeleteObject(npdispwin.hPalette);
		npdispwin.hdc = NULL;
		return;
	}
	npdispwin.hBmpShadow = CreateDIBSection(hdcScreen, (BITMAPINFO*)&npdispwin.bi, DIB_RGB_COLORS, &npdispwin.pBitsShadow, NULL, 0);
	if (!npdispwin.hBmpShadow || !npdispwin.pBitsShadow) {
		DeleteObject(npdispwin.hBmp);
		SelectPalette(npdispwin.hdc, npdispwin.hOldPalette, FALSE);
		DeleteDC(npdispwin.hdc);
		DeleteObject(npdispwin.hPalette);
		npdispwin.hBmp = NULL;
		npdispwin.hdc = NULL;
		return;
	}
	ReleaseDC(NULL, hdcScreen); // もういらない

	npdispwin.hOldPen = SelectObject(npdispwin.hdc, GetStockObject(WHITE_PEN));
	npdispwin.hOldBrush = SelectObject(npdispwin.hdc, GetStockObject(BLACK_BRUSH));

	npdispwin.stride = ((width * npdisp.bpp + 31) / 32) * 4;
	memset(npdispwin.pBits, 0x00, npdispwin.stride * height);

	npdispwin.hOldBmp = SelectObject(npdispwin.hdc, npdispwin.hBmp);
	npdispwin.hOldBmpShadow = SelectObject(npdispwin.hdcShadow, npdispwin.hBmpShadow);

	for (int i = 0; i < NELEMENTS(npdispwin.hdcCache); i++) {
		if (!npdispwin.hdcCache[i]) {
			npdispwin.hdcCache[i] = CreateCompatibleDC(NULL);
		}
	}

	npdispwin.rectShadow.left = 0;
	npdispwin.rectShadow.right = 0;
	npdispwin.rectShadow.top = 0;
	npdispwin.rectShadow.bottom = 0;

	npdispwin.hdcCursor = CreateCompatibleDC(NULL);
	npdispwin.hdcCursorMask = CreateCompatibleDC(NULL);

	BitBlt(npdispwin.hdcShadow, 0, 0, npdisp.width, npdisp.height, npdispwin.hdc, 0, 0, BLACKNESS);

	LOGFONT lf = { 0 };
	lf.lfHeight = -8;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	lstrcpy(lf.lfFaceName, _T("MS Gothic"));
	npdispwin.hFont = CreateFontIndirect(&lf);
	SelectObject(npdispwin.hdc, npdispwin.hFont); // DEBUG
}

void npdisp_reset(const NP2CFG* pConfig)
{
	int i;
	npdispcs_initialize();

	NPDISP_RGB3* p256 = s_npdisp_rgb256;
	p256[0].r = 0x00; p256[0].g = 0x00; p256[0].b = 0x00;
	p256[1].r = 0x80; p256[1].g = 0x00; p256[1].b = 0x00;
	p256[2].r = 0x00; p256[2].g = 0x80; p256[2].b = 0x00;
	p256[3].r = 0x80; p256[3].g = 0x80; p256[3].b = 0x00;
	p256[4].r = 0x00; p256[4].g = 0x00; p256[4].b = 0x80;
	p256[5].r = 0x80; p256[5].g = 0x00; p256[5].b = 0x80;
	p256[6].r = 0x00; p256[6].g = 0x80; p256[6].b = 0x80;
	p256[7].r = 0xc0; p256[7].g = 0xc0; p256[7].b = 0xc0;
	p256[8].r = 0xc0; p256[8].g = 0xdc; p256[8].b = 0xc0;
	p256[9].r = 0xa6; p256[9].g = 0xca; p256[9].b = 0xf0;

	p256[255].r = 0xff; p256[255].g = 0xff; p256[255].b = 0xff;
	p256[254].r = 0x00; p256[254].g = 0xff; p256[254].b = 0xff;
	p256[253].r = 0xff; p256[253].g = 0x00; p256[253].b = 0xff;
	p256[252].r = 0x00; p256[252].g = 0x00; p256[252].b = 0xff;
	p256[251].r = 0xff; p256[251].g = 0xff; p256[251].b = 0x00;
	p256[250].r = 0x00; p256[250].g = 0xff; p256[250].b = 0x00;
	p256[249].r = 0xff; p256[249].g = 0x00; p256[249].b = 0x00;
	p256[248].r = 0x80; p256[248].g = 0x80; p256[248].b = 0x80;
	p256[247].r = 0xa0; p256[247].g = 0xa0; p256[247].b = 0xa4;
	p256[246].r = 0xff; p256[246].g = 0xfb; p256[246].b = 0xf0;

	// とりあえずカラーキューブで埋める
	int index = 10;
	for (int r = 0; r < 6; r++) {
		for (int g = 0; g < 6; g++) {
			for (int b = 0; b < 6; b++) {
				p256[index].r = r * 51;
				p256[index].g = g * 51;
				p256[index].b = b * 51;
				index++;
			}
		}
	}

	npdisp_releaseScreen();

	npdisp.enabled = 0;
	npdisp.width = 1024;
	npdisp.height = 720;
	npdisp.bpp = 24;
	npdisp.dpiX = 96;
	npdisp.dpiY = 96;
	npdisp.cursorX = 0;
	npdisp.cursorY = 0;
	npdispwin.pensIdx = 1;
	npdispwin.brushesIdx = 1;

	npdispwin.hdcCursor = NULL;
	npdispwin.hBmpCursor = NULL;
	npdispwin.hOldBmpCursor = NULL;
	npdispwin.hdcCursorMask = NULL;
	npdispwin.hBmpCursorMask = NULL;
	npdispwin.hOldBmpCursorMask = NULL;

	npdisp.cursorHotSpotX = 0;
	npdisp.cursorHotSpotY = 0;
	npdisp.cursorWidth = 0;
	npdisp.cursorHeight = 0;

	npdisp_memwrite_bufwpos = 0;
	npdisp_memread_buf.clear();
}
void npdisp_bind(void)
{
	iocore_attachout(0x07e7, npdisp_o7e7);
	iocore_attachout(0x07e8, npdisp_o7e8);
	iocore_attachout(0x07e9, npdisp_o7e9);
	iocore_attachinp(0x07e8, npdisp_i7e8);
	iocore_attachinp(0x07e9, npdisp_i7e9);
}
void npdisp_unbind(void)
{

}

void npdisp_shutdown()
{
	//npdispcs_enter_exception_criticalsection();
	//if (npdisp_cs_execflag) {
	//	npdisp_cs_execflag = 0;
	//	npdispcs_leave_criticalsection();
	//}
	//npdispcs_leave_exception_criticalsection();
	npdisp_releaseScreen();
	npdispcs_shutdown();
}

#endif