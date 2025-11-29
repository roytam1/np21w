/**
 * @file	ct1741c.c
 * @brief	Implementation of the Creative SoundBlaster16 CT1741 DSP
 */

#ifdef SUPPORT_SOUND_SB16

#include "compiler.h"
#include "ct1741.h"
#include "iocore.h"
#include "fmboard.h"
#include "dmac.h"
#include "cpucore.h"

#if defined(SUPPORT_MULTITHREAD)
static int ct1741_cs_initialized = 0;
static CRITICAL_SECTION ct1741_cs;

void ct1741cs_enter_criticalsection(void)
{
	if (!ct1741_cs_initialized) return;
	EnterCriticalSection(&ct1741_cs);
}
void ct1741cs_leave_criticalsection(void)
{
	if (!ct1741_cs_initialized) return;
	LeaveCriticalSection(&ct1741_cs);
}

void ct1741cs_initialize(void)
{
	/* クリティカルセクション準備 */
	if (!ct1741_cs_initialized)
	{
		memset(&ct1741_cs, 0, sizeof(ct1741_cs));
		InitializeCriticalSection(&ct1741_cs);
		ct1741_cs_initialized = 1;
	}
}
void ct1741cs_shutdown(void)
{
	/* クリティカルセクション破棄 */
	if (ct1741_cs_initialized)
	{
		memset(&ct1741_cs, 0, sizeof(ct1741_cs));
		DeleteCriticalSection(&ct1741_cs);
		ct1741_cs_initialized = 0;
	}
}
#endif

void ct1741_initialize(UINT rate) {
	g_sb16.dsp_info.dma.rate2 = ct1741_playinfo.playrate = rate;
}

void ct1741_setpicirq(UINT8 irq) {
	pic_setirq(irq);
}
void ct1741_resetpicirq(UINT8 irq) {
	pic_resetirq(irq);
}

void ct1741_set_dma_irq(UINT8 irq) {
	switch (irq) {
	case 1:
		g_sb16.dsp_info.dmairq = irq;
		g_sb16.dmairq = 3;
		break;
	case 8:
		g_sb16.dsp_info.dmairq = irq;
		g_sb16.dmairq = 5;
		break;
	case 2:
		g_sb16.dsp_info.dmairq = irq;
		g_sb16.dmairq = 10;
		break;
	case 4:
		g_sb16.dsp_info.dmairq = irq;
		g_sb16.dmairq = 12;
		break;
	}
}

UINT8 ct1741_get_dma_irq() {
	switch (g_sb16.dmairq) {
	case  3:
		return 1;
	case  5:
		return 8;
	case 10:
		return 2;
	case 12:
		return 4;
	}
	return 0x00;
}

void ct1741_set_dma_ch(UINT8 dmach) {
	g_sb16.dsp_info.dmachnum = dmach;
	if (dmach & 0x21) {
		if (g_sb16.dmachnum != 0) {
			g_sb16.dmachnum = 0;
			dmac_attach(DMADEV_CT1741, g_sb16.dmachnum);
		}
	}
	if (dmach & 0x42) {
		if (g_sb16.dmachnum != 3) {
			g_sb16.dmachnum = 3;
			dmac_attach(DMADEV_CT1741, g_sb16.dmachnum);
		}
	}
}

UINT8 ct1741_get_dma_ch() {
	switch (g_sb16.dmachnum) {
	case 0:
		return (g_sb16.dsp_info.dmachnum & 0xe0) ? 0x21 : 0x01;
	case 3:
		return (g_sb16.dsp_info.dmachnum & 0xe0) ? 0x42 : 0x02;
	}
	return 0x00;
}

