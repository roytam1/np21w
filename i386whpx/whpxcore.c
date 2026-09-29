/*
 * Copyright (c) 2026 SimK
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"timing.h"
#include	"dmax86.h"
#include	"bios/bios.h"
#include	"sound/soundrom.h"
#include	"vram/vram.h"
#include	"wab/cirrus_vga_extern.h"
#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_W2K)
#include	"wab/npdisp_w2k.h"
#endif

#if defined(SUPPORT_IA32_WHPX)

#if 0
#undef	TRACEOUT
//#define USE_TRACEOUT_VS
#ifdef USE_TRACEOUT_VS
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
#else
#define	TRACEOUT(s)	(void)(s)
#endif
#endif	/* 1 */
#include	"whpxfunc.h"
#include	"whpxcore.h"

#if defined(_WINDOWS)
#include	<process.h>
#endif

NP2_WHPX			np2whpx = {0};
NP2_WHPX_STAT	np2whpxstat = {0};
NP2_WHPX_CORE	np2whpxcore = {0};

// Windows Hypervisor Platform availability and lifetime
UINT8 i386whpx_check(void)
{
#if defined(_WIN32)
	np2whpx.available = (i386whpxfunc_is_available() == SUCCESS);
	if (!np2whpx.available) {
		msgbox("WHPX VM", "WHPX is not available.");
		np2whpx.enable = 0;
	}
	return np2whpx.available ? SUCCESS : FAILURE;
#else
	return FAILURE;
#endif
}

void i386whpx_initialize(void)
{
#if defined(_WIN32)
	if (!np2whpx.available || !np2whpx.enable) return;
	i386whpx_disposeVM();
#endif
}

void i386whpx_createVM(void)
{
#if defined(_WIN32)
	if (!np2whpx.available || !np2whpx.enable) return;
	i386whpx_disposeVM();
	if (i386whpxfunc_create_vm() == FAILURE) {
		TRACEOUT(("WHPX: failed to create partition or virtual processor."));
		msgbox("WHPX VM", "Failed to create the WHPX virtual machine.");
		np2whpx.enable = 0;
		return;
	}
	np2whpx.created = 1;
	np2whpx.bioshookenable = 0;
	memset(&np2whpxcore, 0, sizeof(np2whpxcore));
	if (i386whpxfunc_get_registers(&np2whpxstat.state) == FAILURE ||
		i386whpxfunc_get_fpu(&np2whpxstat.fpustate) == FAILURE) {
		i386whpx_disposeVM();
		np2whpx.enable = 0;
		return;
	}
	memcpy(&np2whpxstat.default_state, &np2whpxstat.state,
		   sizeof(np2whpxstat.state));
	memcpy(&np2whpxstat.default_fpustate, &np2whpxstat.fpustate,
		   sizeof(np2whpxstat.fpustate));
	np2whpx.emumode = 0;
#endif
}

void i386whpx_resetVMMem(void) {
	
	// メモリ周りの初期設定
	np2whpxcore.lastA20en = (CPU_ADRSMASK != 0x000fffff);
	i386whpx_vm_sethmemory(np2whpxcore.lastA20en);
	np2whpxcore.lastITFbank = CPU_ITFBANK;
	i386whpx_vm_setitfmemory(np2whpxcore.lastITFbank);
	np2whpxcore.lastVGA256linear = (vramop.mio2[0x2]==0x1);
	i386whpx_vm_setvga256linearmemory();
	np2whpxcore.lastVRAMMMIO = 1;
	
	i386whpx_vm_setmemoryarea(mem+0xA5000, 0xA5000, 0x3000);
	//i386whpx_vm_setmemoryarea(mem+0xA8000, 0xA8000, 0x8000);
	//i386whpx_vm_setmemoryarea(mem+0xB0000, 0xB0000, 0x10000);
	//i386whpx_vm_setmemoryarea(mem+0xC0000, 0xC0000, 0x10000);
	i386whpx_vm_setmemoryarea(mem+0xD0000, 0xD0000, 0x10000);
	//i386whpx_vm_setmemoryarea(mem+0xE0000, 0xE0000, 0x8000);
	i386whpx_vm_setmemoryarea(mem+0xE8000, 0xE8000, 0x8000);
	i386whpx_vm_setmemoryarea(mem+0xF0000, 0xF0000, 0x8000);
	i386whpx_vm_setmemoryarea(mem+0xF8000, 0xF8000, 0x8000);
#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_W2K)
	npdisp_w2k_whpxSync();
#endif
	
	//i386whpx_vm_setmemoryarea(mem+0xA5000, 0xA5000, 0x3000);
	//i386whpx_vm_setmemoryarea(mem+0xA8000, 0xA8000, 0x8000);
	//i386whpx_vm_setmemoryarea(mem+0xB0000, 0xB0000, 0x10000);
	//i386whpx_vm_setmemoryarea(mem+0xC0000, 0xC0000, 0x10000);
	//i386whpx_vm_setmemoryarea(mem+0xD0000, 0xD0000, 0x10000);
	//i386whpx_vm_setmemoryarea(mem+0xE0000, 0xE0000, 0x8000);
	//i386whpx_vm_setmemoryarea(mem+0xE8000, 0xE8000, 0x8000);
	//i386whpx_vm_setmemoryarea(mem+0xF0000, 0xF0000, 0x8000);
}

void i386whpx_resetVMCPU(void) {
	
	// CPU周りの初期設定
	np2whpxstat.irq_reqidx_cur = np2whpxstat.irq_reqidx_end = 0;
	
	memcpy(&np2whpxstat.state, &np2whpxstat.default_state, sizeof(np2whpxstat.state));
	memcpy(&np2whpxstat.fpustate, &np2whpxstat.default_fpustate, sizeof(np2whpxstat.fpustate));
	
	np2whpxstat.update_regs = np2whpxstat.update_fpu = 1;
	np2whpxstat.update_segment_regs = 1;
	
	np2whpxcore.clockpersec = GetTickCounter_ClockPerSec();
	np2whpxcore.lastclock = GetTickCounter_Clock();
	np2whpxcore.clockcount = GetTickCounter_Clock();
}

/*
 * bios call interface (WHPX)
 */
