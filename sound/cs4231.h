/**
sudo make - * @file	cs4231.h
 * @brief	Interface of the CS4231
 */

#pragma once

#include "sound.h"
#include "io/dmac.h"

enum {
	CS4231_BUFFERS	= (1 << 14),
	CS4231_BUFMASK	= (CS4231_BUFFERS - 1)
};

typedef struct {
	UINT8	adc_l;				// 0
	UINT8	adc_r;				// 1
	UINT8	aux1_l;				// 2
	UINT8	aux1_r;				// 3
	UINT8	aux2_l;				// 4
	UINT8	aux2_r;				// 5
	UINT8	dac_l;				// 6
	UINT8	dac_r;				// 7
	UINT8	datafmt;			// 8
	UINT8	iface;				// 9
	UINT8	pinctrl;			// a
	UINT8	errorstatus;		//b
	UINT8	mode_id;		//c
	UINT8	loopctrl;		//d
	UINT8	playcount[2];		//e-f
	UINT8	featurefunc[2];		//10-11
	UINT8	line_l;		//12
	UINT8	line_r;		//13
	UINT8	timer[2];			//14-15
	UINT8	reserved1;		//16
	UINT8	reserved2;		//17
	UINT8	featurestatus;		//18
	UINT8	chipid;		//19
	UINT8	monoinput;		//1a
	UINT8	reserved3;		//1b
	UINT8	cap_datafmt;		//1c
	UINT8	reserved4;		//1d
	UINT8	cap_basecount[2];		//1e-1f
} CS4231REG;

typedef struct {
	UINT		bufsize;
	UINT		bufdatas;
	UINT		bufpos;
	UINT32		pos12;
	UINT32		step12;

	UINT8		enable;
	UINT8		portctrl;
	UINT8		dmairq;
	UINT8		dmach;
	UINT16		port[16];
	UINT8		adrs;
	UINT8		index;
	UINT8		intflag;
	UINT8		outenable;
	UINT8		extfunc;
	UINT8		extindex;

	CS4231REG	reg;
	UINT8		buffer[CS4231_BUFFERS*2];
} _CS4231, *CS4231;

typedef struct {
	UINT	rate;
} CS4231CFG;


#ifdef __cplusplus
extern "C"
{
#endif

//Index Address Register 0xf44
#define TRD (1 << 5) //cs4231.index bit5 Transfer Request Disable
#define MCE (1 << 6) //cs4231.index bit6 Mode Change Enable

//Status Register 0xf46
#define INt (1 << 0) //cs4231.intflag bit0 Interrupt Status
#define PRDY (1 << 1) //cs4231.intflag bit1 Playback Data Ready(PIO data)
#define PLR (1 << 2) //cs4231.intflag bit2 Playback Left/Right Sample
#define PULR (1 << 3) //cs4231.intflag bit3 Playback Upper/Lower Byte
#define SER (1 << 4) //cs4231.intflag bit4 Sample Error
#define CRDY (1 << 5) //cs4231.intflag bit5 Capture Data Ready
#define CLR (1 << 6) //cs4231.intflag bit6 Capture Left/Right Sample
#define CUL (1 << 7) //cs4231.intglag bit7 Capture Upper/Lower Byte

//cs4231.reg.iface(9)
#define PEN (1 << 0) //bit0 Playback Enable set and reset without MCE
#define CEN (1 << 1) //bit1 Capture Enable
#define SDC (1 << 2) //bit2 Single DMA Channel 0 Dual 1 Single �t�Ǝv���Ă��̂ŏC�����ׂ�
#define CAL0 (1 << 3) //bit3 Calibration 0 No Calibration 1 Converter calibration
#define CAL1 (1 << 4) //bit4 2 DAC calibration 3 Full Calibration
#define PPIO (1 << 6) //bit6 Playback PIO Enable 0 DMA 1 PIO
#define CPIO (1 << 7) //bit7 Capture PIO Enable 0 DMA 1 PIO

//cs4231.reg.errorstatus(11)b
#define ACI (1 << 5) //bit5 Auto-calibrate In-Progress

//cs4231.reg.pinctrl(10)a
#define IEN (1 << 1) //bit1 Interrupt Enable reflect cs4231.intflag bit0
#define DEN (1 << 3) //bit3 Dither Enable only active in 8-bit unsigned mode

//cs4231.reg.modeid(12)c
#define MODE2 (1 << 6) //bit6

//cs4231.reg.featurestatus(24)0x18
#define PI (1 << 4) //bit4 Playback Interrupt pending from Playback DMA count registers
#define CI (1 << 5) //bit5 Capture Interrupt pending from record DMA count registers when SDC=1 non-functional
#define TI (1 << 6) //bit6 Timer Interrupt pending from timer count registers
// PI,CI,TI bits are reset by writing a "0" to the particular interrupt bit or by writing any value to the Status register

void cs4231_dma(NEVENTITEM item);
REG8 DMACCALL cs4231dmafunc(REG8 func);
void cs4231_datasend(REG8 dat);

void cs4231_initialize(UINT rate);
void cs4231_setvol(UINT vol);

void cs4231_reset(void);
void cs4231_update(void);
void cs4231_control(UINT index, REG8 dat);

void SOUNDCALL cs4231_getpcm(CS4231 cs, SINT32 *pcm, UINT count);

extern int cs4231_lock;

#ifdef __cplusplus
}
#endif
