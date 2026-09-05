#include	"compiler.h"
#include	"dosio.h"
#include	"textfile.h"
#include	"cpucore.h"
#include	"fdd/sxsi.h"
#include	"cddfile.h"

#ifdef SUPPORT_KAI_IMAGES

#include	"diskimage/img_strres.h"
#include	"diskimage/win9x/img_dosio.h"
#include	"diskimage/cd/cdd_iso.h"

//	ISO9660のボリューム記述子によるチェックを有効にする場合はコメントを外す
//	※有効にした場合、CD-ROM以外がマウントできなくなる
//#define	CHECK_ISO9660

#ifdef	CHECK_ISO9660
static const UINT8 cd001[7] = {0x01,'C','D','0','0','1',0x01};
#endif

#define CD_EDC_POLYNOMIAL	0xD8018001 // Reverse 0x8001801B

UINT32 crcTable[256];
static UINT8 ecc_f_lut[256];
static UINT8 ecc_b_lut[256];

void makeCRCTable( void)
{
	UINT32 i, j;
	for (i = 0; i < 256; i++) {
		UINT32 crc = i;
		UINT32 ecc = i << 1;
		for (j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ ((crc & 0x1) ? CD_EDC_POLYNOMIAL : 0);
		}
		crcTable[i] = crc;
		if (ecc & 0x100) {
			ecc ^= 0x11d;
		}
		ecc_f_lut[i] = (UINT8)ecc;
		ecc_b_lut[i ^ ecc] = (UINT8)i;
	}
}

//	追加(kaiA)
BOOL isCDImage(const OEMCHAR *fname) {

const OEMCHAR	*ext;

	ext = file_getext(fname);
	if ((!file_cmpname(ext, str_cue)) ||
		(!file_cmpname(ext, str_ccd)) ||
		(!file_cmpname(ext, str_cdm)) ||
		(!file_cmpname(ext, str_mds)) ||
		(!file_cmpname(ext, str_nrg)) ||
		(!file_cmpname(ext, str_iso))) {
		return TRUE;
	}
	return FALSE;
}
//

long issec2048(FILEH fh) {

#ifdef	CHECK_ISO9660
	FILEPOS	fpos;
	UINT8	buf[2048];
	UINT	secsize;
#endif
	FILELEN	fsize;

#ifdef	CHECK_ISO9660
	fpos = 16 * 2048;
	if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
		goto sec2048_err;
	}
	if (file_read(fh, buf, sizeof(buf)) != sizeof(buf)) {
		goto sec2048_err;
	}
	if (memcmp(buf, cd001, 7) != 0) {
		goto sec2048_err;
	}
	secsize = LOADINTELWORD(buf + 128);
	if (secsize != 2048) {
		goto sec2048_err;
	}
#endif
	fsize = file_getsize(fh);
	if ((fsize % 2048) != 0) {
		goto sec2048_err;
	}
	return((long)(fsize / 2048));

sec2048_err:
	return(-1);
}

long issec2352(FILEH fh) {

#ifdef	CHECK_ISO9660
	FILEPOS	fpos;
	UINT8	buf[2048];
	UINT	secsize;
#endif
	FILELEN	fsize;

#ifdef	CHECK_ISO9660
	fpos = (16 * 2352) + 16;
	if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
		goto sec2352_err;
	}
	if (file_read(fh, buf, sizeof(buf)) != sizeof(buf)) {
		goto sec2352_err;
	}
	if (memcmp(buf, cd001, 7) != 0) {
		goto sec2352_err;
	}
	secsize = LOADINTELWORD(buf + 128);
	if (secsize != 2048) {
		goto sec2352_err;
	}
#endif
	fsize = file_getsize(fh);
	if ((fsize % 2352) != 0) {
		goto sec2352_err;
	}
	return((long)(fsize / 2352));

sec2352_err:
	return(-1);
}

long issec2448(FILEH fh) {

#ifdef	CHECK_ISO9660
	FILEPOS	fpos;
	UINT8	buf[2048];
	UINT	secsize;
#endif
	FILELEN	fsize;

#ifdef	CHECK_ISO9660
	fpos = (16 * 2448) + 16;
	if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
		goto sec2448_err;
	}
	if (file_read(fh, buf, sizeof(buf)) != sizeof(buf)) {
		goto sec2448_err;
	}
	if (memcmp(buf, cd001, 7) != 0) {
		goto sec2448_err;
	}
	secsize = LOADINTELWORD(buf + 128);
	if (secsize != 2048) {
		goto sec2448_err;
	}
#endif
	fsize = file_getsize(fh);
	if ((fsize % 2448) != 0) {
		goto sec2448_err;
	}
	return((long)(fsize / 2448));

sec2448_err:
	return(-1);
}