static int
ia32whpx_bioscall(void)
{
	UINT32 adrs;
	int ret = 0;

	if (!CPU_STAT_PM || CPU_STAT_VM86) {
#if 1
		adrs = CPU_PREV_EIP + (CPU_CS << 4);
#else
		adrs = CPU_PREV_EIP + CPU_STAT_CS_BASE;
#endif
#if defined(SUPPORT_EMU_SOUNDBIOS)
		if (soundrom_biosfunc(adrs)) {
			LOAD_SEGREG(CPU_ES_INDEX, CPU_ES);
			LOAD_SEGREG(CPU_CS_INDEX, CPU_CS);
			LOAD_SEGREG(CPU_SS_INDEX, CPU_SS);
			LOAD_SEGREG(CPU_DS_INDEX, CPU_DS);
			ret = 1;
		}
		else
#endif
		if ((adrs >= 0xf8000) && (adrs < 0x100000)) {
			biosfunc(adrs);
			LOAD_SEGREG(CPU_ES_INDEX, CPU_ES);
			LOAD_SEGREG(CPU_CS_INDEX, CPU_CS);
			LOAD_SEGREG(CPU_SS_INDEX, CPU_SS);
			LOAD_SEGREG(CPU_DS_INDEX, CPU_DS);
			ret = 1;
		}
	}
	return ret;
}

#define	TSS_16_SIZE	44
#define	TSS_16_LIMIT	(TSS_16_SIZE - 1)
#define	TSS_32_SIZE	104
#define	TSS_32_LIMIT	(TSS_32_SIZE - 1)

static void CPUCALL
ia32whpx_setWHPXtoNP2IOADDR()
{
	UINT16 iobase;
	
	if (CPU_STAT_PM && !CPU_STAT_VM86) {
		/* check descriptor type & stack room size */
		switch (CPU_TR_DESC.type) {
		case CPU_SYSDESC_TYPE_TSS_16:
		case CPU_SYSDESC_TYPE_TSS_BUSY_16:
			if (CPU_TR_DESC.u.seg.limit < TSS_16_LIMIT) {
				return;
			}
			iobase = 0;
			break;

		case CPU_SYSDESC_TYPE_TSS_32:
		case CPU_SYSDESC_TYPE_TSS_BUSY_32:
			if (CPU_TR_DESC.u.seg.limit < TSS_32_LIMIT) {
				return;
			}
			iobase = cpu_kmemoryread_w(CPU_TR_DESC.u.seg.segbase + 102);
				//return;
			break;

		default:
			return;
		}

		/* I/O deny bitmap */
		CPU_STAT_IOLIMIT = 0;
		if (CPU_TR_DESC.type == CPU_SYSDESC_TYPE_TSS_BUSY_32) {
			if (iobase < CPU_TR_LIMIT) {
				CPU_STAT_IOLIMIT = (UINT16)(CPU_TR_LIMIT - iobase);
				CPU_STAT_IOADDR = CPU_TR_BASE + iobase;
			}
		}
	}
}

