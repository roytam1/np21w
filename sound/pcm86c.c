/**
 * @file	pcm86c.c
 * @brief	Implementation of the 86-PCM
 */

#include "compiler.h"
#include "pcm86.h"
#include "pccore.h"
#include "cpucore.h"
#include "iocore.h"
#include "fmboard.h"

#if 1
#undef	TRACEOUT
#define	TRACEOUT(s)	(void)(s)
static void trace_fmt_ex(const char *fmt, ...)
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

/* �T���v�����O���[�g��8�|������ */
const UINT pcm86rate8[] = {352800, 264600, 176400, 132300,
							88200,  66150,  44010,  33075};

/* 32,24,16,12, 8, 6, 4, 3 - �ŏ����{��: 96 */
/*  3, 4, 6, 8,12,16,24,32 */

static const UINT clk25_128[] = {
					0x00001bde, 0x00002527, 0x000037bb, 0x00004a4e,
					0x00006f75, 0x0000949c, 0x0000df5f, 0x00012938};
static const UINT clk20_128[] = {
					0x000016a4, 0x00001e30, 0x00002d48, 0x00003c60,
					0x00005a8f, 0x000078bf, 0x0000b57d, 0x0000f17d};


	PCM86CFG	pcm86cfg;

	static int bufunferflag = 0;

void pcm86gen_initialize(UINT rate)
{
	pcm86cfg.rate = rate;
}

void pcm86gen_setvol(UINT vol)
{
	pcm86cfg.vol = vol;
	pcm86gen_update();
}

void pcm86_reset(void)
{
	PCM86 pcm86 = &g_pcm86;

	memset(pcm86, 0, sizeof(*pcm86));
	pcm86->fifosize = 0x80;
	pcm86->dactrl = 0x32;
	pcm86->stepmask = (1 << 2) - 1;
	pcm86->stepbit = 2;
	pcm86->stepclock = ((UINT64)pccore.baseclock << 6);
	pcm86->stepclock /= 44100;
	pcm86->stepclock *= pccore.multiple;
	pcm86->rescue = (PCM86_RESCUE * 32) << 2;
	pcm86->irq = 0xff;	
	pcm86_setpcmrate(pcm86->fifo); // �f�t�H���g�l���Z�b�g
}

void pcm86gen_update(void)
{
	PCM86 pcm86 = &g_pcm86;

	pcm86->volume = pcm86cfg.vol * pcm86->vol5;
	pcm86_setpcmrate(pcm86->fifo);
}

void pcm86_setpcmrate(REG8 val)
{
	PCM86 pcm86 = &g_pcm86;
	SINT32	rate;

	pcm86->rateval = rate = pcm86rate8[val & 7];
	pcm86->stepclock = ((UINT64)pccore.baseclock << 6);
	pcm86->stepclock /= rate;
	pcm86->stepclock *= (pccore.multiple << 3);
	if (pcm86cfg.rate)
	{
		pcm86->div = (rate << (PCM86_DIVBIT - 3)) / pcm86cfg.rate;
		pcm86->div2 = (pcm86cfg.rate << (PCM86_DIVBIT + 3)) / rate;
	}
}

void pcm86_cb(NEVENTITEM item)
{
	PCM86 pcm86 = &g_pcm86;
	SINT32 adjustbuf;
	
	if (pcm86->reqirq)
	{
		sound_sync();
//		RECALC_NOWCLKP;

		adjustbuf = (SINT32)(((SINT64)pcm86->virbuf * 9 + pcm86->realbuf) / 10);
		if (pcm86->virbuf <= pcm86->fifosize || pcm86->realbuf > pcm86->stepmask && adjustbuf <= pcm86->fifosize)
		{
			pcm86->reqirq = 0;
			pcm86->irqflag = 1;
			if (pcm86->irq != 0xff)
			{
				pic_setirq(pcm86->irq);
			}
		}
		else
		{
			pcm86_setnextintr();
		}
	}
	else
	{
		pcm86_setnextintr();
	}

	(void)item;
}