void ct1741_dma(NEVENTITEM item)
{
	UINT	r;
	SINT32	cnt;
	UINT	rem;
	UINT	pos;
	UINT	irqsamples;
	UINT	irqsamplesleft;
	UINT8	dmabuf[CT1741_DMA_BUFSIZE];
	int i;
	static int zerocounter = 0; // DMA転送が終了してからct1741_dmaが呼ばれた回数カウント
	int bytesPerSample; // 1サンプルあたりのバイト数

	if (item->flag & NEVENT_SETEVENT) {
		if (g_sb16.dmachnum != 0xff) {
			sound_sync();
			if (g_sb16.dsp_info.mode == CT1741_DSPMODE_DMA) {
				int isautoinit = g_sb16.dsp_info.dma.lastautoinit || g_sb16.dsp_info.dma.autoinit;
				g_sb16.dsp_info.wbusy = 1;

#if defined(SUPPORT_MULTITHREAD)
				ct1741cs_enter_criticalsection();
#endif
				// 1サンプルあたりのバイト数を計算
				bytesPerSample = CT1741_BUF_ALIGN[g_sb16.dsp_info.dma.mode | g_sb16.dsp_info.dma.stereo << 3];

				// 転送可能サイズを計算
				if (g_sb16.dsp_info.dma.bufsize < g_sb16.dsp_info.dma.bufdatas) {
					rem = 0;
				}
				else {
					rem = g_sb16.dsp_info.dma.bufsize - g_sb16.dsp_info.dma.bufdatas;
				}
				//rem = rem & ~(bytesPerSample - 1);

				// DMA転送
				r = dmac_getdatas(g_sb16.dsp_info.dma.dmach, dmabuf, rem);
				if (r != 0) {
					// 1byteでも転送できたらゼロカウンタクリア
					zerocounter = 0;
				}

				// 再生用バッファへコピー　なぜかバッファをはみ出して送られてくることがあるようなのでg_sb16.dsp_info.dma.dmach->startcountの範囲内しか転送しない（範囲外は捨てる）
				pos = (g_sb16.dsp_info.dma.bufpos + g_sb16.dsp_info.dma.bufdatas) & (CT1741_DMA_BUFSIZE - 1);
				for (i = 0; i < r; i++) {
					int dstpos = (pos + i) & (CT1741_DMA_BUFSIZE - 1);
					if (g_sb16.dsp_info.smpcounter2 < (g_sb16.dsp_info.dma.dmach->startcount & ~(bytesPerSample - 1))) {
						g_sb16.dsp_info.dma.buffer[dstpos] = dmabuf[i];
						g_sb16.dsp_info.smpcounter2++;
						g_sb16.dsp_info.dma.bufdatas++;
					}
				}

				// 再生開始をディレイさせる（バッファが空になるのを防止）
				if (ct1741_playinfo.playwaitcounter > 0) {
					ct1741_playinfo.playwaitcounter -= r;
					if (ct1741_playinfo.playwaitcounter < 0) {
						ct1741_playinfo.playwaitcounter = 0;
					}
				}
#if defined(SUPPORT_MULTITHREAD)
				ct1741cs_leave_criticalsection();
#endif

				// autoinitの時一定のデータ量を転送したら割り込みを発生させる　
				irqsamples = (int)g_sb16.dsp_info.dma.total * (g_sb16.dsp_info.dma.last16mode ? 2 : 1);
				g_sb16.dsp_info.smpcounter += r;
				if (g_sb16.dsp_info.smpcounter >= irqsamples) {
					if (isautoinit) {
						// autoinitならカウンタを戻して割り込み、そうでないならDMA転送完了処理で割り込み
						g_sb16.dsp_info.smpcounter -= irqsamples;
						g_sb16.mixreg[0x82] |= (g_sb16.dsp_info.dma.last16mode ? 2 : 1);
						ct1741_setpicirq(g_sb16.dmairq);
						g_sb16.dsp_info.wbusy = 0;
					}
				}
				if (g_sb16.dsp_info.smpcounter < irqsamples) {
					irqsamplesleft = bytesPerSample + irqsamples - g_sb16.dsp_info.smpcounter; // 次の割り込みを発生させるデータ数残り　1サンプル分はマージンを付ける
				}
				else {
					irqsamplesleft = bytesPerSample; // 逆転していたら1サンプル分とする
				}

				if ((g_sb16.dsp_info.dma.dmach->leng.w) && (g_sb16.dsp_info.freq)) {
					// まだデータがあるので再度イベント設定 
					cnt = pccore.realclock / g_sb16.dsp_info.freq / bytesPerSample * min(min(g_sb16.dsp_info.dma.dmach->startcount, g_sb16.dsp_info.dma.bufdatas) / 4, irqsamplesleft); // バッファの1/4を消費するクロック数 or 次の割り込みタイミング
					if (cnt != 0) {
						nevent_set(NEVENT_CT1741, cnt, ct1741_dma, NEVENT_RELATIVE);
					}
					else {
						nevent_setbyms(NEVENT_CT1741, 1, ct1741_dma, NEVENT_RELATIVE); // neventを0でセットすると猫がフリーズするので回避（基本はここには来ないはず）
					}
				}
				else {
					// DMA転送終わった
					g_sb16.dsp_info.wbusy = 0;
					if (zerocounter == 0 && g_sb16.dsp_info.freq) {
						// 終わった直後。フラグを立てたり割り込みしたりする。
						if (isautoinit || g_sb16.dsp_info.smpcounter + 1 < irqsamples) {
							// autoinitまたはsingleで規定数転送してないならDMA転送を繰り返す
							g_sb16.dsp_info.dma.laststartaddr = g_sb16.dsp_info.dma.dmach->startaddr;
							g_sb16.dsp_info.dma.laststartcount = g_sb16.dsp_info.dma.dmach->startcount;
							if ((g_sb16.dsp_info.dmachnum & 0xe0) && g_sb16.dsp_info.dma.mode == CT1741_DMAMODE_16) {
								// なぞの1ビットずらし
								g_sb16.dsp_info.dma.dmach->adrs.d = (g_sb16.dsp_info.dma.dmach->startaddr & 0xffff0000) | ((g_sb16.dsp_info.dma.dmach->startaddr << 1) & 0xffff);
								g_sb16.dsp_info.dma.dmach->lastaddr = g_sb16.dsp_info.dma.dmach->adrs.d + g_sb16.dsp_info.dma.dmach->startcount;
								g_sb16.dsp_info.dma.last16mode = 1; // 16bit転送モード
							}
							else {
								g_sb16.dsp_info.dma.dmach->adrs.d = g_sb16.dsp_info.dma.dmach->startaddr; // 戻す
							}
							g_sb16.dsp_info.dma.dmach->leng.w = g_sb16.dsp_info.dma.dmach->startcount; // 戻す
							g_sb16.dsp_info.smpcounter2 = 0;
							if (isautoinit && g_sb16.dsp_info.smpcounter >= irqsamples - 16) {
								// 若干誤差があるが割り込みをDMA周期に合わせるのを優先する
								g_sb16.dsp_info.smpcounter = 0;
								g_sb16.mixreg[0x82] |= (g_sb16.dsp_info.dma.last16mode ? 2 : 1);
								ct1741_setpicirq(g_sb16.dmairq);
								g_sb16.dsp_info.wbusy = 0;
							}

							// 再度イベント設定
							cnt = pccore.realclock / g_sb16.dsp_info.freq / bytesPerSample * min(min(g_sb16.dsp_info.dma.dmach->startcount, g_sb16.dsp_info.dma.bufdatas) / 16, irqsamplesleft); // バッファの1/16を消費するクロック数
							if (cnt != 0) {
								nevent_set(NEVENT_CT1741, cnt, ct1741_dma, NEVENT_RELATIVE);
							}
							else {
								nevent_setbyms(NEVENT_CT1741, 1, ct1741_dma, NEVENT_RELATIVE);
							}
						}
						else {
							// singleなら割り込みを送出してDMA転送終了
							g_sb16.dsp_info.wbusy = 0;
							g_sb16.mixreg[0x82] |= (g_sb16.dsp_info.dma.last16mode ? 2 : 1);
							ct1741_setpicirq(g_sb16.dmairq);
							if (g_sb16.dmachnum != 0xff) {
								dmac.stat |= (1 << g_sb16.dmachnum);
							}
							g_sb16.dsp_info.smpcounter2 = 0;
							g_sb16.dsp_info.smpcounter = 0;
							g_sb16.dsp_info.dma.dmach->ready = 0;
						}
					}
					else {
						// 無反応なら再送。終わった直後に出した割り込みがスルーされる場合があるので無理矢理
						if ((zerocounter % 8) == 7 || zerocounter > 32) {
							if (g_sb16.dsp_info.dma.bufdatas < CT1741_DMA_BUFSIZE / 4) {
								g_sb16.mixreg[0x82] |= (g_sb16.dsp_info.dma.last16mode ? 2 : 1);
								ct1741_setpicirq(g_sb16.dmairq);
								if (g_sb16.dmachnum != 0xff) {
									dmac.stat |= (1 << g_sb16.dmachnum);
								}
							}
						}
						//// それでも無反応ならごり押し
						//if (zerocounter > 32) {
						//	if (isautoinit) {
						//		g_sb16.dsp_info.dma.dmach->leng.w = g_sb16.dsp_info.dma.dmach->startcount; // 戻す
						//		g_sb16.dsp_info.dma.dmach->adrs.d = g_sb16.dsp_info.dma.dmach->startaddr; // 戻す
						//		g_sb16.dsp_info.smpcounter2 = 0;
						//	}
						//	else {
						//		g_sb16.dsp_info.smpcounter2 = 0;
						//		return;
						//	}
						//}
						cnt = pccore.realclock / g_sb16.dsp_info.freq / bytesPerSample * CT1741_DMA_BUFSIZE / 16; // 割り込みをだすクロック数
						if (cnt != 0) {
							nevent_set(NEVENT_CT1741, cnt, ct1741_dma, NEVENT_RELATIVE);
						}
						else {
							nevent_setbyms(NEVENT_CT1741, 1, ct1741_dma, NEVENT_RELATIVE);
						}
					}
					if (r == 0) {
						zerocounter++;
					}
				}
			}
			else {
				// DMA転送終了
				g_sb16.dsp_info.wbusy = 0;
				g_sb16.mixreg[0x82] |= (g_sb16.dsp_info.dma.last16mode ? 2 : 1);
				ct1741_setpicirq(g_sb16.dmairq);
				g_sb16.dsp_info.smpcounter2 = 0;
				g_sb16.dsp_info.smpcounter = 0;
				g_sb16.dsp_info.dma.dmach->ready = 0;
				if (g_sb16.dmachnum != 0xff) {
					dmac.stat |= (1 << g_sb16.dmachnum);
				}
			}
		}
	}

}