long issec(FILEH fh, _CDTRK *trk, UINT trks) {

#ifdef	CHECK_ISO9660
	FILEPOS	fpos;
	UINT8	buf[2048];
	UINT	secsize;
#endif
	UINT	i;
	FILELEN	fsize;
	long	total;

	total = 0;

#ifdef	CHECK_ISO9660
	fpos = 16 * trk[0].sector_size;
	if (trk[0].sector_size != 2048) {
		fpos += 16;
	}
	if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
		goto sec_err;
	}
	if (file_read(fh, buf, sizeof(buf)) != sizeof(buf)) {
		goto sec_err;
	}
	if (memcmp(buf, cd001, 7) != 0) {
		goto sec_err;
	}
	secsize = LOADINTELWORD(buf + 128);
	if (secsize != 2048) {
		goto sec_err;
	}
#endif

	if (trks == 1) {
		trk[0].sector_size = 2048;
		trk[0].str_sec = 0;
		total = issec2048(fh);
		if (total < 0) {
			trk[0].sector_size = 2352;
			total = issec2352(fh);
		}
		if (total < 0) {
			trk[0].sector_size = 2448;
			total = issec2448(fh);
		}
		if (total < 0) {
			return(-1);
		}
		else {
			trk[0].end_sec = total - 1;
			trk[0].sectors = total;
			return(total);
		}
	}

	fsize = file_getsize(fh);
	if (trk[0].pos0 == 0) {
		trk[0].str_sec = trk[0].pos;
	}
	else {
		trk[0].str_sec = trk[0].pos0;
	}
	for (i = 1; i < trks; i++) {
		if (trk[i].pos0 == 0) {
			trk[i].str_sec = trk[i].pos;
		}
		else {
			trk[i].str_sec = trk[i].pos0;
		}
		trk[i-1].end_sec = trk[i].str_sec - 1;
		trk[i-1].sectors = trk[i-1].end_sec - trk[i-1].str_sec + 1;
		total += trk[i-1].sectors;
		fsize -= trk[i-1].sectors * trk[i-1].sector_size;
	}
	if (fsize % trk[trks-1].sector_size != 0) {
		return(-1);
	}
	if (trk[trks-1].pos0 == 0) {
		trk[trks-1].str_sec = trk[trks-1].pos;
	}
	else {
		trk[trks-1].str_sec = trk[trks-1].pos0;
	}
	trk[trks-1].end_sec = (UINT32)(trk[trks-1].str_sec + (fsize / trk[trks-1].sector_size));
	trk[trks-1].sectors = trk[trks-1].end_sec - trk[trks-1].str_sec + 1;
	total += trk[trks-1].sectors;

	return(total);

#ifdef	CHECK_ISO9660
sec_err:
	return(-1);
#endif
}

//	※CDTRK構造体内の
//		UINT32	str_sec;
//		UINT32	end_sec;
//		UINT32	sectors;
//		等のメンバの設定
long set_trkinfo(FILEH fh, _CDTRK *trk, UINT trks, FILELEN imagesize) {

	UINT	i;
	FILELEN	fsize;
	FILELEN	real_sectors;
	long	total;

	if (trks == 1) {
		trk[0].sector_size = 2048;
		trk[0].str_sec = 0;
		total = issec2048(fh);
		if (total < 0) {
			trk[0].sector_size = 2352;
			total = issec2352(fh);
		}
		if (total < 0) {
			trk[0].sector_size = 2448;
			total = issec2448(fh);
		}
		if (total < 0) {
			return(-1);
		}
		else {
			trk[0].end_sec = total - 1;
			trk[0].sectors = total;
			return(total);
		}
	}

	if (imagesize == 0) {
		fsize = file_getsize(fh);
	}
	else {
		fsize = imagesize;
	}

	trk[0].str_sec = (trk[0].pos0 == 0) ? trk[0].pos : trk[0].pos0;
	for (i = 1; i < trks; i++) {
		trk[i].str_sec = (trk[i].pos0 == 0) ? trk[i].pos : trk[i].pos0;

		if (trk[i].str_sec <= trk[i-1].str_sec) {
			return(-1);
		}
		trk[i-1].end_sec = trk[i].str_sec - 1;
		trk[i-1].sectors = trk[i-1].end_sec - trk[i-1].str_sec + 1;

		real_sectors = trk[i-1].sectors;
		if (fsize < real_sectors * trk[i-1].sector_size) {
			return(-1);
		}
		fsize -= real_sectors * trk[i-1].sector_size;
	}

	if (fsize % trk[trks-1].sector_size != 0) {
		return(-1);
	}
	trk[trks-1].str_sec = (trk[trks-1].pos0 == 0) ? trk[trks-1].pos : trk[trks-1].pos0;
	real_sectors = fsize / trk[trks-1].sector_size;
	if (real_sectors == 0) {
		return(-1);
	}
	trk[trks-1].end_sec = (UINT32)(trk[trks-1].str_sec + real_sectors - 1);
	trk[trks-1].sectors = trk[trks-1].end_sec - trk[trks-1].str_sec + 1;

	return((long)(trk[trks-1].end_sec + 1));
}