void pcm86_setnextintr(void) {

	PCM86 pcm86 = &g_pcm86;
	SINT32	cntv;
	SINT32	cntr;
	SINT32	cnt;
	SINT32	clk;

	if (pcm86->fifo & 0x80)
	{
		cntv = pcm86->virbuf - pcm86->fifosize;
		cntr = pcm86->realbuf - pcm86->fifosize;
		if (cntr < cntv && pcm86->realbuf > pcm86->stepmask)
		{
			//cnt = cntr;
			if (bufunferflag > 64)
			{
				cnt = (SINT32)(((SINT64)cntv * 9 + cntr) / 10);
				//TRACEOUT(("Buf Under", bufunferflag));
			}
			else
			{
				cnt = (SINT32)(((SINT64)cntv * 99 + cntr) / 100);
			}
		}
		else
		{
			cnt = cntv;
		}
		//cnt = (SINT32)(((SINT64)cntv + cntr) / 2);
		if (cnt > 0)
		{
			cnt += pcm86->stepmask;
			cnt >>= pcm86->stepbit;
			cnt += 4;
			/* ������ clk = pccore.realclock * cnt / 86pcm_rate */
			/* clk = ((pccore.baseclock / 86pcm_rate) * cnt) * pccore.multiple */
			if (pccore.cpumode & CPUMODE_8MHZ) {
				clk = clk20_128[pcm86->fifo & 7];
			}
			else {
				clk = clk25_128[pcm86->fifo & 7];
			}
			/* cnt�͍ő� 8000h �� 32bit�Ŏ��܂�悤�Ɂc */
			clk *= cnt;
			clk >>= 7;
//			clk++;						/* roundup */
			//clk--;						/* roundup */
			//if (clk > 1) clk--;
			clk *= pccore.multiple;
			nevent_set(NEVENT_86PCM, clk, pcm86_cb, NEVENT_ABSOLUTE);
			//TRACEOUT(("%d,%d", pcm86->virbuf, pcm86->realbuf));
		}
		else
		{
			if (pcm86->reqirq)
			{
				// ����
				nevent_set(NEVENT_86PCM, 1, pcm86_cb, NEVENT_ABSOLUTE);
			}
			else
			{
				// WORKAROUND: �O��̊��荞�݂����܂������Ă��Ȃ��B�K���Ȏ��ԊԊu�ōēx���荞�݂𑗂�B���������NT4�������荞�ݖ������[�v����̂Œ���
				pcm86->reqirq = 1;
				nevent_set(NEVENT_86PCM, 100 * pccore.multiple, pcm86_cb, NEVENT_ABSOLUTE);
			}
		}
	}
}

void RECALC_NOWCLKWAIT(UINT64 cnt)
{
	SINT64 decvalue = (SINT64)(cnt << g_pcm86.stepbit);
	if (g_pcm86.virbuf - decvalue < g_pcm86.virbuf)
	{
		g_pcm86.virbuf -= decvalue;
	}
	if (g_pcm86.virbuf < 0)
	{
		g_pcm86.virbuf &= g_pcm86.stepmask;
	}
}

void pcm86_changeclock(UINT oldmultiple)
{
	PCM86 pcm86 = &g_pcm86;
	if(pcm86){
		if(pcm86->rateval){
			UINT64	cur;
			UINT64	past;
			UINT64  pastCycle;
			UINT64	newstepclock;
			newstepclock = ((UINT64)pccore.baseclock << 6);
			newstepclock /= pcm86->rateval;
			newstepclock *= ((UINT64)pccore.multiple << 3);
			pastCycle = (UINT64)UINT_MAX << 6;
			cur = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
			cur <<= 6;
			past = (cur + pastCycle - pcm86->lastclock) % pastCycle;
			if (past > pastCycle / 2)
			{
				// ���̒l�ɂȂ��Ă��܂��Ă���Ƃ�
				if (past < pastCycle - pcm86->stepclock * 4)
				{
					// ���Ȃ菬�����Ȃ烊�Z�b�g��������
					past = 1;
					pcm86->lastclock = cur - 1;
				}
				else
				{
					// �������Ȃ�l�q����0�����Ƃ���
					past = 0;
				}
			}
			if (past >= pcm86->stepclock)
			{
				//SINT32 latvirbuf = pcm86->virbuf;
				past = past / pcm86->stepclock;
				pcm86->lastclock = (pcm86->lastclock + past * pcm86->stepclock) % pastCycle;
				RECALC_NOWCLKWAIT(past);
				//TRACEOUT(("%d %d %d", latvirbuf, pcm86->virbuf, past));
			}
			past = (cur + pastCycle - pcm86->lastclock) % pastCycle;
			pcm86->lastclock = (cur + pastCycle - past * (newstepclock + pcm86->stepclock / 2) / pcm86->stepclock) % pastCycle; // �␳
			pcm86->stepclock = newstepclock;
			pcm86_setnextintr();
			//TRACEOUT(("changed"));
		}else{
			//pcm86->stepclock = ((UINT64)pccore.baseclock << 6);
			//pcm86->stepclock /= 44100;
			//pcm86->stepclock *= pccore.multiple;
		}
	}
}