void ct1741_startdma()
{
#if defined(SUPPORT_MULTITHREAD)
	ct1741cs_enter_criticalsection();
#endif
	g_sb16.dsp_info.dma.laststartaddr = g_sb16.dsp_info.dma.dmach->startaddr;
	g_sb16.dsp_info.dma.laststartcount = g_sb16.dsp_info.dma.dmach->startcount;
	if ((g_sb16.dsp_info.dmachnum & 0xe0) && (g_sb16.dsp_info.dma.mode == CT1741_DMAMODE_16))
	{
		// 16bit DMA なぞの1ビットずらし
		g_sb16.dsp_info.dma.dmach->startcount *= 2; // データ数も2倍
		g_sb16.dsp_info.dma.dmach->adrs.d = (g_sb16.dsp_info.dma.dmach->startaddr & 0xffff0000) | ((g_sb16.dsp_info.dma.dmach->startaddr << 1) & 0xffff);
		g_sb16.dsp_info.dma.dmach->lastaddr = g_sb16.dsp_info.dma.dmach->adrs.d + g_sb16.dsp_info.dma.dmach->startcount;
		g_sb16.dsp_info.dma.dmach->leng.w = g_sb16.dsp_info.dma.dmach->startcount; // 戻す
	}
	else {
		// 8bit DMA
		g_sb16.dsp_info.dma.dmach->adrs.d = g_sb16.dsp_info.dma.dmach->startaddr;
		g_sb16.dsp_info.dma.dmach->lastaddr = g_sb16.dsp_info.dma.dmach->adrs.d + g_sb16.dsp_info.dma.dmach->startcount;
		g_sb16.dsp_info.dma.dmach->leng.w = g_sb16.dsp_info.dma.dmach->startcount; // 戻す
	}
	g_sb16.dsp_info.dma.last16mode = (g_sb16.dsp_info.dma.mode == CT1741_DMAMODE_16) ? 1 : 0; // 16bit転送モードフラグ
	g_sb16.mixreg[0x82] &= ~3;
	ct1741_resetpicirq(g_sb16.dmairq);
	g_sb16.dsp_info.wbusy = 0;
	g_sb16.dsp_info.smpcounter = 0;// g_sb16.dsp_info.dma.total / 2;
	g_sb16.dsp_info.smpcounter2 = 0;
	g_sb16.dsp_info.dma.bufdatas = 0;
	g_sb16.dsp_info.dma.bufpos = 0;
	ct1741_playinfo.bufdatasrem = 0;
	ct1741_playinfo.playwaitcounter = CT1741_DMA_BUFSIZE * CT1741_BUF_ALIGN[g_sb16.dsp_info.dma.mode | g_sb16.dsp_info.dma.stereo << 3] * g_sb16.dsp_info.freq / 44100 / 16;
#if defined(SUPPORT_MULTITHREAD)
	ct1741cs_leave_criticalsection();
#endif
}

