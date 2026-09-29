#ifndef NP2_I386WHPX_WHPXFUNC_H__
#define NP2_I386WHPX_WHPXFUNC_H__

#if defined(SUPPORT_IA32_WHPX)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	UINT16 selector;
	UINT32 limit;
	UINT64 base;
	union {
		struct {
			UINT32 type         : 4;
			UINT32 desc         : 1;
			UINT32 dpl          : 2;
			UINT32 present      : 1;
			UINT32              : 4;
			UINT32 available    : 1;
			UINT32 long_mode    : 1;
			UINT32 operand_size : 1;
			UINT32 granularity  : 1;
			UINT32 null_segment : 1;
			UINT32              : 15;
		};
		UINT32 attributes;
	};
} WHPX_SEGMENT;

typedef struct {
	union {
		UINT64 regs[16];
		struct {
			union { struct { UINT8 al, ah; }; UINT16 ax; UINT32 eax; UINT64 rax; };
			union { struct { UINT8 cl, ch; }; UINT16 cx; UINT32 ecx; UINT64 rcx; };
			union { struct { UINT8 dl, dh; }; UINT16 dx; UINT32 edx; UINT64 rdx; };
			union { struct { UINT8 bl, bh; }; UINT16 bx; UINT32 ebx; UINT64 rbx; };
			union { UINT16 sp; UINT32 esp; UINT64 rsp; };
			union { UINT16 bp; UINT32 ebp; UINT64 rbp; };
			union { UINT16 si; UINT32 esi; UINT64 rsi; };
			union { UINT16 di; UINT32 edi; UINT64 rdi; };
			UINT64 r8, r9, r10, r11, r12, r13, r14, r15;
		};
	};
	union { UINT32 eip; UINT64 rip; };
	union { UINT32 eflags; UINT64 rflags; };
	WHPX_SEGMENT cs, ss, ds, es, fs, gs, ldt, tr;
	WHPX_SEGMENT gdt, idt;
	UINT64 cr0, cr2, cr3, cr4;
	UINT64 dr0, dr1, dr2, dr3, dr6, dr7;
	UINT32 efer;
	UINT32 sysenter_cs;
	UINT64 sysenter_eip;
	UINT64 sysenter_esp;
} WHPX_CPU_STATE;

typedef struct {
	UINT16 control;
	UINT16 status;
	UINT8 tag;
	UINT16 opcode;
	UINT64 instruction_pointer;
	UINT64 data_pointer;
	UINT32 mxcsr;
	UINT32 mxcsr_mask;
	UINT8 fp_mmx[8][16];
	UINT8 xmm[16][16];
} WHPX_FPU_STATE;

typedef enum {
	WHPX_RUN_CONTINUE = 0,
	WHPX_RUN_HALT,
	WHPX_RUN_CANCELED,
	WHPX_RUN_UNSUPPORTED,
	WHPX_RUN_FAILED
} WHPX_RUN_RESULT;

UINT8 i386whpxfunc_is_available(void);
UINT8 i386whpxfunc_create_vm(void);
void i386whpxfunc_destroy_vm(void);

UINT8 i386whpxfunc_map_memory(UINT64 guest_address, void *host_address,
							  UINT64 size, UINT8 readonly);
void i386whpxfunc_unmap_memory(UINT64 guest_address, UINT64 size);

WHPX_RUN_RESULT i386whpxfunc_run(void);
UINT8 i386whpxfunc_interrupt(UINT8 vector);

UINT8 i386whpxfunc_set_registers(const WHPX_CPU_STATE *state);
UINT8 i386whpxfunc_get_registers(WHPX_CPU_STATE *state);
UINT8 i386whpxfunc_set_fpu(const WHPX_FPU_STATE *state);
UINT8 i386whpxfunc_get_fpu(WHPX_FPU_STATE *state);

#ifdef __cplusplus
}
#endif

#endif /* SUPPORT_IA32_WHPX */

#endif /* NP2_I386WHPX_WHPXFUNC_H__ */