// WHPXレジスタ → NP2 IA-32 レジスタ
void
ia32whpx_copyregWHPXtoNP2(void)
{
	CPU_EAX = np2whpxstat.state.eax;
	CPU_EBX = np2whpxstat.state.ebx;
	CPU_ECX = np2whpxstat.state.ecx;
	CPU_EDX = np2whpxstat.state.edx;

	CPU_ESI = np2whpxstat.state.esi;
	CPU_EDI = np2whpxstat.state.edi;

	CPU_CS = np2whpxstat.state.cs.selector;
	CPU_DS = np2whpxstat.state.ds.selector;
	CPU_ES = np2whpxstat.state.es.selector;
	CPU_SS = np2whpxstat.state.ss.selector;
	CPU_FS = np2whpxstat.state.fs.selector;
	CPU_GS = np2whpxstat.state.gs.selector;
	CS_BASE = np2whpxstat.state.cs.base;
	DS_BASE = np2whpxstat.state.ds.base;
	ES_BASE = np2whpxstat.state.es.base;
	SS_BASE = np2whpxstat.state.ss.base;
	FS_BASE = np2whpxstat.state.fs.base;
	GS_BASE = np2whpxstat.state.gs.base;

	CPU_CS_DESC.u.seg.limit = np2whpxstat.state.cs.limit;
	CPU_CS_DESC.flag = np2whpxstat.state.cs.type;
	CPU_CS_DESC.s = np2whpxstat.state.cs.desc;
	CPU_CS_DESC.dpl = np2whpxstat.state.cs.dpl;
	CPU_CS_DESC.rpl = np2whpxstat.state.cs.selector & 0x3;
	CPU_CS_DESC.p = np2whpxstat.state.cs.available;
	CPU_CS_DESC.valid = np2whpxstat.state.cs.present;
	CPU_CS_DESC.d = np2whpxstat.state.cs.operand_size;
	CPU_CS_DESC.u.seg.g = np2whpxstat.state.cs.granularity;

	CPU_DS_DESC.u.seg.limit = np2whpxstat.state.ds.limit;
	CPU_DS_DESC.flag = np2whpxstat.state.ds.type;
	CPU_DS_DESC.s = np2whpxstat.state.ds.desc;
	CPU_DS_DESC.dpl = np2whpxstat.state.ds.dpl;
	CPU_DS_DESC.rpl = np2whpxstat.state.ds.selector & 0x3;
	CPU_DS_DESC.p = np2whpxstat.state.ds.available;
	CPU_DS_DESC.valid = np2whpxstat.state.ds.present;
	CPU_DS_DESC.d = np2whpxstat.state.ds.operand_size;
	CPU_DS_DESC.u.seg.g = np2whpxstat.state.ds.granularity;

	CPU_ES_DESC.u.seg.limit = np2whpxstat.state.es.limit;
	CPU_ES_DESC.flag = np2whpxstat.state.es.type;
	CPU_ES_DESC.s = np2whpxstat.state.es.desc;
	CPU_ES_DESC.dpl = np2whpxstat.state.es.dpl;
	CPU_ES_DESC.rpl = np2whpxstat.state.es.selector & 0x3;
	CPU_ES_DESC.p = np2whpxstat.state.es.available;
	CPU_ES_DESC.valid = np2whpxstat.state.es.present;
	CPU_ES_DESC.d = np2whpxstat.state.es.operand_size;
	CPU_ES_DESC.u.seg.g = np2whpxstat.state.es.granularity;

	CPU_SS_DESC.u.seg.limit = np2whpxstat.state.ss.limit;
	CPU_SS_DESC.flag = np2whpxstat.state.ss.type;
	CPU_SS_DESC.s = np2whpxstat.state.ss.desc;
	CPU_SS_DESC.dpl = np2whpxstat.state.ss.dpl;
	CPU_SS_DESC.rpl = np2whpxstat.state.ss.selector & 0x3;
	CPU_SS_DESC.p = np2whpxstat.state.ss.available;
	CPU_SS_DESC.valid = np2whpxstat.state.ss.present;
	CPU_SS_DESC.d = np2whpxstat.state.ss.operand_size;
	CPU_SS_DESC.u.seg.g = np2whpxstat.state.ss.granularity;

	CPU_FS_DESC.u.seg.limit = np2whpxstat.state.fs.limit;
	CPU_FS_DESC.flag = np2whpxstat.state.fs.type;
	CPU_FS_DESC.s = np2whpxstat.state.fs.desc;
	CPU_FS_DESC.dpl = np2whpxstat.state.fs.dpl;
	CPU_FS_DESC.rpl = np2whpxstat.state.fs.selector & 0x3;
	CPU_FS_DESC.p = np2whpxstat.state.fs.available;
	CPU_FS_DESC.valid = np2whpxstat.state.fs.present;
	CPU_FS_DESC.d = np2whpxstat.state.fs.operand_size;
	CPU_FS_DESC.u.seg.g = np2whpxstat.state.fs.granularity;

	CPU_GS_DESC.u.seg.limit = np2whpxstat.state.gs.limit;
	CPU_GS_DESC.flag = np2whpxstat.state.gs.type;
	CPU_GS_DESC.s = np2whpxstat.state.gs.desc;
	CPU_GS_DESC.dpl = np2whpxstat.state.gs.dpl;
	CPU_GS_DESC.rpl = np2whpxstat.state.gs.selector & 0x3;
	CPU_GS_DESC.p = np2whpxstat.state.gs.available;
	CPU_GS_DESC.valid = np2whpxstat.state.gs.present;
	CPU_GS_DESC.d = np2whpxstat.state.gs.operand_size;
	CPU_GS_DESC.u.seg.g = np2whpxstat.state.gs.granularity;

	CPU_EBP = np2whpxstat.state.ebp;
	CPU_ESP = np2whpxstat.state.esp;
	CPU_EIP = np2whpxstat.state.eip;
	CPU_PREV_EIP = np2whpxstat.state.eip; // XXX: あんまり良くない

	CPU_EFLAG = np2whpxstat.state.eflags;

	CPU_CR0 = np2whpxstat.state.cr0;
	//CPU_CR1 = np2whpxstat.state._cr1;
	CPU_CR2 = np2whpxstat.state.cr2;
	CPU_CR3 = np2whpxstat.state.cr3;
	CPU_CR4 = np2whpxstat.state.cr4;

	CPU_GDTR_BASE = np2whpxstat.state.gdt.base;
	CPU_GDTR_LIMIT = np2whpxstat.state.gdt.limit;
	CPU_IDTR_BASE = np2whpxstat.state.idt.base;
	CPU_IDTR_LIMIT = np2whpxstat.state.idt.limit;
	CPU_LDTR = np2whpxstat.state.ldt.selector;
	CPU_LDTR_BASE = np2whpxstat.state.ldt.base;
	CPU_LDTR_LIMIT = np2whpxstat.state.ldt.limit;
	CPU_LDTR_DESC.flag = np2whpxstat.state.ldt.type;
	CPU_LDTR_DESC.s = np2whpxstat.state.ldt.desc;
	CPU_LDTR_DESC.dpl = np2whpxstat.state.ldt.dpl;
	CPU_LDTR_DESC.p = np2whpxstat.state.ldt.available;
	CPU_LDTR_DESC.valid = np2whpxstat.state.ldt.present;
	CPU_LDTR_DESC.d = np2whpxstat.state.ldt.operand_size;
	CPU_LDTR_DESC.u.seg.g = np2whpxstat.state.ldt.granularity;
	CPU_TR = np2whpxstat.state.tr.selector;
	CPU_TR_BASE = np2whpxstat.state.tr.base;
	CPU_TR_LIMIT = np2whpxstat.state.tr.limit;
	CPU_TR_DESC.flag = np2whpxstat.state.tr.type;
	CPU_TR_DESC.s = np2whpxstat.state.tr.desc;
	CPU_TR_DESC.dpl = np2whpxstat.state.tr.dpl;
	CPU_TR_DESC.p = np2whpxstat.state.tr.available;
	CPU_TR_DESC.valid = np2whpxstat.state.tr.present;
	CPU_TR_DESC.d = np2whpxstat.state.tr.operand_size;
	CPU_TR_DESC.u.seg.segbase = np2whpxstat.state.tr.base;
	CPU_TR_DESC.u.seg.limit = np2whpxstat.state.tr.limit;
	CPU_TR_DESC.u.seg.c = 0;
	CPU_TR_DESC.u.seg.g = np2whpxstat.state.tr.granularity;

	CPU_DR(0) = np2whpxstat.state.dr0;
	CPU_DR(1) = np2whpxstat.state.dr1;
	CPU_DR(2) = np2whpxstat.state.dr2;
	CPU_DR(3) = np2whpxstat.state.dr3;
	//CPU_DR(4) = np2whpxstat.state._dr4;
	//CPU_DR(5) = np2whpxstat.state._dr5;
	CPU_DR(6) = np2whpxstat.state.dr6;
	CPU_DR(7) = np2whpxstat.state.dr7;

	CPU_STAT_PM = (np2whpxstat.state.cr0 & 0x1) != 0;
	CPU_STAT_PAGING = (CPU_CR0 & CPU_CR0_PG) != 0;
	CPU_STAT_VM86 = (np2whpxstat.state.eflags & VM_FLAG) != 0;
	CPU_STAT_WP = (CPU_CR0 & CPU_CR0_WP) ? 0x10 : 0;
	CPU_STAT_CPL = (UINT8)CPU_CS_DESC.rpl;
	CPU_STAT_USER_MODE = (CPU_CS_DESC.rpl == 3) ? CPU_MODE_USER : CPU_MODE_SUPERVISER;
	CPU_STAT_PDE_BASE = CPU_CR3 & CPU_CR3_PD_MASK;
	//CPU_STAT_PDE_BASE = np2whpxstat.state.pde;

	CPU_OV = CPU_FLAG & O_FLAG;
	CPU_TRAP = (CPU_FLAG & (I_FLAG | T_FLAG)) == (I_FLAG | T_FLAG);
	CPU_INST_OP32 = CPU_INST_AS32 =
		CPU_STATSAVE.cpu_inst_default.op_32 =
		CPU_STATSAVE.cpu_inst_default.as_32 = CPU_CS_DESC.d;
#if defined(USE_CPU_EIPMASK)
	CPU_EIPMASK = CPU_STATSAVE.cpu_inst_default.op_32 ? 0xffffffff : 0xffff;
#endif
	CPU_STAT_SS32 = CPU_SS_DESC.d;

	ia32whpx_setWHPXtoNP2IOADDR();

}
// NP2 IA-32 レジスタ → WHPXレジスタ
void
ia32whpx_copyregNP2toWHPX(void)
{
	np2whpxstat.state.eax = CPU_EAX;
	np2whpxstat.state.ebx = CPU_EBX;
	np2whpxstat.state.ecx = CPU_ECX;
	np2whpxstat.state.edx = CPU_EDX;

	np2whpxstat.state.esi = CPU_ESI;
	np2whpxstat.state.edi = CPU_EDI;

	if (1 || np2whpxstat.update_segment_regs)
	{
		np2whpxstat.state.cs.selector = CPU_CS;
		np2whpxstat.state.ds.selector = CPU_DS;
		np2whpxstat.state.es.selector = CPU_ES;
		np2whpxstat.state.ss.selector = CPU_SS;
		np2whpxstat.state.fs.selector = CPU_FS;
		np2whpxstat.state.gs.selector = CPU_GS;
		np2whpxstat.state.cs.base = CS_BASE;
		np2whpxstat.state.ds.base = DS_BASE;
		np2whpxstat.state.es.base = ES_BASE;
		np2whpxstat.state.ss.base = SS_BASE;
		np2whpxstat.state.fs.base = FS_BASE;
		np2whpxstat.state.gs.base = GS_BASE;

		np2whpxstat.state.cs.base = CPU_CS_DESC.u.seg.segbase;
		np2whpxstat.state.cs.limit = CPU_CS_DESC.u.seg.limit;
		np2whpxstat.state.cs.type = CPU_CS_DESC.flag;
		np2whpxstat.state.cs.desc = CPU_CS_DESC.s;
		np2whpxstat.state.cs.dpl = CPU_CS_DESC.dpl;
		//if(wacounter > 10)np2whpxstat.state.cs.selector = (np2whpxstat.state.cs.selector & ~0x3) | CPU_CS_DESC.rpl;
		np2whpxstat.state.cs.present = CPU_CS_DESC.valid;
		np2whpxstat.state.cs.available = CPU_CS_DESC.p;
		np2whpxstat.state.cs.operand_size = CPU_CS_DESC.d;
		np2whpxstat.state.cs.granularity = CPU_CS_DESC.u.seg.g;

		np2whpxstat.state.ds.base = CPU_DS_DESC.u.seg.segbase;
		np2whpxstat.state.ds.limit = CPU_DS_DESC.u.seg.limit;
		np2whpxstat.state.ds.type = CPU_DS_DESC.flag;
		np2whpxstat.state.ds.desc = CPU_DS_DESC.s;
		np2whpxstat.state.ds.dpl = CPU_DS_DESC.dpl;
		//if(wacounter > 10)np2whpxstat.state.ds.selector = (np2whpxstat.state.ds.selector & ~0x3) | CPU_CS_DESC.rpl;
		np2whpxstat.state.ds.present = CPU_DS_DESC.valid;
		np2whpxstat.state.ds.available = CPU_DS_DESC.p;
		np2whpxstat.state.ds.operand_size = CPU_DS_DESC.d;
		np2whpxstat.state.ds.granularity = CPU_DS_DESC.u.seg.g;

		np2whpxstat.state.es.base = CPU_ES_DESC.u.seg.segbase;
		np2whpxstat.state.es.limit = CPU_ES_DESC.u.seg.limit;
		np2whpxstat.state.es.type = CPU_ES_DESC.flag;
		np2whpxstat.state.es.desc = CPU_ES_DESC.s;
		np2whpxstat.state.es.dpl = CPU_ES_DESC.dpl;
		//if(wacounter > 10)np2whpxstat.state.es.selector = (np2whpxstat.state.es.selector & ~0x3) | CPU_CS_DESC.rpl;
		np2whpxstat.state.es.present = CPU_ES_DESC.valid;
		np2whpxstat.state.es.available = CPU_ES_DESC.p;
		np2whpxstat.state.es.operand_size = CPU_ES_DESC.d;
		np2whpxstat.state.es.granularity = CPU_ES_DESC.u.seg.g;

		np2whpxstat.state.ss.base = CPU_SS_DESC.u.seg.segbase;
		np2whpxstat.state.ss.limit = CPU_SS_DESC.u.seg.limit;
		np2whpxstat.state.ss.type = CPU_SS_DESC.flag;
		np2whpxstat.state.ss.desc = CPU_SS_DESC.s;
		np2whpxstat.state.ss.dpl = CPU_SS_DESC.dpl;
		//if(wacounter > 10)np2whpxstat.state.ss.selector = (np2whpxstat.state.ss.selector & ~0x3) | CPU_CS_DESC.rpl;
		np2whpxstat.state.ss.present = CPU_SS_DESC.valid;
		np2whpxstat.state.ss.available = CPU_SS_DESC.p;
		np2whpxstat.state.ss.operand_size = CPU_SS_DESC.d;
		np2whpxstat.state.ss.granularity = CPU_SS_DESC.u.seg.g;

		np2whpxstat.state.fs.base = CPU_FS_DESC.u.seg.segbase;
		np2whpxstat.state.fs.limit = CPU_FS_DESC.u.seg.limit;
		np2whpxstat.state.fs.type = CPU_FS_DESC.flag;
		np2whpxstat.state.fs.desc = CPU_FS_DESC.s;
		np2whpxstat.state.fs.dpl = CPU_FS_DESC.dpl;
		//if(wacounter > 10)np2whpxstat.state.fs.selector = (np2whpxstat.state.fs.selector & ~0x3) | CPU_CS_DESC.rpl;
		np2whpxstat.state.fs.present = CPU_FS_DESC.valid;
		np2whpxstat.state.fs.available = CPU_FS_DESC.p;
		np2whpxstat.state.fs.operand_size = CPU_FS_DESC.d;
		np2whpxstat.state.fs.granularity = CPU_FS_DESC.u.seg.g;

		np2whpxstat.state.gs.base = CPU_GS_DESC.u.seg.segbase;
		np2whpxstat.state.gs.limit = CPU_GS_DESC.u.seg.limit;
		np2whpxstat.state.gs.type = CPU_GS_DESC.flag;
		np2whpxstat.state.gs.desc = CPU_GS_DESC.s;
		np2whpxstat.state.gs.dpl = CPU_GS_DESC.dpl;
		//if(wacounter > 10)np2whpxstat.state.gs.selector = (np2whpxstat.state.gs.selector & ~0x3) | CPU_CS_DESC.rpl;
		np2whpxstat.state.gs.present = CPU_GS_DESC.valid;
		np2whpxstat.state.gs.available = CPU_GS_DESC.p;
		np2whpxstat.state.gs.operand_size = CPU_GS_DESC.d;
		np2whpxstat.state.gs.granularity = CPU_GS_DESC.u.seg.g;

		np2whpxstat.update_segment_regs = 0;
	}

	np2whpxstat.state.ebp = CPU_EBP;
	np2whpxstat.state.esp = CPU_ESP;
	np2whpxstat.state.eip = CPU_EIP;

	np2whpxstat.state.eflags = CPU_EFLAG;

	np2whpxstat.state.cr0 = CPU_CR0;
	//np2whpxstat.state._cr1 = CPU_CR1;
	np2whpxstat.state.cr2 = CPU_CR2;
	np2whpxstat.state.cr3 = CPU_CR3;
	np2whpxstat.state.cr4 = CPU_CR4;

	np2whpxstat.state.gdt.base = CPU_GDTR_BASE;
	np2whpxstat.state.gdt.limit = CPU_GDTR_LIMIT;
	np2whpxstat.state.idt.base = CPU_IDTR_BASE;
	np2whpxstat.state.idt.limit = CPU_IDTR_LIMIT;
	np2whpxstat.state.ldt.selector = CPU_LDTR;
	np2whpxstat.state.ldt.base = CPU_LDTR_BASE;
	np2whpxstat.state.ldt.limit = CPU_LDTR_LIMIT;
	np2whpxstat.state.ldt.type = CPU_LDTR_DESC.flag;
	np2whpxstat.state.ldt.desc = CPU_LDTR_DESC.s;
	np2whpxstat.state.ldt.dpl = CPU_LDTR_DESC.dpl;
	//np2whpxstat.state.ldt.present = CPU_LDTR_DESC.valid;
	np2whpxstat.state.ldt.available = CPU_LDTR_DESC.p;
	np2whpxstat.state.ldt.operand_size = CPU_LDTR_DESC.d;
	np2whpxstat.state.ldt.granularity = CPU_LDTR_DESC.u.seg.g;
	np2whpxstat.state.tr.selector = CPU_TR;
	np2whpxstat.state.tr.base = CPU_TR_BASE;
	np2whpxstat.state.tr.limit = CPU_TR_LIMIT;
	np2whpxstat.state.tr.type = CPU_TR_DESC.flag;
	np2whpxstat.state.tr.desc = CPU_TR_DESC.s;
	np2whpxstat.state.tr.dpl = CPU_TR_DESC.dpl;
	//np2whpxstat.state.tr.present = CPU_TR_DESC.valid;
	np2whpxstat.state.tr.available = CPU_TR_DESC.p;
	np2whpxstat.state.tr.operand_size = CPU_TR_DESC.d;
	np2whpxstat.state.tr.granularity = CPU_TR_DESC.u.seg.g;

	//np2whpxstat.state.tr.base = CPU_TR_DESC.u.seg.segbase;
	//np2whpxstat.state.tr.limit = CPU_TR_DESC.u.seg.limit;

	np2whpxstat.state.dr0 = CPU_DR(0);
	np2whpxstat.state.dr1 = CPU_DR(1);
	np2whpxstat.state.dr2 = CPU_DR(2);
	np2whpxstat.state.dr3 = CPU_DR(3);
	//np2whpxstat.state._dr4 = CPU_DR(4);
	//np2whpxstat.state._dr5 = CPU_DR(5);
	np2whpxstat.state.dr6 = CPU_DR(6);
	np2whpxstat.state.dr7 = CPU_DR(7);

}