REG8 DMACCALL ct1741dmafunc(REG8 func)
{
	SINT32	cnt;
	int bytesPerSample; // 1サンプルあたりのバイト数

	switch (func) {
	case DMAEXT_START:
		// DMA転送開始
		TRACEOUT(("DMAEXT_START DMA_MODE=%d", g_sb16.dsp_info.dma.mode));
		ct1741_startdma();
		bytesPerSample = CT1741_BUF_ALIGN[g_sb16.dsp_info.dma.mode | g_sb16.dsp_info.dma.stereo << 3];
		cnt = pccore.realclock / g_sb16.dsp_info.freq / bytesPerSample * CT1741_DMA_BUFSIZE; // バッファを消費するクロック数だけ待つ　短すぎるとノイズが入る
		if (cnt != 0) {
			nevent_set(NEVENT_CT1741, cnt, ct1741_dma, NEVENT_RELATIVE);
		}
		else {
			nevent_setbyms(NEVENT_CT1741, 1, ct1741_dma, NEVENT_RELATIVE);
		}
		break;

	case DMAEXT_END:
		// DMA転送完了。本当はここで割り込むのでは？と思いましたがここで割り込むと何だかおかしくなります。謎
		break;

	case DMAEXT_BREAK:
		// DMA転送中断
		nevent_reset(NEVENT_CT1741);
		g_sb16.mixreg[0x82] &= ~3;
		ct1741_resetpicirq(g_sb16.dmairq);
		g_sb16.dsp_info.wbusy = 0;
		if (g_sb16.dmachnum != 0xff) {
			dmac.stat |= (1 << g_sb16.dmachnum);
		}
		if (g_sb16.dsp_info.dma.last16mode) {
			g_sb16.dsp_info.dma.last16mode = 0;
			g_sb16.dsp_info.dma.dmach->startaddr = g_sb16.dsp_info.dma.laststartaddr;
			g_sb16.dsp_info.dma.dmach->startcount = g_sb16.dsp_info.dma.laststartcount;
		}

		break;
	}
	return(0);
}

#endif
