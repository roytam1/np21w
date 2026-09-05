#include	"compiler.h"
#include	"textfile.h"
#include	"dosio.h"
#include	"fdd/sxsi.h"

#ifdef SUPPORT_KAI_IMAGES

#include	"diskimage/cddfile.h"

const OEMCHAR str_track[] = OEMTEXT("TRACK");
const OEMCHAR str_index[] = OEMTEXT("INDEX");

static const OEMCHAR str_file[] = OEMTEXT("FILE");
static const OEMCHAR str_binary[] = OEMTEXT("BINARY");
static const OEMCHAR str_audio[] = OEMTEXT("AUDIO");
static const OEMCHAR str_pregap[] = OEMTEXT("PREGAP");
static const OEMCHAR str_postgap[] = OEMTEXT("POSTGAP");

typedef struct {
	UINT8	adr_ctl;
	UINT8	point;
	UINT16	sector_size;
	UINT16	data_offset;
	UINT8	sector_mode;
	UINT32	index0;
	UINT32	index1;
	UINT32	pregap;
	UINT32	postgap;
	BOOL	has_index0;
	BOOL	has_index1;
} _CUETRK;

/* Parses an MM:SS:FF CUE position into a sector count. */
static BRESULT cue_getpos(const OEMCHAR *str, UINT32 *pos) {
	UINT32	value[3];
	UINT64	sectors;
	UINT	part;
	UINT	digits;
	UINT	i;

	ZeroMemory(value, sizeof(value));
	part = 0;
	digits = 0;
	for (i = 0; str[i] != '\0'; i++) {
		if ((str[i] >= '0') && (str[i] <= '9')) {
			UINT32 digit;

			digit = (UINT32)(str[i] - '0');
			if (value[part] > (0xffffffffUL - digit) / 10) {
				return(FAILURE);
			}
			value[part] = value[part] * 10 + digit;
			digits++;
		}
		else if ((str[i] == ':') && (digits != 0) && (part < 2)) {
			part++;
			digits = 0;
		}
		else {
			return(FAILURE);
		}
	}
	if ((part != 2) || (digits == 0) || (value[1] >= 60) || (value[2] >= 75)) {
		return(FAILURE);
	}
	sectors = ((UINT64)value[0] * 60 + value[1]) * 75 + value[2];
	if (sectors > 0xffffffffUL) {
		return(FAILURE);
	}
	*pos = (UINT32)sectors;
	return(SUCCESS);
}


/* Decodes the CUE track mode and the user-data position within each image sector. */
static BRESULT cue_setmode(_CUETRK *trk, const OEMCHAR *mode) {
	if (!milstr_cmp(mode, str_audio)) {
		trk->adr_ctl = TRACKTYPE_AUDIO;
		trk->sector_size = 2352;
		trk->data_offset = 0;
		trk->sector_mode = CDSECTORMODE_AUDIO;
	}
	else if (!milstr_cmp(mode, OEMTEXT("MODE1/2048"))) {
		trk->adr_ctl = TRACKTYPE_DATA;
		trk->sector_size = 2048;
		trk->data_offset = 0;
		trk->sector_mode = CDSECTORMODE_MODE1;
	}
	else if (!milstr_cmp(mode, OEMTEXT("MODE1/2352"))) {
		trk->adr_ctl = TRACKTYPE_DATA;
		trk->sector_size = 2352;
		trk->data_offset = 16;
		trk->sector_mode = CDSECTORMODE_MODE1;
	}
	else if (!milstr_cmp(mode, OEMTEXT("MODE2/2048"))) {
		trk->adr_ctl = TRACKTYPE_DATA;
		trk->sector_size = 2048;
		trk->data_offset = 0;
		trk->sector_mode = CDSECTORMODE_MODE2;
	}
	else if (!milstr_cmp(mode, OEMTEXT("MODE2/2324"))) {
		trk->adr_ctl = TRACKTYPE_DATA;
		trk->sector_size = 2324;
		trk->data_offset = 0;
		trk->sector_mode = CDSECTORMODE_MODE2;
	}
	else if (!milstr_cmp(mode, OEMTEXT("MODE2/2336"))) {
		trk->adr_ctl = TRACKTYPE_DATA;
		trk->sector_size = 2336;
		trk->data_offset = 8;
		trk->sector_mode = CDSECTORMODE_MODE2;
	}
	else if (!milstr_cmp(mode, OEMTEXT("MODE2/2352"))) {
		trk->adr_ctl = TRACKTYPE_DATA;
		trk->sector_size = 2352;
		trk->data_offset = 24;
		trk->sector_mode = CDSECTORMODE_MODE2;
	}
	else {
		return(FAILURE);
	}
	return(SUCCESS);
}

/* Returns the first file-backed sector position for a CUE track. */
static UINT32 cue_filebegin(const _CUETRK *trk) {
	return(trk->has_index0 ? trk->index0 : trk->index1);
}