// 仮想マシン実行
static void i386whpx_update_elapsed_time(void)
{
	SINT32 elapsed;
	np2whpxcore.clockcount = GetTickCounter_Clock();
	elapsed = (SINT32)((np2whpxcore.clockcount.QuadPart -
		np2whpxcore.lastclock.QuadPart) * pccore.realclock /
		np2whpxcore.clockpersec.QuadPart);
	if (CPU_REMCLOCK > 0) CPU_REMCLOCK -= elapsed;
	np2whpxcore.lastclock = np2whpxcore.clockcount;
}

void i386whpx_vm_exec_part(void)
{
	WHPX_RUN_RESULT result;
	UINT32 address;
	static UINT32 pmcounter;

	np2whpxcore.running = 1;
	if (!np2whpx.created) {
		np2whpxcore.running = 0;
		return;
	}

	switch (sigsetjmp(exec_1step_jmpbuf, 1)) {
	case 0:
		break;
	case 1:
		VERBOSE(("ia32: return from exception"));
		break;
	case 2:
		VERBOSE(("ia32: return from panic"));
		np2whpxcore.running = 0;
		return;
	default:
		VERBOSE(("ia32: return from unknown cause"));
		break;
	}

	if (np2whpxcore.lastA20en != (CPU_ADRSMASK != 0x000fffff)) {
		i386whpx_vm_sethmemory(CPU_ADRSMASK != 0x000fffff);
		np2whpxcore.lastA20en = (CPU_ADRSMASK != 0x000fffff);
	}
	if (np2whpxcore.lastITFbank != CPU_ITFBANK) {
		i386whpx_vm_setitfmemory(CPU_ITFBANK);
		np2whpxcore.lastITFbank = CPU_ITFBANK;
	}
	if (np2whpxcore.lastVGA256linear !=
		(vramop.mio2[0x2] == 1 && (gdc.analog & (1 << GDCANALOG_256)))) {
		i386whpx_vm_setvga256linearmemory();
		np2whpxcore.lastVGA256linear =
			(vramop.mio2[0x2] == 1 && (gdc.analog & (1 << GDCANALOG_256)));
	}

	ia32whpx_copyregNP2toWHPX();
	if (np2whpxstat.update_regs &&
		i386whpxfunc_set_registers(&np2whpxstat.state) == FAILURE) {
		result = WHPX_RUN_FAILED;
		goto finished;
	}
	if (np2whpxstat.update_fpu &&
		i386whpxfunc_set_fpu(&np2whpxstat.fpustate) == FAILURE) {
		result = WHPX_RUN_FAILED;
		goto finished;
	}

	if (np2whpxstat.irq_reqidx_cur != np2whpxstat.irq_reqidx_end) {
		if (i386whpxfunc_interrupt(
			np2whpxstat.irq_req[np2whpxstat.irq_reqidx_cur]) == SUCCESS) {
			np2whpxstat.irq_reqidx_cur++;
		}
	}

	np2whpxcore.ready_for_reset = 0;
	np2whpxcore.hltflag = 0;
	result = i386whpxfunc_run();

finished:
	if (i386whpxfunc_get_registers(&np2whpxstat.state) == SUCCESS &&
		i386whpxfunc_get_fpu(&np2whpxstat.fpustate) == SUCCESS) {
		np2whpxstat.update_regs = 0;
		np2whpxstat.update_fpu = 0;
		ia32whpx_copyregWHPXtoNP2();
	}
	i386whpx_update_elapsed_time();
	pic_irq();

	switch (result) {
	case WHPX_RUN_CONTINUE:
		break;
	case WHPX_RUN_HALT:
		np2whpxcore.hltflag = 1;
		np2whpxcore.ready_for_reset = 1;
		if (!CPU_STAT_PM || CPU_STAT_VM86) {
			CPU_PREV_EIP = CPU_EIP - 1;
			address = CPU_PREV_EIP + (CPU_CS << 4);
			if (memp_read8(address) == bioshookinfo.hookinst &&
				ia32whpx_bioscall()) {
				np2whpxstat.update_regs = 1;
				np2whpxstat.update_segment_regs = 1;
				pmcounter = 0;
				np2whpxcore.hltflag = 0;
			}
		}
		break;
	case WHPX_RUN_CANCELED:
		np2whpxcore.ready_for_reset = 1;
		break;
	case WHPX_RUN_UNSUPPORTED:
		np2whpxcore.ready_for_reset = 1;
		CPU_REMCLOCK = 0;
		break;
	case WHPX_RUN_FAILED:
	default:
		np2whpxcore.ready_for_reset = 1;
		np2whpx.enable = 0;
		CPU_REMCLOCK = 0;
		break;
	}
	np2whpxcore.running = 0;
}

