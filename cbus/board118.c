﻿/**
 * @file	board118.c
 * @brief	Implementation of PC-9801-118
 */

#include "compiler.h"
#include "board118.h"
#include "pccore.h"
#include "iocore.h"
#include "cbuscore.h"
#include "cs4231io.h"
#include "sound/fmboard.h"
#include "sound/sound.h"
#include "sound/soundrom.h"
#include "mpu98ii.h"


static int opna_idx = 0;

/* for OPL */

#ifdef USE_MAME
#ifdef SUPPORT_SOUND_SB16
#include "boardsb16.h"
static void *opl3;
static int samplerate;
static double oplfm_volume;
void *YMF262Init(INT clock, INT rate);
void YMF262ResetChip(void *chip);
void YMF262Shutdown(void *chip);
INT YMF262Write(void *chip, INT a, INT v);
UINT8 YMF262Read(void *chip, INT a);
void YMF262UpdateOne(void *chip, INT16 **buffer, INT length);

static void IOOUTCALL sb16_o20d2(UINT port, REG8 dat) {
	(void)port;
	g_opl.addr = dat;
	YMF262Write(opl3, 0, dat);
}

static void IOOUTCALL sb16_o21d2(UINT port, REG8 dat) {
	(void)port;
	g_opl.reg[g_opl.addr] = dat;
	//S98_put(NORMAL2608, g_opl.addr, dat);
	//opl3_writeRegister(&g_opl3, g_opl3.s.addrl, dat);
	YMF262Write(opl3, 1, dat);
}
static void IOOUTCALL sb16_o22d2(UINT port, REG8 dat) {
	(void)port;
	g_opl.addr2 = dat;
	YMF262Write(opl3, 2, dat);
}

static void IOOUTCALL sb16_o23d2(UINT port, REG8 dat) {
	(void)port;
	g_opl.reg[g_opl.addr2 + 0x100] = dat;
	//opl3_writeExtendedRegister(&g_opl3, g_opl3.s.addrh, dat);
	//S98_put(EXTEND2608, opl.addr2, dat);
	YMF262Write(opl3, 3, dat);
}

static void IOOUTCALL sb16_o28d2(UINT port, REG8 dat) {
	/**
	 * いわゆるPC/ATで言うところのAdlib互換ポート
	 * UltimaUnderWorldではこちらを叩く
	 */
	port = dat;
	YMF262Write(opl3, 0, dat);
}
static void IOOUTCALL sb16_o29d2(UINT port, REG8 dat) {
	port = dat;
	YMF262Write(opl3, 1, dat);
}

static REG8 IOINPCALL sb16_i20d2(UINT port) {
	(void)port;
	return YMF262Read(opl3, 0);
}

static REG8 IOINPCALL sb16_i22d2(UINT port) {
	(void)port;
	return YMF262Read(opl3, 1);
}

static REG8 IOINPCALL sb16_i28d2(UINT port) {
	(void)port;
	return YMF262Read(opl3, 0);
}
#endif
#endif

static void IOOUTCALL ymf_o188(UINT port, REG8 dat)
{
	g_opna[opna_idx].s.addrl = dat;
	g_opna[opna_idx].s.addrh = 0;
	g_opna[opna_idx].s.data = dat;
	(void)port;
}

static void IOOUTCALL ymf_o18a(UINT port, REG8 dat)
{
	g_opna[opna_idx].s.data = dat;
	if (g_opna[opna_idx].s.addrh != 0) {
		return;
	}

	if (g_opna[opna_idx].s.addrl == 0x27) {
		/* OPL3-LにCSMモードは無い */
		dat &= ~0x80;
		g_opna[opna_idx].opngen.opnch[2].extop = dat & 0xc0;
	}

	opna_writeRegister(&g_opna[opna_idx], g_opna[opna_idx].s.addrl, dat);

	(void)port;
}


