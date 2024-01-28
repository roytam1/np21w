/**
 * @file	cs4231c.c
 * @brief	Implementation of the CS4231
 */

#include "compiler.h"
#include "cs4231.h"
#include "iocore.h"
#include "fmboard.h"
#include "dmac.h"
#include "cpucore.h"
#ifndef CPU_STAT_PM
#define CPU_STAT_PM	0
#endif

	CS4231CFG	cs4231cfg;

	static SINT32 cs4231_totalsample = 0;

enum {
	CS4231REG_LINPUT	= 0x00,
	CS4231REG_RINPUT	= 0x01,
	CS4231REG_AUX1L		= 0x02,
	CS4231REG_AUX1R		= 0x03,
	CS4231REG_AUX2L		= 0x04,
	CS4231REG_AUX2R		= 0x05,
	CS4231REG_LOUTPUT	= 0x06,
	CS4231REG_ROUTPUT	= 0x07,
	CS4231REG_PLAYFMT	= 0x08,
	CS4231REG_INTERFACE	= 0x09,
	CS4231REG_PINCTRL	= 0x0a,
	CS4231REG_TESTINIT	= 0x0b,
	CS4231REG_MISCINFO	= 0x0c,
	CS4231REG_LOOPBACK	= 0x0d,
	CS4231REG_PLAYCNTM	= 0x0e,
	CS4231REG_PLAYCNTL	= 0x0f,

	CS4231REG_FEATURE1	= 0x10,
	CS4231REG_FEATURE2	= 0x11,
	CS4231REG_LLINEIN	= 0x12,
	CS4231REG_RLINEIN	= 0x13,
	CS4231REG_TIMERL	= 0x14,
	CS4231REG_TIMERH	= 0x15,
	CS4231REG_RESERVED1= 0x16,
	CS4231REG_RESERVED2= 0x17,
	CS4231REG_IRQSTAT	= 0x18,
	CS4231REG_VERSION	= 0x19,
	CS4231REG_MONOCTRL	= 0x1a,
	CS4231REG_RECFMT	= 0x1c,
	CS4231REG_PLAYFREQ	= 0x1d,
	CS4231REG_RECCNTM	= 0x1e,
	CS4231REG_RECCNTL	= 0x1f
};


UINT dmac_getdata_(DMACH dmach, UINT8 *buf, UINT offset, UINT size);
static const UINT32 cs4231xtal64[2] = {24576000/64, 16934400/64};

static const UINT8 cs4231cnt64[8] = {
				3072/64,	//  8000/ 5510
				1536/64,	// 16000/11025
				 896/64,	// 27420/18900
				 768/64,	// 32000/22050
				 448/64,	// 54840/37800
				 384/64,	// 64000/44100
				 512/64,	// 48000/33075
				2560/64};	//  9600/ 6620

//    640:441
				
void cs4231_initialize(UINT rate) {

	cs4231cfg.rate = rate;
}

void cs4231_setvol(UINT vol) {

	(void)vol;
}

// CS4231 DMA����
void cs4231_dma(NEVENTITEM item) {

	DMACH	dmach;
	UINT	rem;
	UINT	pos;
	UINT	size;
	UINT	r;
	SINT32	cnt;
	if (item->flag & NEVENT_SETEVENT) {
		if (cs4231.dmach != 0xff) {
			dmach = dmac.dmach + cs4231.dmach;

			// �T�E���h�Đ��p�o�b�t�@�ɑ���H(cs4231g.c)
			sound_sync();

			// �o�b�t�@�ɋ󂫂�����΃f�[�^��ǂݏo��
			if (cs4231.bufsize-4 > cs4231.bufdatas) {
				rem = min(cs4231.bufsize - 4 - cs4231.bufdatas, 512); //�ǂݎ��P�ʂ�16bit�X�e���I��1�T���v����(4byte)�ɂ��Ă����Ȃ��ƎG��������
				pos = cs4231.bufwpos & CS4231_BUFMASK; // �o�b�t�@�������݈ʒu
				size = min(rem, dmach->startcount); // �o�b�t�@�������݃T�C�Y
				r = dmac_getdata_(dmach, cs4231.buffer, pos, size); // DMA�ǂݎ����s
				cs4231.bufwpos = (cs4231.bufwpos + r) & CS4231_BUFMASK; // �o�b�t�@�������݈ʒu���X�V
				cs4231.bufdatas += r; // �o�b�t�@���̗L���ȃf�[�^�����X�V = (bufwpos-bufpos)&CS4231_BUFMASK
			}

			// �܂��f�[�^�����肻���Ȃ�NEVENT���Z�b�g
			if ((dmach->leng.w) && (cs4231cfg.rate)) {
				cnt = pccore.realclock * 32 / cs4231cfg.rate;
				nevent_set(NEVENT_CS4231, cnt, cs4231_dma, NEVENT_RELATIVE);
			}
		}

	}
	(void)item;
}