//	----
//	イメージファイル内全トラックセクタ長2048byte用
REG8 sec2048_read(SXSIDEV sxsi, FILEPOS pos, UINT8 *buf, UINT size) {

	CDINFO	cdinfo;
	FILEH	fh;
	UINT	rsize;

	if (sxsi_prepare(sxsi) != SUCCESS) {
		return(0x60);
	}
	if ((pos < 0) || (pos >= sxsi->totals)) {
		return(0x40);
	}

	cdinfo = (CDINFO)sxsi->hdl;
	fh = cdinfo->fh;

	pos = (FILEPOS)(pos * 2048 + cdinfo->trk[0].start_offset);
	if (file_seek(fh, pos, FSEEK_SET) != pos) {
		return(0xd0);
	}

	while(size) {
		rsize = min(size, 2048);
		CPU_REMCLOCK -= rsize;
		if (file_read(fh, buf, rsize) != rsize) {
			return(0xd0);
		}
		buf += rsize;
		size -= rsize;
	}
	return(0x00);
}


//	イメージファイル内全トラックセクタ長2352byte用
REG8 sec2352_read(SXSIDEV sxsi, FILEPOS pos, UINT8 *buf, UINT size) {

	CDINFO	cdinfo;
	FILEH	fh;
	FILEPOS	fpos;
	UINT	rsize;

	if (sxsi_prepare(sxsi) != SUCCESS) {
		return(0x60);
	}
	if ((pos < 0) || (pos >= sxsi->totals)) {
		return(0x40);
	}

	cdinfo = (CDINFO)sxsi->hdl;
	fh = cdinfo->fh;

	while(size) {
		fpos = (FILEPOS)((pos * 2352) + 16 + cdinfo->trk[0].start_offset);
		if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
			return(0xd0);
		}
		rsize = min(size, 2048);
		CPU_REMCLOCK -= rsize;
		if (file_read(fh, buf, rsize) != rsize) {
			return(0xd0);
		}
		buf += rsize;
		size -= rsize;
		pos++;
	}
	return(0x00);
}

UINT32 calcCRC(UINT8 *buf, int len)
{
	int i;
	UINT32 crc = 0x00000000;
	for (i = 0; i < len; i++) {
		crc = (crc >> 8) ^ crcTable[(crc ^ buf[i]) & 0xff];
	}
	return(crc);
}

static void cdd_storeedc(UINT8 *dst, UINT32 edc) {
	dst[0] = (UINT8)(edc >> 0);
	dst[1] = (UINT8)(edc >> 8);
	dst[2] = (UINT8)(edc >> 16);
	dst[3] = (UINT8)(edc >> 24);
}

static void cdd_ecc_compute(const UINT8 *src, UINT major_count, UINT minor_count, UINT major_mult, UINT minor_inc, UINT8 *dst) {
	UINT size;
	UINT major;

	size = major_count * minor_count;
	for (major = 0; major < major_count; major++) {
		UINT index;
		UINT minor;
		UINT8 ecc_a;
		UINT8 ecc_b;

		index = (major >> 1) * major_mult + (major & 1);
		ecc_a = 0;
		ecc_b = 0;
		for (minor = 0; minor < minor_count; minor++) {
			UINT8 temp = src[index];
			index += minor_inc;
			if (index >= size) {
				index -= size;
			}
			ecc_a ^= temp;
			ecc_b ^= temp;
			ecc_a = ecc_f_lut[ecc_a];
		}
		ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
		dst[major] = ecc_a;
		dst[major + major_count] = ecc_a ^ ecc_b;
	}
}

static void cdd_ecc_generate(UINT8 *sector, BOOL zero_address) {
	UINT8 address[4];

	if (zero_address) {
		CopyMemory(address, sector + 12, sizeof(address));
		ZeroMemory(sector + 12, sizeof(address));
	}
	cdd_ecc_compute(sector + 0x0c, 86, 24, 2, 86, sector + 0x81c);
	cdd_ecc_compute(sector + 0x0c, 52, 43, 86, 88, sector + 0x8c8);
	if (zero_address) {
		CopyMemory(sector + 12, address, sizeof(address));
	}
}

static UINT8 cdd_tobcd(UINT value) {
	return((UINT8)(((value / 10) << 4) | (value % 10)));
}

static void cdd_makeheader(UINT8 *sector, FILEPOS lba, UINT8 mode) {
	UINT32 absolute;
	UINT32 minute;
	UINT32 second;
	UINT32 frame;

	ZeroMemory(sector, 2352);
	sector[0] = 0x00;
	memset(sector + 1, 0xff, 10);
	sector[11] = 0x00;
	absolute = (UINT32)lba + 150;
	minute = absolute / (60 * 75);
	second = (absolute / 75) % 60;
	frame = absolute % 75;
	sector[12] = cdd_tobcd(minute);
	sector[13] = cdd_tobcd(second);
	sector[14] = cdd_tobcd(frame);
	sector[15] = mode;
}