static void IOOUTCALL ymf_o18c(UINT port, REG8 dat)
{
	if (g_opna[opna_idx].s.extend)
	{
		g_opna[opna_idx].s.addrl = dat;
		g_opna[opna_idx].s.addrh = 1;
		g_opna[opna_idx].s.data = dat;
	}
	(void)port;
}

static void IOOUTCALL ymf_o18e(UINT port, REG8 dat)
{
	if (!g_opna[opna_idx].s.extend)
	{
		return;
	}
	g_opna[opna_idx].s.data = dat;

	if (g_opna[opna_idx].s.addrh != 1)
	{
		return;
	}

	opna_writeExtendedRegister(&g_opna[opna_idx], g_opna[opna_idx].s.addrl, dat);

	(void)port;
}

static REG8 IOINPCALL ymf_i188(UINT port)
{
	(void)port;
	return g_opna[opna_idx].s.status;
}

static REG8 IOINPCALL ymf_i18a(UINT port)
{
	UINT nAddress;

	if (g_opna[opna_idx].s.addrh == 0)
	{
		nAddress = g_opna[opna_idx].s.addrl;
		if (nAddress == 0x0e)
		{
			return fmboard_getjoy(&g_opna[opna_idx]);
		}
		else if (nAddress < 0x10)
		{
			return opna_readRegister(&g_opna[opna_idx], nAddress);
		}
		else if (nAddress == 0xff)
		{
			return 1;
		}
	}

	(void)port;
	return g_opna[opna_idx].s.data;
}

static REG8 IOINPCALL ymf_i18c(UINT port)
{
	if (g_opna[opna_idx].s.extend)
	{
		return (g_opna[opna_idx].s.status & 3);
	}

	(void)port;
	return 0xff;
}

static void extendchannel(REG8 enable)
{
	g_opna[opna_idx].s.extend = enable;
	if (enable)
	{
		opngen_setcfg(&g_opna[opna_idx].opngen, 6, OPN_STEREO | 0x007);
	}
	else
	{
		opngen_setcfg(&g_opna[opna_idx].opngen, 3, OPN_MONORAL | 0x007);
		rhythm_setreg(&g_opna[opna_idx].rhythm, 0x10, 0xff);
	}
}

static void IOOUTCALL ymf_oa460(UINT port, REG8 dat)
{
	cs4231.extfunc = dat;
	extendchannel((REG8)(dat & 1));
	(void)port;
}

static REG8 IOINPCALL ymf_ia460(UINT port)
{
	(void)port;
	if(g_nSoundID==SOUNDID_MATE_X_PCM || g_nSoundID==SOUNDID_PC_9801_86_WSS){
		return (0x70 | (cs4231.extfunc & 1));
	}else{
		return (0x80 | (cs4231.extfunc & 1));
	}
}
char srnf;
static void IOOUTCALL srnf_oa460(UINT port, REG8 dat)
{
	srnf = dat;
	(void)port;
}

static REG8 IOINPCALL srnf_ia460(UINT port)
{
	(void)port;
	return (srnf);
}


static REG8 IOINPCALL wss_i881e(UINT port)
{
	if(g_nSoundID==SOUNDID_MATE_X_PCM || g_nSoundID==SOUNDID_PC_9801_86_WSS){
		int ret = 0x64;
		ret |= (cs4231.dmairq-1) << 3;
		if((cs4231.dmairq-1)==0x1 || (cs4231.dmairq-1)==0x2){
			ret |= 0x80; // 奇数パリティ
		}
		return (ret);
	}else{
		return (0xDC);
	}
}
REG8 ymf701;
static void IOOUTCALL wss_o548e(UINT port, REG8 dat)
{
	ymf701 = dat;
}
static REG8 IOINPCALL wss_i548e(UINT port)
{
	return (ymf701); 
}
static REG8 IOINPCALL wss_i548f(UINT port)
{
	if(ymf701 == 0) return 0xe8;
	else if(ymf701 == 0x1) return 0xfe;
	else if(ymf701 == 0x2) return 0x40;
	else if(ymf701 == 0x3) return 0x30;
	else if(ymf701 == 0x4) return 0xff;
	else if(ymf701 == 0x20) return 0x04;
	else if(ymf701 == 0x40) return 0x20;
	else return 0;// from PC-9821Nr166
}

