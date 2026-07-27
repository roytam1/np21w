#include "compiler.h"
#include <sys/stat.h>
#include <time.h>
#if defined(WIN32) && defined(OSLANG_UTF8)
#include "codecnv/codecnv.h"
#endif
#include "dosio.h"
#if defined(WIN32)
#include <direct.h>
#include <io.h>
#ifndef FILE_ATTRIBUTE_REPARSE_POINT
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#endif
#else
#include <dirent.h>
#include <unistd.h>
#endif

static	char	curpath[MAX_PATH] = "./";
static	char	*curfilep = curpath + 2;

/* ファイル操作 */
FILEH file_open(const char *path) {

#if defined(WIN32) && defined(OSLANG_UTF8)
	char	sjis[MAX_PATH];
	codecnv_utf8tosjis(sjis, sizeof(sjis), path, (UINT)-1);
	return(fopen(sjis, "rb+"));
#else
	return(fopen(path, "rb+"));
#endif
}

FILEH file_open_rb(const char *path) {

#if defined(WIN32) && defined(OSLANG_UTF8)
	char	sjis[MAX_PATH];
	codecnv_utf8tosjis(sjis, sizeof(sjis), path, (UINT)-1);
	return(fopen(sjis, "rb"));
#else
	return(fopen(path, "rb"));
#endif
}

FILEH file_create(const char *path) {

#if defined(WIN32) && defined(OSLANG_UTF8)
	char	sjis[MAX_PATH];
	codecnv_utf8tosjis(sjis, sizeof(sjis), path, (UINT)-1);
	return(fopen(sjis, "wb+"));
#else
	return(fopen(path, "wb+"));
#endif
}

FILEPOS file_seek(FILEH handle, FILEPOS pointer, int method) {

#if defined(WIN32)
	__int64 pos;
	if (_fseeki64(handle, (__int64)pointer, method) != 0)
		return (FILEPOS)-1;
	pos = _ftelli64(handle);
	if ((pos < 0) || ((__int64)(FILEPOS)pos != pos))
		return (FILEPOS)-1;
	return (FILEPOS)pos;
#else
	off_t pos;
	if ((FILEPOS)(off_t)pointer != pointer)
		return (FILEPOS)-1;
	if (fseeko(handle, (off_t)pointer, method) != 0)
		return (FILEPOS)-1;
	pos = ftello(handle);
	if ((pos == (off_t)-1) || ((off_t)(FILEPOS)pos != pos))
		return (FILEPOS)-1;
	return (FILEPOS)pos;
#endif
}

UINT file_read(FILEH handle, void *data, UINT length) {

	return((UINT)fread(data, 1, length, handle));
}

UINT file_write(FILEH handle, const void *data, UINT length) {

	return((UINT)fwrite(data, 1, length, handle));
}

short file_close(FILEH handle) {

	fclose(handle);
	return(0);
}

FILELEN file_getsize(FILEH handle) {
#if defined(WIN32)
	struct _stat64 sb;
#else
	struct stat sb;
#endif
	FILELEN size;

#if defined(WIN32)
	if (_fstat64(_fileno(handle), &sb) != 0 || sb.st_size < 0)
		return 0;
#else
	if (fstat(fileno(handle), &sb) != 0 || sb.st_size < 0)
		return 0;
#endif
	size = (FILELEN)sb.st_size;
	if ((SINT64)size != (SINT64)sb.st_size)
		return 0;
	return size;
}

short file_sync(FILEH handle) {
	if (fflush(handle) != 0)
		return -1;
#if defined(WIN32)
	return (_commit(_fileno(handle)) == 0) ? 0 : -1;
#else
	return (fsync(fileno(handle)) == 0) ? 0 : -1;
#endif
}

short file_setsize(FILEH handle, FILELEN length) {
	if (length < 0)
		return -1;
	if (fflush(handle) != 0)
		return -1;
#if defined(WIN32)
	return (_chsize_s(_fileno(handle), (__int64)length) == 0) ? 0 : -1;
#else
	{
		off_t size = (off_t)length;
		if ((FILELEN)size != length)
			return -1;
		return (ftruncate(fileno(handle), size) == 0) ? 0 : -1;
	}
#endif
}

short file_attr(const char *path) {

struct stat	sb;
	short	attr;

#if defined(WIN32) && defined(OSLANG_UTF8)
	char	sjis[MAX_PATH];
	codecnv_utf8tosjis(sjis, sizeof(sjis), path, (UINT)-1);
	if (stat(sjis, &sb) == 0)
#else
	if (stat(path, &sb) == 0)
#endif
	{
#if defined(WIN32)
		if (sb.st_mode & _S_IFDIR) {
			attr = FILEATTR_DIRECTORY;
		}
		else {
			attr = 0;
		}
		if (!(sb.st_mode & S_IWRITE)) {
			attr |= FILEATTR_READONLY;
		}
#else
		if (S_ISDIR(sb.st_mode)) {
			return(FILEATTR_DIRECTORY);
		}
		attr = 0;
		if (!(sb.st_mode & S_IWUSR)) {
			attr |= FILEATTR_READONLY;
		}
#endif
		return(attr);
	}
	return(-1);
}