BRESULT cddfile_makerawsector(FILEPOS lba, UINT8 sector_mode, UINT16 source_sector_size, UINT8 *sector) {
	UINT32 edc;

	if ((lba < 0) || (sector == NULL)) {
		return(FAILURE);
	}
	if (sector_mode == CDSECTORMODE_AUDIO) {
		ZeroMemory(sector, 2352);
		return(SUCCESS);
	}
	if (sector_mode == CDSECTORMODE_MODE1) {
		cdd_makeheader(sector, lba, 1);
		edc = calcCRC(sector, 0x810);
		cdd_storeedc(sector + 0x810, edc);
		ZeroMemory(sector + 0x814, 8);
		cdd_ecc_generate(sector, FALSE);
		return(SUCCESS);
	}
	if (sector_mode == CDSECTORMODE_MODE2) {
		cdd_makeheader(sector, lba, 2);
		if (source_sector_size == 2048) {
			sector[18] = 0x08;
			sector[22] = 0x08;
			edc = calcCRC(sector + 0x10, 0x808);
			cdd_storeedc(sector + 0x818, edc);
			cdd_ecc_generate(sector, TRUE);
		}
		else if (source_sector_size == 2324) {
			sector[18] = 0x28;
			sector[22] = 0x28;
			edc = calcCRC(sector + 0x10, 0x91c);
			cdd_storeedc(sector + 0x92c, edc);
		}
		return(SUCCESS);
	}
	return(FAILURE);
}

static BOOL cddfile_check_mode1_edc(const UINT8 *sector) {
	return(calcCRC((UINT8 *)sector, 0x810) == LOADINTELDWORD(sector + 0x810));
}

//	イメージファイル内全トラックセクタ長2352byte用(ECCチェック有効)
REG8 sec2352_read_with_ecc(SXSIDEV sxsi, FILEPOS pos, UINT8 *buf, UINT size) {
	
	CDINFO	cdinfo;
	FILEH	fh;
	FILEPOS	fpos;
	UINT	rsize;
	UINT8	bufdata[2352];

	if (sxsi_prepare(sxsi) != SUCCESS) {
		return(0x60);
	}
	if ((pos < 0) || (pos >= sxsi->totals)) {
		return(0x40);
	}

	cdinfo = (CDINFO)sxsi->hdl;
	fh = cdinfo->fh;

	while(size) {
		fpos = (FILEPOS)((pos * 2352) + cdinfo->trk[0].start_offset);
		if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
			return(0xd0);
		}
		rsize = 2352;
		CPU_REMCLOCK -= rsize;
		if (file_read(fh, bufdata, rsize) != rsize) {
			return(0xd0);
		}
		memcpy(buf, bufdata + 16, min(size, 2048));
		if (!cddfile_check_mode1_edc(bufdata)) {
			sxsi->cdflag_ecc = (sxsi->cdflag_ecc & ~CD_ECC_BITMASK) | CD_ECC_ERROR;
		}

		rsize = min(size, 2048);
		buf += rsize;
		size -= rsize;
		pos++;
	}
	return(0x00);
}


//	イメージファイル内全トラックセクタ長2448(2352+96)用
REG8 sec2448_read(SXSIDEV sxsi, FILEPOS pos, UINT8 *buf, UINT size) {

	CDINFO	cdinfo;
	FILEH	fh;
	FILEPOS	fpos;
	UINT	rsize;

	if (sxsi_prepare(sxsi) != SUCCESS) {
		return(0x60);
	}
	if ((pos < 0) || (pos >= sxsi->totals)) {
		return(0x40);
	}

	cdinfo = (CDINFO)sxsi->hdl;
	fh = cdinfo->fh;
	while(size) {
		fpos = (FILEPOS)((pos * 2448) + 16 + cdinfo->trk[0].start_offset);
		if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
			return(0xd0);
		}
		rsize = min(size, 2048);
		CPU_REMCLOCK -= rsize;
		if (file_read(fh, buf, rsize) != rsize) {
			return(0xd0);
		}
		buf += rsize;
		size -= rsize;
		pos++;
	}
	return(0x00);
}