// PIO�Đ��p
void cs4231_datasend(REG8 dat) {
	UINT	pos;
	if (cs4231.reg.iface & PPIO) {		// PIO play enable
		if (cs4231.bufsize <= cs4231.bufdatas) {
			sound_sync();
		}
		if (cs4231.bufsize > cs4231.bufdatas) {
			pos = (cs4231.bufwpos) & CS4231_BUFMASK;
			cs4231.buffer[pos] = dat;
			cs4231.bufdatas++;
			cs4231.bufwpos = (cs4231.bufwpos + 1) & CS4231_BUFMASK;
		}
	}
}

// DMA�Đ��J�n�E�I���E���f���ɌĂ΂��i����j
REG8 DMACCALL cs4231dmafunc(REG8 func) {
	SINT32	cnt;
	switch(func) {
		case DMAEXT_START:
			if (cs4231cfg.rate) {
				//DMACH	dmach;
				//dmach = dmac.dmach + cs4231.dmach;
				//dmach->adrs.d = dmach->startaddr;
				//cs4231.bufpos = cs4231.bufwpos;
				//cs4231.bufdatas = 0;
				cs4231_totalsample = 0;
				//if ((cs4231.reg.pinctrl & IEN) && (cs4231.dmairq != 0xff)) {
				//	cs4231.intflag |= INt;
				//	cs4231.reg.featurestatus |= PI;
				//	pic_setirq(cs4231.dmairq);
				//}
				cnt = pccore.realclock * 32 / cs4231cfg.rate;
				nevent_set(NEVENT_CS4231, cnt, cs4231_dma, NEVENT_ABSOLUTE);
			}
			break;
		case DMAEXT_END:
			// �����ł̊��荞�݂͗v��Ȃ��H
			//if ((cs4231.reg.pinctrl & IEN) && (cs4231.dmairq != 0xff)) {
			//	cs4231.intflag |= INt;
			//	cs4231.reg.featurestatus |= PI;
			//	pic_setirq(cs4231.dmairq);
			//}
			break;

		case DMAEXT_BREAK:
			//{
			//	DMACH	dmach;
			//	dmach = dmac.dmach + cs4231.dmach;
			//	dmach->adrs.d = dmach->startaddr;
			//}
			nevent_reset(NEVENT_CS4231);
			break;

	}
	return(0);
}

void cs4231_reset(void) {

	ZeroMemory(&cs4231, sizeof(cs4231));
	cs4231.bufsize = CS4231_BUFFERS;
//	cs4231.proc = cs4231_nodecode;
	cs4231.dmach = 0xff;
	cs4231.dmairq = 0xff;
	//cs4231.timer = 200; // XXX: ��������Ă���Ȃ��̂ł�����ۂ��̂����饥�(10usec�P��)
	cs4231_totalsample = 0;
	FillMemory(cs4231.port, sizeof(cs4231.port), 0xff);
}

void cs4231_update(void) {
}