/* Builds independent logical-LBA and image-byte layouts for a single-BIN CUE sheet. */
static BRESULT cue_finalize(const OEMCHAR *path, const _CUETRK *cue, UINT trks, _CDTRK *trk, UINT32 *totals) {
	FILEH	fh;
	FILELEN	fsize;
	UINT64	offset;
	UINT64	end_offset;
	UINT64	remain;
	UINT32	logical;
	UINT32	file_begin;
	UINT32	next_file_begin;
	UINT32	file_sectors;
	UINT32	file_pregap;
	UINT32	track_sectors;
	UINT32	visible_pregap;
	UINT	i;

	fh = file_open_rb(path);
	if (fh == FILEH_INVALID) {
		return(FAILURE);
	}
	fsize = file_getsize(fh);
	file_close(fh);
	if (fsize < 0) {
		return(FAILURE);
	}

	logical = 0;
	file_begin = cue_filebegin(&cue[0]);
	offset = (UINT64)file_begin * cue[0].sector_size;
	if (offset > (UINT64)fsize) {
		return(FAILURE);
	}

	for (i = 0; i < trks; i++) {
		if (!cue[i].has_index1 || (cue[i].sector_size == 0)) {
			return(FAILURE);
		}
		file_begin = cue_filebegin(&cue[i]);
		if (cue[i].index1 < file_begin) {
			return(FAILURE);
		}
		file_pregap = cue[i].index1 - file_begin;

		if (i + 1 < trks) {
			next_file_begin = cue_filebegin(&cue[i + 1]);
			if ((next_file_begin <= file_begin) || (next_file_begin < cue[i].index1)) {
				return(FAILURE);
			}
			file_sectors = next_file_begin - file_begin;
			end_offset = offset + (UINT64)file_sectors * cue[i].sector_size;
			if (end_offset > (UINT64)fsize) {
				return(FAILURE);
			}
		}
		else {
			if (offset > (UINT64)fsize) {
				return(FAILURE);
			}
			remain = (UINT64)fsize - offset;
			if ((remain % cue[i].sector_size) != 0) {
				return(FAILURE);
			}
			if ((remain / cue[i].sector_size) > 0xffffffffUL) {
				return(FAILURE);
			}
			file_sectors = (UINT32)(remain / cue[i].sector_size);
			end_offset = (UINT64)fsize;
		}
		if (file_sectors <= file_pregap) {
			return(FAILURE);
		}
		track_sectors = file_sectors - file_pregap;

		trk[i].adr_ctl = cue[i].adr_ctl;
		trk[i].point = cue[i].point;
		trk[i].sector_size = cue[i].sector_size;
		trk[i].data_offset = cue[i].data_offset;
		trk[i].sector_mode = cue[i].sector_mode;
		trk[i].pregap_offset_ex = 0;

		if (cue[i].pregap > (UINT32)(0xffffffffUL - file_pregap)) {
			return(FAILURE);
		}
		if (i == 0) {
			/* Track 1 INDEX 01 defines LBA 0. INDEX 00/PREGAP belongs before LBA 0. */
			visible_pregap = 0;
		}
		else {
			visible_pregap = cue[i].pregap + file_pregap;
		}
		if ((UINT32)(0xffffffffUL - logical) < visible_pregap) {
			return(FAILURE);
		}
		trk[i].pregap_sector = logical;
		trk[i].start_sector = logical + visible_pregap;
		trk[i].pregap_sectors = visible_pregap;
		trk[i].pregap_offset = offset;
		trk[i].start_offset = offset + (UINT64)file_pregap * cue[i].sector_size;
		trk[i].img_pregap_sec = file_begin;
		trk[i].img_start_sec = cue[i].index1;
		trk[i].pos0 = ((i != 0) && cue[i].has_index0) ? (logical + cue[i].pregap) : 0;

		if ((UINT32)(0xffffffffUL - trk[i].start_sector) < (track_sectors - 1)) {
			return(FAILURE);
		}
		trk[i].end_sector = trk[i].start_sector + track_sectors - 1;
		if ((UINT32)(0xffffffffUL - trk[i].end_sector) < cue[i].postgap) {
			return(FAILURE);
		}
		trk[i].end_sector += cue[i].postgap;
		if ((trk[i].end_sector == 0xffffffffUL) || (file_begin > (UINT32)(0xffffffffUL - (file_sectors - 1)))) {
			return(FAILURE);
		}
		trk[i].track_sectors = track_sectors;
		trk[i].end_offset = end_offset;
		trk[i].img_end_sec = file_begin + file_sectors - 1;

		trk[i].pos = trk[i].start_sector;
		trk[i].str_sec = trk[i].pregap_sector;
		trk[i].end_sec = trk[i].end_sector;
		trk[i].sectors = trk[i].end_sec - trk[i].str_sec + 1;

		logical = trk[i].end_sector + 1;
		offset = end_offset;
	}

	*totals = logical;
	return(SUCCESS);
}