void i386whpx_vm_exec(void) {
	static int skipcounter = 0;
	static SINT32 remain_clk = 0;
	SINT32 remclktmp = CPU_REMCLOCK;
	int timing;
	int i;
	if(pcstat.hardwarereset){
		CPU_REMCLOCK = 0;
		return;
	}
	if(!np2whpxcore.hltflag || !CPU_isEI || np2whpxstat.irq_reqidx_cur != np2whpxstat.irq_reqidx_end){
		CPU_REMCLOCK += remain_clk;
		np2whpxcore.lastclock = GetTickCounter_Clock();
		while(CPU_REMCLOCK > 0){
			int remclkdiff = 0;
			i386whpx_vm_exec_part();
			np2whpxcore.clockcount = GetTickCounter_Clock();
			if(CPU_REMCLOCK > 0){
				remclkdiff = (np2whpxcore.clockcount.QuadPart - np2whpxcore.lastclock.QuadPart) * pccore.realclock / np2whpxcore.clockpersec.QuadPart;
				CPU_REMCLOCK -= remclkdiff;
			}
			np2whpxcore.lastclock  = np2whpxcore.clockcount;
			if(dmac.working) {
				for(i=0;i<(remclkdiff+3)/4;i++){
					dmax86(); // XXX: 本当は1命令実行ごとに呼ぶのが正しいけど出来ないのでまとめて呼ぶ･･･
				}
			}
			if(CPU_RESETREQ) break;
			if(pcstat.hardwarereset) break;
			if (CPU_REMCLOCK > 0 && (timing = timing_getcount_baseclock())!=0) {
				CPU_REMCLOCK = 0;
				break;
			}
			if(np2whpxcore.hltflag){
				CPU_REMCLOCK = 0;
				remain_clk = 0;
				break;
			}
		}
		if(CPU_REMCLOCK <= 0){
			remain_clk = CPU_REMCLOCK;
		}else{
			remain_clk = 0;
		}
		if (CPU_REMCLOCK < -(SINT32)(CPU_BASECLOCK*3)) {
			np2whpxcore.hurryup = 1;
		}
		CPU_REMCLOCK = 0;
	}else{
		if(dmac.working) {
			for(i=0;i<(CPU_REMCLOCK+3)/4;i++){
				dmax86();
			}
		}
		CPU_REMCLOCK = 0;
		remain_clk = 0;
	}
}