// �o�b�t�@�ʒu�̃Y���C���p�i�G�����h�~�j
static void setdataalign(void) {

	UINT	step;
	
	// �o�b�t�@�ʒu���Y���Ă�����C���i4byte�P�ʂɁj
	step = (0 - cs4231.bufpos) & 3;
	if (step) {
		cs4231.bufpos += step;
		cs4231.bufdatas -= min(step, cs4231.bufdatas);
	}
	cs4231.bufdatas &= ~3;
	step = (0 - cs4231.bufwpos) & 3;
	if (step) {
		cs4231.bufwpos += step;
	}
}

// CS4231 Indexed Data register�̏���
void cs4231_control(UINT idx, REG8 dat) {
	UINT8	modify;
	DMACH	dmach;
	switch(idx){
	case 0xd:
		break;
	case 0xc:
		dat &= 0x40;
		dat |= 0x8a;
		break;
	case 0xb://ErrorStatus 
	case 0x19://Version ID
		return;
	default:
		break;

	}
	dmach = dmac.dmach + cs4231.dmach;
	modify = ((UINT8 *)&cs4231.reg)[idx] ^ dat;
	((UINT8 *)&cs4231.reg)[idx] = dat;
	switch(idx) {
	case CS4231REG_PLAYFMT:
		// �Đ��t�H�[�}�b�g�ݒ�Ƃ��@Fs and Playback Data Format (I8)
		if (modify & 0xf0) {
			//dmach->adrs.d = dmach->startaddr;
			cs4231.bufpos = cs4231.bufwpos;
			cs4231.bufdatas = 0;
			setdataalign();
		}
		if (cs4231cfg.rate) {
			UINT32 r;
			r = cs4231xtal64[dat & 1] / cs4231cnt64[(dat >> 1) & 7];
			TRACEOUT(("samprate = %d", r));
			r <<= 12;
			r /= cs4231cfg.rate;
			cs4231.step12 = r;
			TRACEOUT(("step12 = %d", r));
		}
		else {
			cs4231.step12 = 0;
		}
		break;
	case CS4231REG_INTERFACE:
		// �Đ��^���̗L�������Ƃ�DMA�Ƃ��̐ݒ�@Interface Configuration (I9)
		if (modify & PEN ) {
			if (cs4231.dmach != 0xff) {
				dmach = dmac.dmach + cs4231.dmach;
				if ((dat & (PEN)) == (PEN)){
					dmach->ready = 1;
				}
				else {
					dmach->ready = 0;
				}
				dmac_check();
			}	
			if (!(dat & PEN)) {		// stop!
				cs4231.pos12 = 0; 
			}
		}
		break;
	case CS4231REG_IRQSTAT:
		// �o�b�t�@�I�[�o�[�����E�A���_�[�����⊄�荞�݂����o���邽�߂̃��W�X�^�H�@Alternate Feature Status (I24)
		if (modify & PI) {
			/* XXX: TI CI */
			pic_resetirq (cs4231.dmairq);
			cs4231.intflag &= ~INt;
		}
        break;
	case CS4231REG_PLAYCNTM:
		// Playback Upper Base (I14)
		// cs4231.reg.playcount[0]
        break;
	case CS4231REG_PLAYCNTL:
		// Playback Lower Base (I15)
		// cs4231.reg.playcount[1]
        break;
	//case CS4231REG_TIMERL:
	//	// CS4231 ���荞�݃^�C�}�[�ݒ�(���ʃo�C�g)
	//	cs4231.reg.timer[0]
	//	break;
	//case CS4231REG_TIMERH:
	//	// CS4231 ���荞�݃^�C�}�[�ݒ�(��ʃo�C�g)
	//	cs4231.reg.timer[1] 
	//	break;
	}
}

// �e�t�H�[�}�b�g�̊��荞�݊Ԋu�e�[�u���i���Ԃ�1�T���v��������̃o�C�g���j
//static const SINT32 cs4231_irqsamples[16] = {
//			1  ,		// 0: 8bit PCM
//			1*2,
//			1  ,		// 1: u-Law
//			1  ,
//			1*2,		// 2: 16bit PCM(little endian)?
//			1*4,
//			1  ,		// 3: A-Law
//			1  ,
//			1  ,		// 4:
//			1  ,
//			1  ,		// 5: ADPCM
//			1  ,
//			1*2,		// 6: 16bit PCM
//			1*4,
//			1  ,		// 7: ADPCM
//			1  };