static BRESULT cnv_sttime(time_t *t, DOSDATE *dosdate, DOSTIME *dostime) {

struct tm	*ftime;

	ftime = localtime(t);
	if (ftime == NULL) {
		return(FAILURE);
	}
	if (dosdate) {
		dosdate->year = ftime->tm_year + 1900;
		dosdate->month = ftime->tm_mon + 1;
		dosdate->day = ftime->tm_mday;
	}
	if (dostime) {
		dostime->hour = ftime->tm_hour;
		dostime->minute = ftime->tm_min;
		dostime->second = ftime->tm_sec;
	}
	return(SUCCESS);
}

short file_getdatetime(FILEH handle, DOSDATE *dosdate, DOSTIME *dostime) {

struct stat sb;

	if (fstat(fileno(handle), &sb) == 0) {
		if (cnv_sttime(&sb.st_mtime, dosdate, dostime) == SUCCESS) {
			return(0);
		}
	}
	return(-1);
}

BRESULT file_getshortname(const char *path, char *shortname, UINT cchShortName) {

	(void)path;
	(void)shortname;
	(void)cchShortName;
	return(FAILURE);
}

BOOL file_islink(const char *path) {
#if defined(WIN32)
	DWORD attr;
#if defined(OSLANG_UTF8)
	char sjis[MAX_PATH];
	codecnv_utf8tosjis(sjis, sizeof(sjis), path, (UINT)-1);
	attr = GetFileAttributes(sjis);
#else
	attr = GetFileAttributes(path);
#endif
	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) ? TRUE : FALSE;
#else
	struct stat sb;
	return (lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode)) ? TRUE : FALSE;
#endif
}

short file_delete(const char *path) {

	return(remove(path));
}

short file_dircreate(const char *path) {

#if defined(WIN32)
	return((short)mkdir(path));
#else
	return((short)mkdir(path, 0777));
#endif
}


/* カレントファイル操作 */
void file_setcd(const char *exepath) {

	file_cpyname(curpath, exepath, sizeof(curpath));
	curfilep = file_getname(curpath);
	*curfilep = '\0';
}

char *file_getcd(const char *path) {

	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return(curpath);
}

FILEH file_open_c(const char *path) {

	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return(file_open(curpath));
}

FILEH file_open_rb_c(const char *path) {

	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return(file_open_rb(curpath));
}

FILEH file_create_c(const char *path) {

	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return(file_create(curpath));
}

short file_delete_c(const char *path) {

	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return(file_delete(curpath));
}

short file_attr_c(const char *path) {

	file_cpyname(curfilep, path, NELEMENTS(curpath) - (UINT)(curfilep - curpath));
	return(file_attr(curpath));
}

#if defined(WIN32)
static BRESULT cnvdatetime(FILETIME *file, DOSDATE *dosdate, DOSTIME *dostime) {

	FILETIME	localtime;
	SYSTEMTIME	systime;

	if ((FileTimeToLocalFileTime(file, &localtime) == 0) ||
		(FileTimeToSystemTime(&localtime, &systime) == 0)) {
		return(FAILURE);
	}
	if (dosdate) {
		dosdate->year = (UINT16)systime.wYear;
		dosdate->month = (UINT8)systime.wMonth;
		dosdate->day = (UINT8)systime.wDay;
	}
	if (dostime) {
		dostime->hour = (UINT8)systime.wHour;
		dostime->minute = (UINT8)systime.wMinute;
		dostime->second = (UINT8)systime.wSecond;
	}
	return(SUCCESS);
}

static BRESULT setflist(WIN32_FIND_DATA *w32fd, FLINFO *fli) {

	if ((w32fd->dwFileAttributes & FILEATTR_DIRECTORY) &&
		((!file_cmpname(w32fd->cFileName, ".")) ||
		(!file_cmpname(w32fd->cFileName, "..")))) {
		return(FAILURE);
	}
	memset(fli, 0, sizeof(*fli));
	fli->caps = FLICAPS_SIZE | FLICAPS_ATTR;
	fli->size = w32fd->nFileSizeLow;
	fli->attr = w32fd->dwFileAttributes;
	if (cnvdatetime(&w32fd->ftLastWriteTime, &fli->date, &fli->time)
																== SUCCESS) {
		fli->caps |= FLICAPS_DATE | FLICAPS_TIME;
	}
#if defined(OSLANG_UTF8)
	codecnv_sjistoutf8(fli->path, sizeof(fli->path),
												w32fd->cFileName, (UINT)-1);
#else
	file_cpyname(fli->path, w32fd->cFileName, sizeof(fli->path));
#endif
	return(SUCCESS);
}