void SOUNDCALL pcm86gen_checkbuf(PCM86 pcm86, UINT nCount)
{
	long	bufs;
	UINT64	cur;
	UINT64	past;
	UINT64  pastCycle;

	pastCycle = (UINT64)UINT_MAX << 6;
	cur = CPU_CLOCK + CPU_BASECLOCK - CPU_REMCLOCK;
	cur <<= 6;
	past = (cur + pastCycle - pcm86->lastclock) % pastCycle;
	if (past > pastCycle / 2)
	{
		// ���̒l�ɂȂ��Ă��܂��Ă���Ƃ�
		if (past < pastCycle - pcm86->stepclock * 4)
		{
			// ���Ȃ菬�����Ȃ烊�Z�b�g��������
			past = 1;
			pcm86->lastclock = cur - 1;
		}
		else
		{
			// �������Ȃ�l�q����0�����Ƃ���
			past = 0;
		}
	}
	if (past >= pcm86->stepclock)
	{
		past = past / pcm86->stepclock;
		pcm86->lastclock = (pcm86->lastclock + past * pcm86->stepclock) % pastCycle;
		RECALC_NOWCLKWAIT(past);
	}

	//pcm86->virbuf = pcm86->realbuf;
	bufs = pcm86->realbuf - pcm86->virbuf;
	if (bufs < 0)
	{
		// ���������Ă邩������Ȃ�
		if (bufs <= -pcm86->fifosize && pcm86->virbuf < pcm86->fifosize)
		{
			// �ʖڂ����ȏꍇ
			//TRACEOUT(("CRITCAL buf: real %d, vir %d, (FIFOSIZE: %d) FORCE IRQ", pcm86->realbuf, pcm86->virbuf, pcm86->fifosize));
			bufs &= ~3;
			pcm86->virbuf += bufs;
			// nevent_set�Ńf�b�h���b�N����\��������̂Ŋ��荞�݂͂��Ȃ�
		}
		//TRACEOUT(("buf: real %d, vir %d, (FIFOSIZE: %d) FORCE IRQ", pcm86->realbuf, pcm86->virbuf, pcm86->fifosize));
		//pcm86->lastclock += pcm86->stepclock / 50;
		if (bufunferflag < INT_MAX)
		{
			bufunferflag++;
			//TRACEOUT(("%d", bufunferflag));
		}
	}
	else
	{
		// �]�T����
		//bufs -= PCM86_EXTBUF;
		//if (bufs > 0)
		//{
		//	bufs &= ~3;
		//	pcm86->realbuf -= bufs;
		//	pcm86->readpos += bufs;
		//}

		//TRACEOUT(("%d,%d", pcm86->virbuf, pcm86->realbuf));
		if (pcm86->virbuf > pcm86->fifosize && pcm86->realbuf > pcm86->stepmask && pcm86->realbuf > pcm86->virbuf + pcm86->fifosize * 3)
		{
			pcm86->virbuf += 8;
			//TRACEOUT(("ADJUST!"));
		}
		bufunferflag = 0;
	}
}

BOOL pcm86gen_intrq(int fromFMTimer)
{
	PCM86 pcm86 = &g_pcm86;
	if (!(pcm86->fifo & 0x20))
	{
		return FALSE;
	}
	if (pcm86->irqflag)
	{
		return TRUE;
	}
	if (!nevent_iswork(NEVENT_86PCM)) {
		sound_sync();
		if (!(pcm86->irqflag) && (pcm86->virbuf <= pcm86->fifosize || pcm86->realbuf > pcm86->stepmask && pcm86->realbuf <= pcm86->fifosize))
		{
			//pcm86->reqirq = 0;
			pcm86->irqflag = 1;
			return TRUE;
		}
	}
	return FALSE;
}