void i386whpx_disposeVM(void)
{
#if defined(_WIN32)
#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_W2K)
	npdisp_w2k_whpxInvalidate();
#endif
	if (!np2whpx.created) return;
	i386whpxfunc_destroy_vm();
	np2whpx.created = 0;
#endif
}

void i386whpx_deinitialize(void)
{
#if defined(_WIN32)
	i386whpx_disposeVM();
#endif
}

// WHPX仮想マシンで使用するホストのメモリ領域を登録（登録済みメモリのみ仮想マシンに割り当て可能）
void i386whpx_vm_allocmemory(void)
{
	/* WHPX maps host buffers directly; no separate registration is needed. */
}

void i386whpx_vm_allocmemoryex(UINT8 *vramptr, UINT32 size)
{
	(void)vramptr;
	(void)size;
}

typedef struct {
	UINT64 pa_start;
	UINT64 size;
	UINT64 va;
	UINT8 flags;
} WHPX_MEMORY_MAPPING;

#define WHPX_MEMORY_READONLY 0x01
#define WHPX_MEMORY_UNMAP    0x80
#define WHPX_GPA_PAGE_SIZE   ((UINT64)0x1000)
#define WHPX_GPA_PAGE_MASK   (WHPX_GPA_PAGE_SIZE - 1)