static void IOOUTCALL ym_o1488(UINT port, REG8 dat) //FM Music Register Address Port
{
	g_opl3.s.addrl = dat;
	(void)port;
}
REG8 opl_data;
static void IOOUTCALL ym_o1489(UINT port, REG8 dat) //FM Music Data Port
{
	opl3_writeRegister(&g_opl3, g_opl3.s.addrl, dat);
	opl_data = dat;
	(void)port;
}


static void IOOUTCALL ym_o148a(UINT port, REG8 dat) // Advanced FM Music Register Address	 Port
{
	g_opl3.s.addrh = dat;
	(void)port;
}
static void IOOUTCALL ym_o148b(UINT port, REG8 dat) //Advanced FM Music Data Port
{
	opl3_writeExtendedRegister(&g_opl3, g_opl3.s.addrh, dat);
	(void)port;
}

static REG8 IOINPCALL ym_i1488(UINT port) //FM Music Status Port
{
	TRACEOUT(("%x read",port));
//	if (opl_data == 0x80) return 0xe0;
	if (opl_data == 0x21) return 0xc0;
	return 0;
}

static REG8 IOINPCALL ym_i1489(UINT port) //  ???
{
	TRACEOUT(("%x read",port));
	return opl3_readRegister(&g_opl3, g_opl3.s.addrl);
}
static REG8 IOINPCALL ym_i148a(UINT port) //Advanced FM Music Status Port
{
	TRACEOUT(("%x read",port));
	return opl3_readStatus(&g_opl3);
}

static REG8 IOINPCALL ym_i148b(UINT port) //  ???
{
	TRACEOUT(("%x read",port));
	return opl3_readExtendedRegister(&g_opl3, g_opl3.s.addrh);
}
REG8 sound118;
static void IOOUTCALL csctrl_o148e(UINT port, REG8 dat) {
	sound118 = dat;
}

static REG8 IOINPCALL csctrl_i148e(UINT port) {
	return(sound118);
}
REG8 control118;
static REG8 IOINPCALL csctrl_i148f(UINT port) {

	(void)port;
	if(sound118 == 0)	return(0x03);
/*	if(sound118 == 0x05) {
		if(control118 == 0x04)return (0x04);
		else if(control118 == 0) return 0;}
	if(sound118 == 0x04) return (0x00);
	if(sound118 == 0x21) return (0x00);
	if(sound118 == 0xff) return (0x00);
	else
*/	return(0xff);
}

static void IOOUTCALL csctrl_o148f(UINT port, REG8 dat) {
	control118 = dat;
}

static REG8 IOINPCALL csctrl_i486(UINT port) {
	return(0);
}

static REG8 IOINPCALL sb98_i2ad2(UINT port) {
	TRACEOUT(("%x read",port));
	return(0xaa);
}

static REG8 IOINPCALL sb98_i2ed2(UINT port) {
	TRACEOUT(("%x read",port));
	return(0xff);
}

static REG8 IOINPCALL sb98_i81d2(UINT port) {
//	TRACEOUT(("%x read",port));
	return(0xbf);
}