//	イメージファイル内セクタ長混在用
//		非RAW(2048byte)＋Audio(2352byte)等
BRESULT cddfile_mapsector(SXSIDEV sxsi, FILEPOS pos, FILEPOS *fpos, UINT16 *sector_size, UINT16 *data_offset, UINT8 *sector_mode, UINT8 *adr_ctl, BOOL *synthetic) {
	CDINFO	cdinfo;
	UINT	i;
	UINT32	secs;
	UINT32	file_pregap;
	UINT32	file_start_lba;
	UINT32	file_sectors;
	UINT64	file_bytes;
	FILEPOS	base;

	if ((sxsi == NULL) || (sxsi->hdl == (INTPTR)NULL) || (pos < 0) || (pos >= sxsi->totals)) {
		return(FAILURE);
	}
	cdinfo = (CDINFO)sxsi->hdl;
	if (cdinfo->trks == 0) {
		return(FAILURE);
	}

	if (cdinfo->layout == CDINFO_LAYOUT_CUE) {
		for (i = 0; i < cdinfo->trks; i++) {
			if ((cdinfo->trk[i].str_sec <= (UINT32)pos) && ((UINT32)pos <= cdinfo->trk[i].end_sec)) {
				if ((cdinfo->trk[i].sector_size == 0) || (cdinfo->trk[i].img_start_sec < cdinfo->trk[i].img_pregap_sec) ||
					(cdinfo->trk[i].end_offset < cdinfo->trk[i].pregap_offset)) {
					return(FAILURE);
				}
				file_pregap = cdinfo->trk[i].img_start_sec - cdinfo->trk[i].img_pregap_sec;
				if (i == 0) {
					/* Track 1 INDEX 00 is before LBA 0 and is not addressable through normal LBA reads. */
					file_start_lba = cdinfo->trk[i].start_sector;
					base = (FILEPOS)cdinfo->trk[i].start_offset;
					file_bytes = cdinfo->trk[i].end_offset - cdinfo->trk[i].start_offset;
				}
				else {
					if (cdinfo->trk[i].start_sector < file_pregap) {
						return(FAILURE);
					}
					file_start_lba = cdinfo->trk[i].start_sector - file_pregap;
					base = (FILEPOS)cdinfo->trk[i].pregap_offset;
					file_bytes = cdinfo->trk[i].end_offset - cdinfo->trk[i].pregap_offset;
				}
				if ((file_bytes % cdinfo->trk[i].sector_size) != 0) {
					return(FAILURE);
				}
				file_sectors = (UINT32)(file_bytes / cdinfo->trk[i].sector_size);
				if (((UINT32)pos < file_start_lba) || ((UINT32)pos - file_start_lba >= file_sectors)) {
					*synthetic = TRUE;
					*fpos = 0;
				}
				else {
					*synthetic = FALSE;
					*fpos = (FILEPOS)((UINT64)base +
						(UINT64)((UINT32)pos - file_start_lba) * cdinfo->trk[i].sector_size);
				}
				*sector_size = cdinfo->trk[i].sector_size;
				*data_offset = cdinfo->trk[i].data_offset;
				*sector_mode = cdinfo->trk[i].sector_mode;
				*adr_ctl = cdinfo->trk[i].adr_ctl;
				return(SUCCESS);
			}
		}
		return(FAILURE);
	}

	base = 0;
	secs = 0;
	for (i = 0; i < cdinfo->trks; i++) {
		if ((cdinfo->trk[i].str_sec <= (UINT32)pos) && ((UINT32)pos <= cdinfo->trk[i].end_sec)) {
			base += (pos - secs) * cdinfo->trk[i].sector_size;
			base += (FILEPOS)cdinfo->trk[0].start_offset;
			*fpos = base;
			*sector_size = cdinfo->trk[i].sector_size;
			*data_offset = (cdinfo->trk[i].sector_size == 2048) ? 0 : 16;
			*sector_mode = cdinfo->trk[i].sector_mode;
			*adr_ctl = cdinfo->trk[i].adr_ctl;
			*synthetic = FALSE;
			return(SUCCESS);
		}
		base += (FILEPOS)cdinfo->trk[i].sectors * cdinfo->trk[i].sector_size;
		secs += cdinfo->trk[i].sectors;
	}
	return(FAILURE);
}