static UINT64 whpx_page_down(UINT64 address)
{
	return address & ~WHPX_GPA_PAGE_MASK;
}

static UINT64 whpx_page_up(UINT64 address)
{
	return (address + WHPX_GPA_PAGE_MASK) & ~WHPX_GPA_PAGE_MASK;
}

static UINT8 whpx_apply_mapping(const WHPX_MEMORY_MAPPING *mapping)
{
	if (mapping->flags & WHPX_MEMORY_UNMAP) {
		i386whpxfunc_unmap_memory(mapping->pa_start, mapping->size);
		return SUCCESS;
	}
	return i386whpxfunc_map_memory(mapping->pa_start,
		(void *)(UINT_PTR)mapping->va, mapping->size,
		(mapping->flags & WHPX_MEMORY_READONLY) != 0);
}

// WHPX仮想マシンのゲスト物理アドレス(Guest Physical Address; GPA)にホストの仮想アドレス(Host Virtual Address; HVA)を割り当て
void i386whpx_vm_setmemory(void) {
	
#if defined(_WIN32)
	WHPX_MEMORY_MAPPING info = {0};

	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	
	info.pa_start = 0;
	info.size = 0xA0000;
	info.flags = 0;
	info.va = (UINT64)mem;
	if(whpx_apply_mapping(&info)==FAILURE){
		TRACEOUT(("WHPX: WHPX VM set memory failed."));
		msgbox("WHPX VM", "WHPX VM set memory failed.");
		np2whpx.enable = 0;
		return;
	}
	
	//info.pa_start = 0xFFF00000;
	//info.size = 0xF8000;
	//info.flags = 0;
	//info.va = (UINT64)mem;
	//if(whpx_apply_mapping(&info)==FAILURE){
	//	TRACEOUT(("WHPX: WHPX VM set memory failed."));
	//	msgbox("WHPX VM", "WHPX VM set memory failed.");
	//	np2whpx.enable = 0;
	//	return;
	//}
	
	TRACEOUT(("WHPX: WHPX VM set memory."));
    //msgbox("WHPX VM", "WHPX VM set memory.");
#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_W2K)
	npdisp_w2k_whpxSync();
#endif
#endif
}
void i386whpx_vm_setbankmemory() {

}
void i386whpx_vm_setitfmemory(UINT8 isitfbank) {
	
#if defined(_WIN32)
	WHPX_MEMORY_MAPPING info = {0};

	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	
	info.pa_start = 0xF8000;
	info.size = 0x8000;
	info.flags = WHPX_MEMORY_UNMAP;
	if(whpx_apply_mapping(&info)==FAILURE){
	}
	if(isitfbank){
		info.flags = WHPX_MEMORY_READONLY;
		info.va = (UINT64)(mem + ITF_ADRS);
	}else{
		info.flags = WHPX_MEMORY_READONLY;
		info.va = (UINT64)(mem + info.pa_start);
	}
	if(whpx_apply_mapping(&info)==FAILURE){
		TRACEOUT(("WHPX: WHPX VM set ITF bank memory failed."));
		msgbox("WHPX VM", "WHPX VM set ITF bank memory failed.");
		np2whpx.enable = 0;
		return;
	}
	
	info.pa_start = 0xFFFF8000;
	info.size = 0x8000;
	info.flags = WHPX_MEMORY_UNMAP;
	if(whpx_apply_mapping(&info)==FAILURE){
	}
	if(isitfbank){
		info.flags = WHPX_MEMORY_READONLY;
		info.va = (UINT64)(mem + ITF_ADRS);
	}else{
		info.flags = WHPX_MEMORY_READONLY;
		info.va = (UINT64)(mem + 0xF8000);
	}
	if(whpx_apply_mapping(&info)==FAILURE){
		TRACEOUT(("WHPX: WHPX VM set ITF bank memory failed."));
		msgbox("WHPX VM", "WHPX VM set ITF bank memory failed.");
		np2whpx.enable = 0;
		return;
	}

	if(isitfbank){
		TRACEOUT(("WHPX: WHPX VM set ITF bank memory. (ITF)"));
		//msgbox("WHPX VM", "WHPX VM set ITF bank memory. (ITF)");
	}else{
		TRACEOUT(("WHPX: WHPX VM set ITF bank memory. (OFF)"));
		//msgbox("WHPX VM", "WHPX VM set ITF bank memory. (OFF)");
	}
#endif
}
void i386whpx_vm_sethmemory(UINT8 a20en) {
	
#if defined(_WIN32)
	WHPX_MEMORY_MAPPING info = {0};
	
	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	
	info.pa_start = 0x100000;
	info.size = 0x10000;
	info.flags = WHPX_MEMORY_UNMAP;
	if(whpx_apply_mapping(&info)==FAILURE){
	}

	if(a20en){
		info.flags = 0;
		info.va = (UINT64)(mem + info.pa_start);
	}else{
		info.flags = 0;
		info.va = (UINT64)(mem);
	}
	if(whpx_apply_mapping(&info)==FAILURE){
		TRACEOUT(("WHPX: WHPX VM set high memory failed."));
		msgbox("WHPX VM", "WHPX VM set high memory failed.");
		np2whpx.enable = 0;
		return;
	}
	
	if(a20en){
		TRACEOUT(("WHPX: WHPX VM set high memory. (A20 ON)"));
		//msgbox("WHPX VM", "WHPX VM set high memory. (A20 ON)");
	}else{
		TRACEOUT(("WHPX: WHPX VM set high memory. (A20 OFF)"));
		//msgbox("WHPX VM", "WHPX VM set ITF bank memory. (A20 OFF)");
	}
#endif
}
void i386whpx_vm_setextmemory(void) {
	
#if defined(_WIN32)
	WHPX_MEMORY_MAPPING info = {0};
	
	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	
	info.pa_start = 0x110000;
	info.size = CPU_EXTLIMIT16 - 0x110000;
	info.flags = 0;
	info.va = (UINT64)(CPU_EXTMEMBASE + info.pa_start);
	if(whpx_apply_mapping(&info)==FAILURE){
		TRACEOUT(("WHPX: WHPX VM set ext16 memory failed."));
		msgbox("WHPX VM", "WHPX VM set ext16 memory failed.");
		np2whpx.enable = 0;
		return;
	}

	if(CPU_EXTLIMIT > 0x01000000){
		info.pa_start = 0x01000000;
		info.size = CPU_EXTLIMIT - 0x01000000;
		info.flags = 0;
		info.va = (UINT64)(CPU_EXTMEMBASE + info.pa_start);
		if(whpx_apply_mapping(&info)==FAILURE){
			TRACEOUT(("WHPX: WHPX VM set ext memory failed."));
			msgbox("WHPX VM", "WHPX VM set ext memory failed.");
			np2whpx.enable = 0;
			return;
		}
	}
	
	TRACEOUT(("WHPX: WHPX VM set ext memory."));
    //msgbox("WHPX VM", "WHPX VM set ext memory.");
#endif
}