// ----
#ifdef USE_MAME
#ifdef SUPPORT_SOUND_SB16
void SOUNDCALL opl3gen_getpcm2(void* opl3, SINT32 *pcm, UINT count) {
	UINT i;
	INT16 *buf[4];
	INT16 s1l,s1r,s2l,s2r;
	SINT32 *outbuf = pcm;
	buf[0] = &s1l;
	buf[1] = &s1r;
	buf[2] = &s2l;
	buf[3] = &s2r;
	oplfm_volume = np2cfg.vol_fm / 64.0;
	for (i=0; i < count; i++) {
		s1l = s1r = s2l = s2r = 0;
		YMF262UpdateOne(opl3, buf, 1);
		outbuf[0] += (SINT32)(s1l * 8 * oplfm_volume);
		outbuf[1] += (SINT32)(s1r * 8 * oplfm_volume);
		outbuf += 2;
	}
}
#endif
#endif
static const IOOUT ymf_o[4] = {
			ymf_o188,	ymf_o18a,	ymf_o18c,	ymf_o18e};

static const IOINP ymf_i[4] = {
			ymf_i188,	ymf_i18a,	ymf_i18c,	NULL};



/**
 * Reset
 * @param[in] pConfig A pointer to a configure structure
 */
void board118_reset(const NP2CFG *pConfig)
{
	if(g_nSoundID==SOUNDID_PC_9801_86_WSS){
		opna_idx = 1;
	}else{
		opna_idx = 0;
	}
	opna_reset(&g_opna[opna_idx], OPNA_MODE_2608 | OPNA_HAS_TIMER | OPNA_S98);
	if(g_nSoundID==SOUNDID_PC_9801_86_WSS){
		opna_timer(&g_opna[opna_idx], 0x10, NEVENT_FMTIMERA, NEVENT_FMTIMERB);
	}else{
		opna_timer(&g_opna[opna_idx], 0xd0, NEVENT_FMTIMERA, NEVENT_FMTIMERB);
	}
	opl3_reset(&g_opl3, OPL3_HAS_OPL3L|OPL3_HAS_OPL3);
	opngen_setcfg(&g_opna[opna_idx].opngen, 3, OPN_STEREO | 0x038);
	cs4231io_reset();
	soundrom_load(0xcc000, OEMTEXT("118"));
	fmboard_extreg(extendchannel);
#ifdef SUPPORT_SOUND_SB16
#ifdef USE_MAME
	if (opl3) {
		if (samplerate != pConfig->samplingrate) {
			YMF262Shutdown(opl3);
			opl3 = YMF262Init(14400000, pConfig->samplingrate);
			samplerate = pConfig->samplingrate;
		} else {
			YMF262ResetChip(opl3);
		}
	}
	ZeroMemory(&g_sb16, sizeof(g_sb16));
	ZeroMemory(&g_opl, sizeof(g_opl));
	// ボードデフォルト IO:D2 DMA:3 INT:5 
	g_sb16.base = 0xd2;
	g_sb16.dmach = 0x3;
	g_sb16.dmairq = 0x5;
#endif
#endif
	(void)pConfig;
}

/**
 * Bind
 */