//	----
//	Reads 2048-byte user data using the logical-to-image sector map.
REG8 sec_read(SXSIDEV sxsi, FILEPOS pos, UINT8 *buf, UINT size) {
	CDINFO	cdinfo;
	FILEH	fh;
	FILEPOS	fpos;
	UINT16	sector_size;
	UINT16	data_offset;
	UINT8	sector_mode;
	UINT8	adr_ctl;
	BOOL	synthetic;
	UINT	rsize;

	if (sxsi_prepare(sxsi) != SUCCESS) {
		return(0x60);
	}
	if ((pos < 0) || (pos >= sxsi->totals)) {
		return(0x40);
	}

	cdinfo = (CDINFO)sxsi->hdl;
	fh = cdinfo->fh;
	while (size) {
		if (cddfile_mapsector(sxsi, pos, &fpos, &sector_size, &data_offset, &sector_mode, &adr_ctl, &synthetic) != SUCCESS) {
			return(0xd0);
		}
		rsize = min(size, 2048);
		CPU_REMCLOCK -= rsize;
		if (synthetic) {
			memset(buf, 0, rsize);
		}
		else {
			if ((sector_mode == CDSECTORMODE_MODE1) && (sector_size == 2352) && (data_offset == 16)) {
				UINT8 rawdata[2352];

				if ((file_seek(fh, fpos, FSEEK_SET) != fpos) || (file_read(fh, rawdata, sizeof(rawdata)) != sizeof(rawdata))) {
					return(0xd0);
				}
				memcpy(buf, rawdata + 16, rsize);
				if (!cddfile_check_mode1_edc(rawdata)) {
					sxsi->cdflag_ecc = (sxsi->cdflag_ecc & ~CD_ECC_BITMASK) | CD_ECC_ERROR;
				}
			}
			else {
				if ((sector_mode == CDSECTORMODE_MODE2) && ((sector_size == 2352) || (sector_size == 2336))) {
					UINT8 subhead[8];
					FILEPOS subpos;

					subpos = fpos + ((sector_size == 2352) ? 16 : 0);
					if ((file_seek(fh, subpos, FSEEK_SET) != subpos) || (file_read(fh, subhead, sizeof(subhead)) != sizeof(subhead))) {
						return(0xd0);
					}
					data_offset = (!memcmp(subhead, subhead + 4, 4)) ? ((sector_size == 2352) ? 24 : 8) : ((sector_size == 2352) ? 16 : 0);
				}
				fpos += data_offset;
				if (file_seek(fh, fpos, FSEEK_SET) != fpos) {
					return(0xd0);
				}
				if (file_read(fh, buf, rsize) != rsize) {
					return(0xd0);
				}
			}
		}
		buf += rsize;
		size -= rsize;
		pos++;
	}
	return(0x00);
}

//	----
BRESULT cd_reopen(SXSIDEV sxsi) {

	CDINFO	cdinfo;
	FILEH	fh;

	cdinfo = (CDINFO)sxsi->hdl;
	fh = file_open_rb(cdinfo->path);
	if (fh != FILEH_INVALID) {
		cdinfo->fh = fh;
		return(SUCCESS);
	}
	else {
		return(FAILURE);
	}
}

void cd_close(SXSIDEV sxsi) {

	CDINFO	cdinfo;

	cdinfo = (CDINFO)sxsi->hdl;
	file_close(cdinfo->fh);
}

void cd_destroy(SXSIDEV sxsi) {

	if(sxsi->hdl){
		_MFREE((CDINFO)sxsi->hdl);
		sxsi->hdl = (INTPTR)NULL;
	}
}
//	----

void set_secread(SXSIDEV sxsi, const _CDTRK *trk, UINT trks) {

	UINT		i;
	UINT16		secsize;

	secsize = trk[0].sector_size;
	for (i = 1; i < trks; i++) {
		if (secsize != trk[i].sector_size) {
			secsize = 0;
			break;
		}
	}
	if (secsize != 0) {
		switch (secsize) {
			case	2048:
				sxsi->read = sec2048_read;
				break;
			case	2352:
				sxsi->read = sec2352_read_with_ecc; // sec2352_read;
				break;
			case	2448:
				sxsi->read = sec2448_read;
				break;
		}
	}
	else {
		sxsi->read = sec_read;
	}
}

//
//#define	TOCLOGOUT
#ifdef	TOCLOGOUT
#define	TOCLOG(fmt, val)	\
			_stprintf(logbuf, fmt, val);	\
			textfile_write(tfh, logbuf);
static const OEMCHAR str_logB[] = OEMTEXT("._CDTRK.Before.log");
static const OEMCHAR str_logA[] = OEMTEXT("._CDTRK.After.log");
#endif
//