void i386whpx_vm_setvga256linearmemory(void) {
	
#if defined(_WIN32)
	WHPX_MEMORY_MAPPING info = {0};
	
	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	
	info.pa_start = 0xF00000;
	info.size =  0x80000;
	info.flags = WHPX_MEMORY_UNMAP;
	whpx_apply_mapping(&info);
	if(vramop.mio2[0x2]==0x1 && (gdc.analog & (1 << GDCANALOG_256))!=0){
		info.flags = 0;
		info.va = (UINT64)vramex;
		if(whpx_apply_mapping(&info)==FAILURE){
			TRACEOUT(("WHPX: WHPX VM set vga256 linear memory failed."));
			msgbox("WHPX VM", "WHPX VM set vga256 linear memory failed.");
			return;
		}
	
		TRACEOUT(("WHPX: WHPX VM set vga256 linear memory."));
	}
	
	info.pa_start = 0xfff00000;
	info.size =  0x80000;
	info.flags = WHPX_MEMORY_UNMAP;
	info.va = 0;
	whpx_apply_mapping(&info);
	info.flags = 0;
	info.va = (UINT64)vramex;
	if(whpx_apply_mapping(&info)==FAILURE){
		TRACEOUT(("WHPX: WHPX VM set vga256 0xfff00000 linear memory failed."));
		msgbox("WHPX VM", "WHPX VM set vga256 0xfff00000 linear memory failed.");
		return;
	}
    //msgbox("WHPX VM", "WHPX VM set ext memory.");
#endif
}

void i386whpx_vm_setmemoryarea(UINT8 *vramptr, UINT32 addr, UINT32 size) {
	
#if defined(_WIN32)
	WHPX_MEMORY_MAPPING info = {0};
	UINT64 request_start;
	UINT64 request_end;
	UINT64 unmap_start;
	UINT64 unmap_end;
	UINT64 map_start;
	UINT64 map_end;
	UINT64 offset;
	UINT8 *map_ptr;
	
	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	if(!size) return;

	/* WHPX mappings are page based; partial edge pages stay MMIO. */
	request_start = (UINT64)addr;
	request_end = request_start + (UINT64)size;
	unmap_start = whpx_page_down(request_start);
	unmap_end = whpx_page_up(request_end);
	if(unmap_end > unmap_start){
		i386whpxfunc_unmap_memory(unmap_start, unmap_end - unmap_start);
	}

	if(vramptr){
		map_start = whpx_page_up(request_start);
		map_end = whpx_page_down(request_end);
		if(map_end <= map_start) return;

		offset = map_start - request_start;
		map_ptr = vramptr + (UINT32)offset;
		if((((UINT_PTR)map_ptr) & (UINT_PTR)WHPX_GPA_PAGE_MASK) != 0){
			TRACEOUT(("WHPX: memory area source is not page aligned addr=%08x size=%08x", addr, size));
			return;
		}

		info.pa_start = map_start;
		info.size = map_end - map_start;
		info.flags = 0;
		info.va = (UINT64)(UINT_PTR)map_ptr;
		if(whpx_apply_mapping(&info)==FAILURE){
			TRACEOUT(("WHPX: WHPX VM set memory area failed addr=%08x size=%08x map=%08x",
				addr, size, (UINT32)info.size));
			return;
		}
	
		TRACEOUT(("WHPX: WHPX VM set memory area."));
	}
#endif
}
void i386whpx_vm_removememoryarea(UINT8 *vramptr, UINT32 addr, UINT32 size) {
	
#if defined(_WIN32)
	UINT64 request_start;
	UINT64 request_end;
	UINT64 unmap_start;
	UINT64 unmap_end;
	(void)vramptr;
	
	if(!np2whpx.enable) return;
	if(!np2whpx.created) return;
	if(!size) return;

	request_start = (UINT64)addr;
	request_end = request_start + (UINT64)size;
	unmap_start = whpx_page_down(request_start);
	unmap_end = whpx_page_up(request_end);
	if(unmap_end > unmap_start){
		i386whpxfunc_unmap_memory(unmap_start, unmap_end - unmap_start);
	}
#endif
}

#endif
