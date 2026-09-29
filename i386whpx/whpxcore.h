#ifndef NP2_I386WHPX_WHPXCORE_H__
#define NP2_I386WHPX_WHPXCORE_H__

#if defined(SUPPORT_IA32_WHPX)

#include "whpxfunc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	UINT8 available;
	UINT8 enable;
	UINT8 created;
	UINT8 emumode;
	UINT8 bioshookenable;
} NP2_WHPX;

typedef struct {
	WHPX_CPU_STATE state;
	WHPX_FPU_STATE fpustate;
	WHPX_CPU_STATE default_state;
	WHPX_FPU_STATE default_fpustate;
	UINT8 update_regs;
	UINT8 update_segment_regs;
	UINT8 update_fpu;
	UINT8 irq_req[256];
	UINT8 irq_reqidx_cur;
	UINT8 irq_reqidx_end;
} NP2_WHPX_STAT;

typedef struct {
	UINT8 running;
	LARGE_INTEGER lastclock;
	LARGE_INTEGER clockpersec;
	LARGE_INTEGER clockcount;
	UINT8 I_ratio;
	UINT32 lastA20en;
	UINT32 lastITFbank;
	UINT32 lastVGA256linear;
	UINT32 lastVRAMMMIO;
	UINT8 hurryup;
	UINT8 hltflag;
	UINT8 allocwabmem;
	UINT8 ready_for_reset;
} NP2_WHPX_CORE;

#define NP2WHPX_I_RATIO_MAX 1024

extern NP2_WHPX np2whpx;
extern NP2_WHPX_STAT np2whpxstat;
extern NP2_WHPX_CORE np2whpxcore;

UINT8 i386whpx_check(void);
void i386whpx_initialize(void);
void i386whpx_createVM(void);
void i386whpx_resetVMMem(void);
void i386whpx_resetVMCPU(void);
void i386whpx_disposeVM(void);
void i386whpx_deinitialize(void);
void i386whpx_vm_exec(void);

void i386whpx_vm_allocmemory(void);
void i386whpx_vm_allocmemoryex(UINT8 *vramptr, UINT32 size);
void i386whpx_vm_setmemory(void);
void i386whpx_vm_setbankmemory(void);
void i386whpx_vm_setitfmemory(UINT8 isitfbank);
void i386whpx_vm_sethmemory(UINT8 a20en);
void i386whpx_vm_setextmemory(void);
void i386whpx_vm_setvga256linearmemory(void);
void i386whpx_vm_setmemoryarea(UINT8 *vramptr, UINT32 addr, UINT32 size);
void i386whpx_vm_removememoryarea(UINT8 *vramptr, UINT32 addr, UINT32 size);

void ia32whpx_copyregWHPXtoNP2(void);
void ia32whpx_copyregNP2toWHPX(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPPORT_IA32_WHPX */

#endif /* NP2_I386WHPX_WHPXCORE_H__ */