//	イメージファイルの実体を開き、各種情報構築
static BRESULT setsxsidev_layout(SXSIDEV sxsi, const OEMCHAR *path, const _CDTRK *trk, UINT trks, UINT8 layout) {

	FILEH	fh;
	long	totals;
	CDINFO	cdinfo;
	UINT	mediatype;
	UINT	i;
#ifdef	TOCLOGOUT
	OEMCHAR		logpath[MAX_PATH];
	OEMCHAR		logbuf[2048];
	TEXTFILEH	tfh;
#endif

	makeCRCTable();

	//	trk、trksは有効な値が設定済みなのが前提
	if ((trk == NULL) || (trks == 0)) {
		goto sxsiope_err1;
	}

	fh = file_open_rb(path);
	if (fh == FILEH_INVALID) {
		goto sxsiope_err1;
	}

	cdinfo = (CDINFO)_MALLOC(sizeof(_CDINFO), path);
	if (cdinfo == NULL) {
		goto sxsiope_err2;
	}
	ZeroMemory(cdinfo, sizeof(_CDINFO));
	cdinfo->fh = fh;
	cdinfo->layout = layout;
	trks = min(trks, NELEMENTS(cdinfo->trk) - 1);
	CopyMemory(cdinfo->trk, trk, trks * sizeof(_CDTRK));

#ifdef	TOCLOGOUT
	file_cpyname(logpath, path, NELEMENTS(logpath));
	file_cutext(logpath);
	file_catname(logpath, str_logB, NELEMENTS(logpath));

	tfh = textfile_create(logpath, 0x800);
	if (tfh == NULL) {
		return(FAILURE);
	}

	TOCLOG(OEMTEXT("STR _CDTRK LOG\r\n"), 0);
	for (i = 0; i < trks; i++) {
		TOCLOG(OEMTEXT("trk[%02d]\r\n"), i);
		TOCLOG(OEMTEXT("  adr_ctl        = 0x%02X\r\n"),     cdinfo->trk[i].adr_ctl);
		TOCLOG(OEMTEXT("  point          = %02d\r\n"),       cdinfo->trk[i].point);
		TOCLOG(OEMTEXT("  [pos0][pos][ ]              = [%18I32d]"), cdinfo->trk[i].pos0);
		TOCLOG(OEMTEXT("[%18I32d][                  ]\r\n"),         cdinfo->trk[i].pos);
		TOCLOG(OEMTEXT("  sec[ ][str][end]            = [                  ][%18I32d]"), cdinfo->trk[i].str_sec);
		TOCLOG(OEMTEXT("[%18I32d]\r\n"), cdinfo->trk[i].end_sec);
		TOCLOG(OEMTEXT("  sectors        = %I32d\r\n"),      cdinfo->trk[i].sectors);
		TOCLOG(OEMTEXT("  sector_size    = %d\r\n"),         cdinfo->trk[i].sector_size);
		TOCLOG(OEMTEXT("  sector [pregap][start][end] = [%18I32d]"), cdinfo->trk[i].pregap_sector);
		TOCLOG(OEMTEXT("[%18I32d]"),     cdinfo->trk[i].start_sector);
		TOCLOG(OEMTEXT("[%18I32d]\r\n"), cdinfo->trk[i].end_sector);
		TOCLOG(OEMTEXT("  img_sec[pregap][start][end] = [%18I32d]"), cdinfo->trk[i].img_pregap_sec);
		TOCLOG(OEMTEXT("[%18I32d]"),     cdinfo->trk[i].img_start_sec);
		TOCLOG(OEMTEXT("[%18I32d]\r\n"), cdinfo->trk[i].img_end_sec);
		TOCLOG(OEMTEXT("  offset [pregap][start][end] = [0x%016I64X]"), cdinfo->trk[i].pregap_offset);
		TOCLOG(OEMTEXT("[0x%016I64X]"),     cdinfo->trk[i].start_offset);
		TOCLOG(OEMTEXT("[0x%016I64X]\r\n"), cdinfo->trk[i].end_offset);
		TOCLOG(OEMTEXT("  pregap_sectors = %I32d\r\n"),      cdinfo->trk[i].pregap_sectors);
		TOCLOG(OEMTEXT("  track_sectors  = %I32d\r\n"),      cdinfo->trk[i].track_sectors);
	}
	TOCLOG(OEMTEXT("END _CDTRK LOG\r\n"), 0);

	textfile_close(tfh);
#endif

#if 1
	if (sxsi->totals == -1) {
		totals = set_trkinfo(fh, cdinfo->trk, trks, 0);
		if (totals < 0) {
			goto sxsiope_err3;
		}
		sxsi->totals = totals;
	}
#else
	totals = issec(fh, cdinfo->trk, trks);	//	とりあえず
	sxsi->read = sec2048_read;
	totals = issec2048(cdinfo->fh);
	if (totals < 0) {
		sxsi->read = sec2352_read;
		totals = issec2352(cdinfo->fh);
	}
	if (totals < 0) {
		sxsi->read = sec2448_read;
		totals = issec2448(cdinfo->fh);
	}
	if (totals < 0) {
		sxsi->read = sec_read;
		totals = issec(cdinfo->fh, cdinfo->trk, trks);
	}
	if (totals < 0) {
		goto sxsiope_err3;
	}
#endif

	mediatype = 0;
	for (i = 0; i < trks; i++) {
		if (cdinfo->trk[i].adr_ctl == TRACKTYPE_DATA) {
			mediatype |= SXSIMEDIA_DATA;
		}
		else if (cdinfo->trk[i].adr_ctl == TRACKTYPE_AUDIO) {
			mediatype |= SXSIMEDIA_AUDIO;
		}
	}

	//	リードアウトトラックを生成
	cdinfo->trk[trks].adr_ctl	= (trks >= 1) ? cdinfo->trk[trks - 1].adr_ctl : 0x10;
	cdinfo->trk[trks].point		= 0xaa;
//	cdinfo->trk[trks].pos		= totals;
	cdinfo->trk[trks].pos		= (UINT32)sxsi->totals;
	cdinfo->trk[trks].pos0		= cdinfo->trk[trks].pos;
	cdinfo->trk[trks].str_sec	= cdinfo->trk[trks].pos;
	cdinfo->trk[trks].end_sec	= cdinfo->trk[trks].pos;
	cdinfo->trk[trks].sectors	= 0;
	cdinfo->trk[trks].pregap_sectors = 0;
	cdinfo->trk[trks].pregap_offset_ex = (trks >= 1) ? cdinfo->trk[trks - 1].pregap_offset_ex : 0;

	cdinfo->trks = trks;
	file_cpyname(cdinfo->path, path, NELEMENTS(cdinfo->path));

	sxsi->reopen		= cd_reopen;
	sxsi->close			= cd_close;
	sxsi->destroy		= cd_destroy;
	sxsi->hdl			= (INTPTR)cdinfo;
//	sxsi->totals		= totals;
	sxsi->cylinders		= (sxsi->totals + 17 * 8 - 1) / (17 * 8);
	sxsi->size			= 2048;
	sxsi->sectors		= 8;
	sxsi->surfaces		= 17;
	sxsi->headersize	= 0;
	sxsi->mediatype		= mediatype;

#ifdef	TOCLOGOUT
	file_cpyname(logpath, path, NELEMENTS(logpath));
	file_cutext(logpath);
	file_catname(logpath, str_logA, NELEMENTS(logpath));

	tfh = textfile_create(logpath, 0x800);
	if (tfh == NULL) {
		return(FAILURE);
	}

	TOCLOG(OEMTEXT("STR _CDTRK LOG\r\n"), 0);
	for (i = 0; i < trks; i++) {
		TOCLOG(OEMTEXT("trk[%02d]\r\n"), i);
		TOCLOG(OEMTEXT("  adr_ctl        = 0x%02X\r\n"),     cdinfo->trk[i].adr_ctl);
		TOCLOG(OEMTEXT("  point          = %02d\r\n"),       cdinfo->trk[i].point);
		TOCLOG(OEMTEXT("  [pos0][pos][ ]              = [%18I32d]"), cdinfo->trk[i].pos0);
		TOCLOG(OEMTEXT("[%18I32d][                  ]\r\n"),         cdinfo->trk[i].pos);
		TOCLOG(OEMTEXT("  sec[ ][str][end]            = [                  ][%18I32d]"), cdinfo->trk[i].str_sec);
		TOCLOG(OEMTEXT("[%18I32d]\r\n"), cdinfo->trk[i].end_sec);
		TOCLOG(OEMTEXT("  sectors        = %I32d\r\n"),      cdinfo->trk[i].sectors);
		TOCLOG(OEMTEXT("  sector_size    = %d\r\n"),         cdinfo->trk[i].sector_size);
		TOCLOG(OEMTEXT("  sector [pregap][start][end] = [%18I32d]"), cdinfo->trk[i].pregap_sector);
		TOCLOG(OEMTEXT("[%18I32d]"),     cdinfo->trk[i].start_sector);
		TOCLOG(OEMTEXT("[%18I32d]\r\n"), cdinfo->trk[i].end_sector);
		TOCLOG(OEMTEXT("  img_sec[pregap][start][end] = [%18I32d]"), cdinfo->trk[i].img_pregap_sec);
		TOCLOG(OEMTEXT("[%18I32d]"),     cdinfo->trk[i].img_start_sec);
		TOCLOG(OEMTEXT("[%18I32d]\r\n"), cdinfo->trk[i].img_end_sec);
		TOCLOG(OEMTEXT("  offset [pregap][start][end] = [0x%016I64X]"), cdinfo->trk[i].pregap_offset);
		TOCLOG(OEMTEXT("[0x%016I64X]"),     cdinfo->trk[i].start_offset);
		TOCLOG(OEMTEXT("[0x%016I64X]\r\n"), cdinfo->trk[i].end_offset);
		TOCLOG(OEMTEXT("  pregap_sectors = %I32d\r\n"),      cdinfo->trk[i].pregap_sectors);
		TOCLOG(OEMTEXT("  track_sectors  = %I32d\r\n"),      cdinfo->trk[i].track_sectors);
	}
	TOCLOG(OEMTEXT("END _CDTRK LOG\r\n"), 0);

	textfile_close(tfh);
#endif

	return(SUCCESS);

sxsiope_err3:
	_MFREE(cdinfo);

sxsiope_err2:
	file_close(fh);

sxsiope_err1:
	return(FAILURE);
}

BRESULT setsxsidev(SXSIDEV sxsi, const OEMCHAR *path, const _CDTRK *trk, UINT trks) {
	return(setsxsidev_layout(sxsi, path, trk, trks, CDINFO_LAYOUT_DEFAULT));
}

BRESULT setsxsidev_cue(SXSIDEV sxsi, const OEMCHAR *path, const _CDTRK *trk, UINT trks) {
	return(setsxsidev_layout(sxsi, path, trk, trks, CDINFO_LAYOUT_CUE));
}

#endif