FLISTH file_list1st(const char *dir, FLINFO *fli) {

	char			path[MAX_PATH];
	HANDLE			hdl;
	WIN32_FIND_DATA	w32fd;

	file_cpyname(path, dir, sizeof(path));
	file_setseparator(path, sizeof(path));
	file_catname(path, "*.*", sizeof(path));
	hdl = FindFirstFile(path, &w32fd);
	if (hdl != INVALID_HANDLE_VALUE) {
		do {
			if (setflist(&w32fd, fli) == SUCCESS) {
				return(hdl);
			}
		} while(FindNextFile(hdl, &w32fd));
		FindClose(hdl);
	}
	return(FLISTH_INVALID);
}

BRESULT file_listnext(FLISTH hdl, FLINFO *fli) {

	WIN32_FIND_DATA	w32fd;

	while(FindNextFile(hdl, &w32fd)) {
		if (setflist(&w32fd, fli) == SUCCESS) {
			return(SUCCESS);
		}
	}
	return(FAILURE);
}

void file_listclose(FLISTH hdl) {

	FindClose(hdl);
}
#else
FLISTH file_list1st(const char *dir, FLINFO *fli) {

	DIR		*ret;

	ret = opendir(dir);
	if (ret == NULL) {
		goto ff1_err;
	}
	if (file_listnext((FLISTH)ret, fli) == SUCCESS) {
		return((FLISTH)ret);
	}
	closedir(ret);

ff1_err:
	return(FLISTH_INVALID);
}

BRESULT file_listnext(FLISTH hdl, FLINFO *fli) {

struct dirent	*de;
struct stat		sb;

	de = readdir((DIR *)hdl);
	if (de == NULL) {
		return(FAILURE);
	}
	if (fli) {
		memset(fli, 0, sizeof(*fli));
		fli->caps = FLICAPS_ATTR;
		fli->attr = (de->d_type & DT_DIR) ? FILEATTR_DIRECTORY : 0;

		if (stat(de->d_name, &sb) == 0) {
			fli->caps |= FLICAPS_SIZE;
			fli->size = (UINT)sb.st_size;
			if (!(sb.st_mode & S_IWUSR)) {
				fli->attr |= FILEATTR_READONLY;
			}
			if (cnv_sttime(&sb.st_mtime, &fli->date, &fli->time) == SUCCESS) {
				fli->caps |= FLICAPS_DATE | FLICAPS_TIME;
			}
		}
		milstr_ncpy(fli->path, de->d_name, sizeof(fli->path));
	}
	return(SUCCESS);
}

void file_listclose(FLISTH hdl) {

	closedir((DIR *)hdl);
}
#endif

void file_catname(char *path, const char *name, int maxlen) {

	int		csize;

	while(maxlen > 0) {
		if (*path == '\0') {
			break;
		}
		path++;
		maxlen--;
	}
	file_cpyname(path, name, maxlen);
	while((csize = milstr_charsize(path)) != 0) {
		if ((csize == 1) && (*path == '\\')) {
			*path = '/';
		}
		path += csize;
	}
}

char *file_getname(const char *path) {

const char	*ret;
	int		csize;

	ret = path;
	while((csize = milstr_charsize(path)) != 0) {
		if ((csize == 1) && (*path == '/')) {
			ret = path + 1;
		}
		path += csize;
	}
	return((char *)ret);
}

void file_cutname(char *path) {

	char	*p;

	p = file_getname(path);
	*p = '\0';
}

char *file_getext(const char *path) {

const char	*p;
const char	*q;

	p = file_getname(path);
	q = NULL;
	while(*p != '\0') {
		if (*p == '.') {
			q = p + 1;
		}
		p++;
	}
	if (q == NULL) {
		q = p;
	}
	return((char *)q);
}

void file_cutext(char *path) {

	char	*p;
	char	*q;

	p = file_getname(path);
	q = NULL;
	while(*p != '\0') {
		if (*p == '.') {
			q = p;
		}
		p++;
	}
	if (q != NULL) {
		*q = '\0';
	}
}

void file_cutseparator(char *path) {

	int		pos;

	pos = (int)strlen(path) - 1;
	if ((pos > 0) &&							// 2文字以上でー
		(path[pos] == '/') &&					// ケツが \ でー
		((pos != 1) || (path[0] != '.'))) {		// './' ではなかったら
		path[pos] = '\0';
	}
}

void file_setseparator(char *path, int maxlen) {

	int		pos;

	pos = (int)strlen(path);
	if ((pos) && (path[pos-1] != '/') && ((pos + 2) < maxlen)) {
		path[pos++] = '/';
		path[pos] = '\0';
	}
}