void board118_bind(void)
{
		cs4231io_bind();
	if(g_nSoundID==SOUNDID_PC_9801_86_WSS){
		opna_idx = 1;
	}else{
		opna_idx = 0;
	}
	if(g_nSoundID==SOUNDID_PC_9801_86_WSS){
		iocore_attachout(0xb460, ymf_oa460);
		iocore_attachinp(0xb460, ymf_ia460);
	}else{
		opna_bind(&g_opna[opna_idx]);
		cbuscore_attachsndex(cs4231.port[4],ymf_o, ymf_i);
#ifdef SUPPORT_SOUND_SB16
#ifdef USE_MAME
		iocore_attachout(cs4231.port[9], sb16_o20d2);
		iocore_attachinp(cs4231.port[9], sb16_i20d2);
		iocore_attachout(cs4231.port[9]+1, sb16_o21d2);
		iocore_attachout(cs4231.port[9]+2, sb16_o22d2);
		iocore_attachout(cs4231.port[9]+3, sb16_o23d2);

		//偽SB-16 mode
		iocore_attachout(0x20d2, sb16_o20d2);//sb16 opl
		iocore_attachinp(0x20d2, sb16_i20d2);//sb16 opl
		iocore_attachout(0x21d2, sb16_o21d2);//sb16 opl
		iocore_attachout(0x22d2, sb16_o22d2);//sb16 opl3
		iocore_attachout(0x23d2, sb16_o23d2);//sb16 opl3
		iocore_attachout(0x28d2, sb16_o28d2);//sb16 opl?
		iocore_attachinp(0x28d2, sb16_i28d2);//sb16 opl
		iocore_attachout(0x29d2, sb16_o29d2);//sb16 opl?

		iocore_attachinp(0x81d2, sb98_i81d2);// sb16 midi port 以下３つはMSDRV4を騙すために必要
		iocore_attachinp(0x2ad2, sb98_i2ad2);// DSP Read Data Port
		iocore_attachinp(0x2ed2, sb98_i2ed2);// DSP Read Buffer Status (Bit 7)
		if (!opl3) {
			opl3 = YMF262Init(14400000, np2cfg.samplingrate);
			samplerate = np2cfg.samplingrate;
		}
		sound_streamregist(opl3, (SOUNDCB)opl3gen_getpcm2);
#else
		iocore_attachout(0x20d2, ym_o1488);//sb16 opl
		iocore_attachinp(0x20d2, ym_i1488);//sb16 opl
		iocore_attachout(0x21d2, ym_o1489);//sb16 opl
		iocore_attachout(0x22d2, ym_o148a);//sb16 opl3
		iocore_attachout(0x23d2, ym_o148b);//sb16 opl3
		iocore_attachout(0x28d2, ym_o1488);//sb16 opl?
		iocore_attachinp(0x28d2, ym_i1488);//sb16 opl
		iocore_attachout(0x29d2, ym_o1489);//sb16 opl?
		iocore_attachinp(0x81d2, csctrl_i486);// sb16 midi port
		iocore_attachinp(0x2ad2, sb98_i2ad2);// DSP Read Data Port
		iocore_attachinp(0x2ed2, sb98_i2ed2);// DSP Read Buffer Status (Bit 7)
#endif
#else
		iocore_attachout(cs4231.port[9], ym_o1488);
		iocore_attachinp(cs4231.port[9], ym_i1488);
		iocore_attachout(cs4231.port[9]+1, ym_o1489);
		iocore_attachout(cs4231.port[9]+2, ym_o148a);
		iocore_attachout(cs4231.port[9]+3, ym_o148b);
		opl3_bind(&g_opl3);
#endif
		iocore_attachout(cs4231.port[1], ymf_oa460);
		iocore_attachinp(cs4231.port[1], ymf_ia460);

		iocore_attachout(cs4231.port[15],srnf_oa460);//SRN-Fは必要なときだけ使う
		iocore_attachinp(cs4231.port[15],srnf_ia460);
		srnf = 0x81;

		iocore_attachout(cs4231.port[14],csctrl_o148e);
		iocore_attachinp(cs4231.port[14],csctrl_i148e);
		iocore_attachout(cs4231.port[14]+1,csctrl_o148f);
		iocore_attachinp(cs4231.port[14]+1,csctrl_i148f);
		iocore_attachout(cs4231.port[6], wss_o548e);// YMF-701
		iocore_attachinp(cs4231.port[6], wss_i548e);// YMF-701
		iocore_attachinp(cs4231.port[6]+1, wss_i548f);// YMF-701
		iocore_attachinp(cs4231.port[11]+6,csctrl_i486);
		iocore_attachinp(0x881e, wss_i881e);
//		iocore_attachout(cs4231.port[10], mpu98ii_o0);//MIDI PORT mpu98ii.c側で調整
//		iocore_attachinp(cs4231.port[10], mpu98ii_i0);
//		iocore_attachout(cs4231.port[10]+1, mpu98ii_o2);
//		iocore_attachinp(cs4231.port[10]+1, mpu98ii_i2);
		//mpu98.irqnum = mpu98.irqnum2;

	}
}