/* Loads a CUE sheet whose referenced image data is stored in one BINARY file. */
BRESULT opencue(SXSIDEV sxsi, const OEMCHAR *fname) {
	_CUETRK	cue[99];
	_CDTRK	trk[99];
	OEMCHAR	path[MAX_PATH];
	OEMCHAR	file_path[MAX_PATH];
	OEMCHAR	buf[512];
	OEMCHAR	*argv[8];
	TEXTFILEH	tfh;
	UINT32	value;
	UINT32	totals;
	UINT	tracks;
	SINT	current;
	int	argc;
	BOOL	multiple_files;
	BOOL	parse_error;
	BOOL	use_mapped_reader;
	UINT	i;

	ZeroMemory(cue, sizeof(cue));
	ZeroMemory(trk, sizeof(trk));
	path[0] = '\0';
	tracks = 0;
	current = -1;
	multiple_files = FALSE;
	parse_error = FALSE;
	use_mapped_reader = FALSE;

	tfh = textfile_open(fname, 0x800);
	if (tfh == NULL) {
		return(FAILURE);
	}
	while (textfile_read(tfh, buf, NELEMENTS(buf)) == SUCCESS) {
		argc = milstr_getarg(buf, argv, NELEMENTS(argv));
		if ((argc >= 3) && (!milstr_cmp(argv[0], str_file))) {
			if (milstr_cmp(argv[argc - 1], str_binary)) {
				parse_error = TRUE;
				break;
			}
			file_cpyname(file_path, fname, NELEMENTS(file_path));
			file_cutname(file_path);
			file_catname(file_path, argv[1], NELEMENTS(file_path));
			if (path[0] == '\0') {
				file_cpyname(path, file_path, NELEMENTS(path));
			}
			else if (file_cmpname(path, file_path)) {
				multiple_files = TRUE;
				break;
			}
		}
		else if ((argc >= 3) && (!milstr_cmp(argv[0], str_track))) {
			if ((path[0] == '\0') || (tracks >= NELEMENTS(cue))) {
				parse_error = TRUE;
				break;
			}
			value = (UINT32)milstr_solveINT(argv[1]);
			if ((value == 0) || (value > 99) ||
				((tracks != 0) && (value != (UINT32)cue[tracks - 1].point + 1))) {
				parse_error = TRUE;
				break;
			}
			current = (SINT)tracks++;
			cue[current].point = (UINT8)value;
			if (cue_setmode(&cue[current], argv[2]) != SUCCESS) {
				parse_error = TRUE;
				break;
			}
		}
		else if ((argc >= 2) && (!milstr_cmp(argv[0], str_pregap))) {
			if ((current < 0) || (cue_getpos(argv[1], &value) != SUCCESS)) {
				parse_error = TRUE;
				break;
			}
			cue[current].pregap = value;
		}
		else if ((argc >= 2) && (!milstr_cmp(argv[0], str_postgap))) {
			if ((current < 0) || (cue_getpos(argv[1], &value) != SUCCESS)) {
				parse_error = TRUE;
				break;
			}
			cue[current].postgap = value;
		}
		else if ((argc >= 3) && (!milstr_cmp(argv[0], str_index))) {
			if ((current < 0) || (cue_getpos(argv[2], &value) != SUCCESS)) {
				parse_error = TRUE;
				break;
			}
			if ((UINT8)milstr_solveINT(argv[1]) == 0) {
				cue[current].index0 = value;
				cue[current].has_index0 = TRUE;
			}
			else if ((UINT8)milstr_solveINT(argv[1]) == 1) {
				cue[current].index1 = value;
				cue[current].has_index1 = TRUE;
			}
		}
	}
	textfile_close(tfh);

	if (parse_error || multiple_files || (path[0] == '\0') || (tracks == 0)) {
		return(FAILURE);
	}
	if (cue_finalize(path, cue, tracks, trk, &totals) != SUCCESS) {
		return(FAILURE);
	}

	for (i = 0; i < tracks; i++) {
		if ((cue[i].pregap != 0) || (cue[i].postgap != 0) ||
			((i == 0) && (cue_filebegin(&cue[i]) != cue[i].index1)) ||
			((cue[i].sector_size == 2352) && (cue[i].data_offset != 16)) ||
			((cue[i].sector_size != 2048) && (cue[i].sector_size != 2352))) {
			use_mapped_reader = TRUE;
			break;
		}
	}
	if (use_mapped_reader) {
		sxsi->read = sec_read;
	}
	else {
		set_secread(sxsi, trk, tracks);
	}
	sxsi->totals = totals;
	if (setsxsidev_cue(sxsi, path, trk, tracks) != SUCCESS) {
		sxsi->totals = -1;
		return(FAILURE);
	}
	return(SUCCESS);
}

#endif