static const SINT32 cs4231_playcountshift[16] = {
			1  ,		// 0: 8bit PCM
			1*2,
			1  ,		// 1: u-Law
			1  ,
			1*2,		// 2: 16bit PCM(little endian)?
			1*4,
			1  ,		// 3: A-Law
			1  ,
			1  ,		// 4:
			1  ,
			1  ,		// 5: ADPCM
			1  ,
			1*2,		// 6: 16bit PCM
			1*4,
			1  ,		// 7: ADPCM
			1  };


// CS4231 DMA�f�[�^�ǂݎ��
UINT dmac_getdata_(DMACH dmach, UINT8 *buf, UINT offset, UINT size) {
	UINT	leng; // �ǂݎ�萔
	UINT	lengsum; // ���v�ǂݎ�萔
	UINT32	addr;
	UINT	i;
	SINT32 sampleirq = 0; // ���荞�݂܂łɕK�v�ȃf�[�^�]����(byte)
	
	lengsum = 0;
	while(size > 0) {
		leng = min(dmach->leng.w, size);
		if (leng) {
			int playcount = (cs4231.reg.playcount[1]|(cs4231.reg.playcount[0] << 8)) * cs4231_playcountshift[cs4231.reg.datafmt >> 4];
			if(cs4231_totalsample + leng > playcount){
				leng = playcount - cs4231_totalsample;
			}
			addr = dmach->adrs.d; // ���݂̃������ǂݎ��ʒu
			if (!(dmach->mode & 0x20)) {			// dir +
				// +������DMA�]��
				for (i=0; i<leng ; i++) {
					buf[offset] = MEMP_READ8(addr);
					addr++;
					if(addr > dmach->lastaddr){
						addr = dmach->startaddr;
					}
					offset = (offset+1) & CS4231_BUFMASK;
				}
				dmach->adrs.d = addr;
			}
			else {									// dir -
				// -������DMA�]��
				for (i=0; i<leng; i++) {
					buf[offset] = MEMP_READ8(addr);
					addr--;
					if(addr < dmach->startaddr){
						addr = dmach->lastaddr;
					}
					offset = (offset-1) & CS4231_BUFMASK;
				}
				dmach->adrs.d = addr;
			}
			dmach->leng.w -= leng;

			if (dmach->leng.w <= 0) {
				dmach->leng.w = dmach->startcount; // �߂�
				dmach->proc.extproc(DMAEXT_END);
			}

			// �ǂݎ�萔�Ǝc�萔�X�V
			lengsum += leng;
			size -= leng;
			
			// Playback Count�����f�[�^��]�������犄�荞�݂𔭐�������
			if ((cs4231.reg.pinctrl & IEN) && (cs4231.dmairq != 0xff)) {
				// �ǂݎ�萔�J�E���g
				cs4231_totalsample += leng;
			
				if(cs4231_totalsample >= playcount){
					cs4231_totalsample -= playcount;
					cs4231.intflag |= INt;
					cs4231.reg.featurestatus |= PI;
					pic_setirq(cs4231.dmairq);
					break;
				}
			}
			//// XXX: ���o�C�g���ǂޖ��Ɋ��荞�݂���i���܂荪���̂Ȃ���������j
			//sampleirq = cs4231_irqsamples[cs4231.reg.datafmt >> 4] * ((cs4231.step12*cs4231cfg.rate)>>12) / 32; // * 1100/1000; //XXX: ���荞�݊Ԋu������
			//if(cs4231_totalsample >= sampleirq){
			//	if ((cs4231.reg.pinctrl & 2) && (cs4231.dmairq != 0xff)) {
			//		//cs4231.intflag |= INt;
			//		//cs4231.reg.featurestatus |= PI;
			//		//pic_setirq(cs4231.dmairq);
			//	}
			//	cs4231_totalsample -= sampleirq;
			//}
		}else{
			break;
		}
	}

	return(lengsum);
}

