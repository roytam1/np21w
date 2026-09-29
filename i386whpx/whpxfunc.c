/*
 * Windows Hypervisor Platform backend for Neko Project II.
 *
 * This file intentionally exposes only WHPX concepts.  Port and MMIO exits
 * are completed by the Windows Hypervisor Instruction Emulator, while the
 * core owns timing, interrupt delivery and conversion to the NP2 CPU state.
 */
#include "compiler.h"

#include "pccore.h"
#include "cpucore.h"
#include "iocore.h"
#include "whpxfunc.h"

#if defined(SUPPORT_IA32_WHPX)

#include <WinHvPlatform.h>
#include <WinHvEmulation.h>

#if 0
#undef	TRACEOUT
#define USE_TRACEOUT_VS
#ifdef USE_TRACEOUT_VS
static void trace_fmt_ex(const char* fmt, ...)
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

#define WHPX_VP_INDEX 0
#define WHPX_TIMESLICE_MS 1

static WHV_PARTITION_HANDLE s_partition;
static WHV_EMULATOR_HANDLE s_emulator;
static UINT8 s_vcpu_created;
static volatile LONG s_vcpu_running;
static UINT8 s_interrupt_window_ready;
static UINT8 s_interrupt_window_requested;

static const WHV_REGISTER_NAME s_cpu_registers[] = {
	WHvX64RegisterRax, WHvX64RegisterRcx, WHvX64RegisterRdx, WHvX64RegisterRbx,
	WHvX64RegisterRsp, WHvX64RegisterRbp, WHvX64RegisterRsi, WHvX64RegisterRdi,
	WHvX64RegisterR8, WHvX64RegisterR9, WHvX64RegisterR10, WHvX64RegisterR11,
	WHvX64RegisterR12, WHvX64RegisterR13, WHvX64RegisterR14, WHvX64RegisterR15,
	WHvX64RegisterRip, WHvX64RegisterRflags,
	WHvX64RegisterCs, WHvX64RegisterSs, WHvX64RegisterDs, WHvX64RegisterEs,
	WHvX64RegisterFs, WHvX64RegisterGs, WHvX64RegisterLdtr, WHvX64RegisterTr,
	WHvX64RegisterGdtr, WHvX64RegisterIdtr,
	WHvX64RegisterCr0, WHvX64RegisterCr2, WHvX64RegisterCr3, WHvX64RegisterCr4,
	WHvX64RegisterDr0, WHvX64RegisterDr1, WHvX64RegisterDr2, WHvX64RegisterDr3,
	WHvX64RegisterDr6, WHvX64RegisterDr7,
	WHvX64RegisterEfer, WHvX64RegisterSysenterCs, WHvX64RegisterSysenterEip,
	WHvX64RegisterSysenterEsp
};

static const WHV_REGISTER_NAME s_fpu_registers[] = {
	WHvX64RegisterFpMmx0, WHvX64RegisterFpMmx1,
	WHvX64RegisterFpMmx2, WHvX64RegisterFpMmx3,
	WHvX64RegisterFpMmx4, WHvX64RegisterFpMmx5,
	WHvX64RegisterFpMmx6, WHvX64RegisterFpMmx7,
	WHvX64RegisterXmm0, WHvX64RegisterXmm1,
	WHvX64RegisterXmm2, WHvX64RegisterXmm3,
	WHvX64RegisterXmm4, WHvX64RegisterXmm5,
	WHvX64RegisterXmm6, WHvX64RegisterXmm7,
	WHvX64RegisterXmm8, WHvX64RegisterXmm9,
	WHvX64RegisterXmm10, WHvX64RegisterXmm11,
	WHvX64RegisterXmm12, WHvX64RegisterXmm13,
	WHvX64RegisterXmm14, WHvX64RegisterXmm15,
	WHvX64RegisterFpControlStatus, WHvX64RegisterXmmControlStatus
};

static void whpx_segment_to_whv(const WHPX_SEGMENT *src,
								WHV_X64_SEGMENT_REGISTER *dst)
{
	memset(dst, 0, sizeof(*dst));
	dst->Base = src->base;
	dst->Limit = src->limit;
	dst->Selector = src->selector;
	dst->Attributes = (UINT16)src->attributes;
}

static void whpx_segment_from_whv(const WHV_X64_SEGMENT_REGISTER *src,
								  WHPX_SEGMENT *dst)
{
	memset(dst, 0, sizeof(*dst));
	dst->base = src->Base;
	dst->limit = src->Limit;
	dst->selector = src->Selector;
	dst->attributes = src->Attributes;
}

static HRESULT CALLBACK whpx_io_callback(void *context,
									 WHV_EMULATOR_IO_ACCESS_INFO *access)
{
	(void)context;
	if (access->Direction) {
		switch (access->AccessSize) {
		case 1: iocore_out8(access->Port, (UINT8)access->Data); break;
		case 2: iocore_out16(access->Port, (UINT16)access->Data); break;
		case 4: iocore_out32(access->Port, access->Data); break;
		default: return E_INVALIDARG;
		}
	}
	else {
		switch (access->AccessSize) {
		case 1: access->Data = iocore_inp8(access->Port); break;
		case 2: access->Data = iocore_inp16(access->Port); break;
		case 4: access->Data = iocore_inp32(access->Port); break;
		default: return E_INVALIDARG;
		}
	}
	return S_OK;
}

static HRESULT CALLBACK whpx_memory_callback(void *context,
									 WHV_EMULATOR_MEMORY_ACCESS_INFO *access)
{
	UINT32 address = (UINT32)access->GpaAddress;
	UINT8 i;
	(void)context;
	if (!access->AccessSize || access->AccessSize > sizeof(access->Data)) {
		return E_INVALIDARG;
	}
	if (access->Direction) {
		if (access->AccessSize == 1) memp_write8(address, access->Data[0]);
		else if (access->AccessSize == 2) memp_write16(address, LOADINTELWORD(access->Data));
		else if (access->AccessSize == 4) memp_write32(address, LOADINTELDWORD(access->Data));
		else for (i = 0; i < access->AccessSize; i++) memp_write8(address + i, access->Data[i]);
	}
	else {
		if (access->AccessSize == 1) access->Data[0] = memp_read8(address);
		else if (access->AccessSize == 2) STOREINTELWORD(access->Data, memp_read16(address));
		else if (access->AccessSize == 4) STOREINTELDWORD(access->Data, memp_read32(address));
		else for (i = 0; i < access->AccessSize; i++) access->Data[i] = memp_read8(address + i);
	}
	return S_OK;
}

static HRESULT CALLBACK whpx_get_registers_callback(void *context,
		const WHV_REGISTER_NAME *names, UINT32 count, WHV_REGISTER_VALUE *values)
{
	(void)context;
	return WHvGetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
										 names, count, values);
}

static HRESULT CALLBACK whpx_set_registers_callback(void *context,
		const WHV_REGISTER_NAME *names, UINT32 count,
		const WHV_REGISTER_VALUE *values)
{
	(void)context;
	return WHvSetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
										 names, count, values);
}

static HRESULT CALLBACK whpx_translate_callback(void *context,
		WHV_GUEST_VIRTUAL_ADDRESS gva, WHV_TRANSLATE_GVA_FLAGS flags,
		WHV_TRANSLATE_GVA_RESULT_CODE *result_code,
		WHV_GUEST_PHYSICAL_ADDRESS *gpa)
{
	WHV_TRANSLATE_GVA_RESULT result;
	HRESULT hr;
	(void)context;
	hr = WHvTranslateGva(s_partition, WHPX_VP_INDEX, gva, flags, &result, gpa);
	if (SUCCEEDED(hr)) *result_code = result.ResultCode;
	return hr;
}

static const WHV_EMULATOR_CALLBACKS s_emulator_callbacks = {
	sizeof(WHV_EMULATOR_CALLBACKS), 0,
	whpx_io_callback, whpx_memory_callback,
	whpx_get_registers_callback, whpx_set_registers_callback,
	whpx_translate_callback
};

static VOID CALLBACK whpx_timeslice_callback(PVOID context, BOOLEAN fired)
{
	(void)context;
	(void)fired;
	if (s_partition && s_vcpu_created &&
		InterlockedCompareExchange(&s_vcpu_running, 1, 1) != 0) {
		WHvCancelRunVirtualProcessor(s_partition, WHPX_VP_INDEX, 0);
	}
}


static UINT8 whpx_configure_cpuid(void)
{
	WHV_X64_CPUID_RESULT result[8];
	char brand[64];
	UINT32 eax;
	UINT32 i;
	UINT32 offset;
	HRESULT hr;

	memset(result, 0, sizeof(result));

	result[0].Function = 0;
	result[0].Eax = 1;
	result[0].Ebx = LOADINTELDWORD((UINT8 *)(i386cpuid.cpu_vendor + 0));
	result[0].Edx = LOADINTELDWORD((UINT8 *)(i386cpuid.cpu_vendor + 4));
	result[0].Ecx = LOADINTELDWORD((UINT8 *)(i386cpuid.cpu_vendor + 8));

	result[1].Function = 1;
	result[1].Eax = (((i386cpuid.cpu_family >> 4) & 0xff) << 20) |
		(((i386cpuid.cpu_model >> 4) & 0xf) << 16) |
		((i386cpuid.cpu_family & 0xf) << 8) |
		((i386cpuid.cpu_model & 0xf) << 4) |
		(i386cpuid.cpu_stepping & 0xf);
	result[1].Ebx = i386cpuid.cpu_brandid;
	if (i386cpuid.cpu_feature & CPU_FEATURE_CLFSH) {
		result[1].Ebx |= 8 << 8;
	}
	result[1].Ecx = i386cpuid.cpu_feature_ecx & CPU_FEATURES_ECX_ALL;
	result[1].Edx = i386cpuid.cpu_feature & CPU_FEATURES_ALL;

	result[2].Function = 2;
	if (i386cpuid.cpu_family >= 6) {
		result[2].Eax = 1;
		result[2].Edx = 0x43;
	}

	result[3].Function = 0x80000000;
	result[3].Eax = 0x80000004;
	if (strncmp(i386cpuid.cpu_vendor, CPU_VENDOR_AMD, 12) == 0) {
		result[3].Ebx = LOADINTELDWORD((UINT8 *)(i386cpuid.cpu_vendor + 0));
		result[3].Edx = LOADINTELDWORD((UINT8 *)(i386cpuid.cpu_vendor + 4));
		result[3].Ecx = LOADINTELDWORD((UINT8 *)(i386cpuid.cpu_vendor + 8));
	}

	result[4].Function = 0x80000001;
	eax = 0;
	if (strncmp(i386cpuid.cpu_vendor, CPU_VENDOR_AMD, 12) == 0) {
		if (i386cpuid.cpu_family >= 6 ||
			(i386cpuid.cpu_family == 5 && i386cpuid.cpu_model >= 6)) {
			eax = ((i386cpuid.cpu_family + 1) << 8) |
				(i386cpuid.cpu_model << 4) | i386cpuid.cpu_stepping;
		}
		else {
			eax = (i386cpuid.cpu_family << 8) |
				(i386cpuid.cpu_model << 4) | i386cpuid.cpu_stepping;
		}
	}
	result[4].Eax = eax;
	result[4].Ecx = i386cpuid.cpu_feature_ex_ecx & CPU_FEATURES_EX_ECX_ALL;
	result[4].Edx = i386cpuid.cpu_feature_ex & CPU_FEATURES_EX_ALL;

	memset(brand, 0, sizeof(brand));
	sprintf(brand, "%s%u MHz", i386cpuid.cpu_brandstring,
		(UINT32)(pccore.realclock / 1000 / 1000));
	for (i = 0; i < 3; i++) {
		offset = i * 16;
		result[5 + i].Function = 0x80000002 + i;
		result[5 + i].Eax = LOADINTELDWORD((UINT8 *)(brand + offset + 0));
		result[5 + i].Ebx = LOADINTELDWORD((UINT8 *)(brand + offset + 4));
		result[5 + i].Ecx = LOADINTELDWORD((UINT8 *)(brand + offset + 8));
		result[5 + i].Edx = LOADINTELDWORD((UINT8 *)(brand + offset + 12));
	}

	hr = WHvSetPartitionProperty(s_partition,
		WHvPartitionPropertyCodeCpuidResultList, result, sizeof(result));
	if (FAILED(hr)) {
		TRACEOUT(("WHPX: CPUID result list failed hr=%08x", (UINT32)hr));
		return FAILURE;
	}
	return SUCCESS;
}

static UINT8 whpx_configure_msr_exits(void)
{
	WHV_CAPABILITY capability;
	WHV_EXTENDED_VM_EXITS exits;
	WHV_X64_MSR_EXIT_BITMAP bitmap;
	UINT32 written;
	HRESULT hr;

	memset(&capability, 0, sizeof(capability));
	written = 0;
	hr = WHvGetCapability(WHvCapabilityCodeExtendedVmExits,
		&capability, sizeof(capability), &written);
	if (FAILED(hr) || !capability.ExtendedVmExits.X64MsrExit) {
		TRACEOUT(("WHPX: MSR exits are not supported hr=%08x", (UINT32)hr));
		return FAILURE;
	}

	memset(&capability, 0, sizeof(capability));
	written = 0;
	hr = WHvGetCapability(WHvCapabilityCodeX64MsrExitBitmap,
		&capability, sizeof(capability), &written);
	if (FAILED(hr) || !capability.X64MsrExitBitmap.UnhandledMsrs) {
		TRACEOUT(("WHPX: unhandled MSR exits are not supported hr=%08x", (UINT32)hr));
		return FAILURE;
	}

	memset(&exits, 0, sizeof(exits));
	exits.X64MsrExit = 1;
	hr = WHvSetPartitionProperty(s_partition,
		WHvPartitionPropertyCodeExtendedVmExits, &exits, sizeof(exits));
	if (FAILED(hr)) {
		TRACEOUT(("WHPX: enabling MSR exits failed hr=%08x", (UINT32)hr));
		return FAILURE;
	}

	memset(&bitmap, 0, sizeof(bitmap));
	bitmap.UnhandledMsrs = 1;
	hr = WHvSetPartitionProperty(s_partition,
		WHvPartitionPropertyCodeX64MsrExitBitmap, &bitmap, sizeof(bitmap));
	if (FAILED(hr)) {
		TRACEOUT(("WHPX: MSR exit bitmap failed hr=%08x", (UINT32)hr));
		return FAILURE;
	}
	return SUCCESS;
}

static WHPX_RUN_RESULT whpx_handle_msr_access(
	const WHV_RUN_VP_EXIT_CONTEXT *exit_context)
{
	WHV_REGISTER_NAME names[3];
	WHV_REGISTER_VALUE values[3];
	UINT32 count;
	UINT32 msr;
	UINT64 value;
	HRESULT hr;

	msr = exit_context->MsrAccess.MsrNumber;
	memset(values, 0, sizeof(values));
	names[0] = WHvX64RegisterRip;
	values[0].Reg64 = exit_context->VpContext.Rip +
		exit_context->VpContext.InstructionLength;
	count = 1;

	if (!exit_context->MsrAccess.AccessInfo.IsWrite) {
		/* Match the existing software core: unknown MSRs read as zero. */
		value = 0;
		if (msr == 0x2c) value = 0xfee00800;
		names[1] = WHvX64RegisterRax;
		names[2] = WHvX64RegisterRdx;
		values[1].Reg64 = (UINT32)value;
		values[2].Reg64 = (UINT32)(value >> 32);
		count = 3;
		TRACEOUT(("WHPX: unhandled RDMSR msr=%08x rip=%08x value=%08x:%08x",
			msr, (UINT32)exit_context->VpContext.Rip,
			(UINT32)(value >> 32), (UINT32)value));
	}
	else {
		/* Match the existing software core: unknown MSR writes are ignored. */
		TRACEOUT(("WHPX: unhandled WRMSR msr=%08x rip=%08x value=%08x:%08x",
			msr, (UINT32)exit_context->VpContext.Rip,
			(UINT32)exit_context->MsrAccess.Rdx,
			(UINT32)exit_context->MsrAccess.Rax));
	}

	hr = WHvSetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
		names, count, values);
	return SUCCEEDED(hr) ? WHPX_RUN_CONTINUE : WHPX_RUN_FAILED;
}

UINT8 i386whpxfunc_is_available(void)
{
	WHV_CAPABILITY capability;
	UINT32 written_size = 0;
	return (SUCCEEDED(WHvGetCapability(WHvCapabilityCodeHypervisorPresent,
			&capability, sizeof(capability), &written_size)) &&
			capability.HypervisorPresent) ? SUCCESS : FAILURE;
}

UINT8 i386whpxfunc_create_vm(void)
{
	WHV_PARTITION_PROPERTY property;
	i386whpxfunc_destroy_vm();
	if (FAILED(WHvCreatePartition(&s_partition))) return FAILURE;
	memset(&property, 0, sizeof(property));
	property.ProcessorCount = 1;
	if (FAILED(WHvSetPartitionProperty(s_partition,
			WHvPartitionPropertyCodeProcessorCount, &property, sizeof(property))) ||
		whpx_configure_cpuid() != SUCCESS ||
		whpx_configure_msr_exits() != SUCCESS ||
		FAILED(WHvSetupPartition(s_partition)) ||
		FAILED(WHvCreateVirtualProcessor(s_partition, WHPX_VP_INDEX, 0))) {
		i386whpxfunc_destroy_vm();
		return FAILURE;
	}
	s_vcpu_created = 1;
	if (FAILED(WHvEmulatorCreateEmulator(&s_emulator_callbacks, &s_emulator))) {
		i386whpxfunc_destroy_vm();
		return FAILURE;
	}
	return SUCCESS;
}

void i386whpxfunc_destroy_vm(void)
{
	if (s_emulator) WHvEmulatorDestroyEmulator(s_emulator);
	s_emulator = NULL;
	if (s_partition && s_vcpu_created) {
		WHvDeleteVirtualProcessor(s_partition, WHPX_VP_INDEX);
	}
	s_vcpu_created = 0;
	InterlockedExchange(&s_vcpu_running, 0);
	s_interrupt_window_ready = 0;
	s_interrupt_window_requested = 0;
	if (s_partition) WHvDeletePartition(s_partition);
	s_partition = NULL;
}

UINT8 i386whpxfunc_map_memory(UINT64 guest_address, void *host_address,
							  UINT64 size, UINT8 readonly)
{
	WHV_MAP_GPA_RANGE_FLAGS flags;
	if (!s_partition || !host_address || !size) return FAILURE;
	flags = WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute;
	if (!readonly) flags |= WHvMapGpaRangeFlagWrite;
	WHvUnmapGpaRange(s_partition, guest_address, size);
	return SUCCEEDED(WHvMapGpaRange(s_partition, host_address, guest_address,
									 size, flags)) ? SUCCESS : FAILURE;
}

void i386whpxfunc_unmap_memory(UINT64 guest_address, UINT64 size)
{
	if (s_partition && size) WHvUnmapGpaRange(s_partition, guest_address, size);
}

WHPX_RUN_RESULT i386whpxfunc_run(void)
{
	WHV_RUN_VP_EXIT_CONTEXT exit_context;
	WHV_EMULATOR_STATUS emulator_status;
	HANDLE timer = NULL;
	HRESULT hr;
	if (!s_partition || !s_vcpu_created) return WHPX_RUN_FAILED;
	/* Interrupt-window readiness is valid only until the next VP run. */
	s_interrupt_window_ready = 0;
	memset(&exit_context, 0, sizeof(exit_context));
	if (!CreateTimerQueueTimer(&timer, NULL, whpx_timeslice_callback, NULL,
			WHPX_TIMESLICE_MS, 0, WT_EXECUTEONLYONCE)) {
		return WHPX_RUN_FAILED;
	}
	InterlockedExchange(&s_vcpu_running, 1);
	hr = WHvRunVirtualProcessor(s_partition, WHPX_VP_INDEX,
								&exit_context, sizeof(exit_context));
	InterlockedExchange(&s_vcpu_running, 0);
	DeleteTimerQueueTimer(NULL, timer, INVALID_HANDLE_VALUE);
	if (FAILED(hr)) return WHPX_RUN_FAILED;
	switch (exit_context.ExitReason) {
	case WHvRunVpExitReasonX64IoPortAccess:
		memset(&emulator_status, 0, sizeof(emulator_status));
		hr = WHvEmulatorTryIoEmulation(s_emulator, NULL,
				&exit_context.VpContext, &exit_context.IoPortAccess,
				&emulator_status);
		return (SUCCEEDED(hr) && emulator_status.EmulationSuccessful) ?
			WHPX_RUN_CONTINUE : WHPX_RUN_UNSUPPORTED;

	case WHvRunVpExitReasonMemoryAccess:
		memset(&emulator_status, 0, sizeof(emulator_status));
		hr = WHvEmulatorTryMmioEmulation(s_emulator, NULL,
				&exit_context.VpContext, &exit_context.MemoryAccess,
				&emulator_status);
		return (SUCCEEDED(hr) && emulator_status.EmulationSuccessful) ?
			WHPX_RUN_CONTINUE : WHPX_RUN_UNSUPPORTED;

	case WHvRunVpExitReasonX64InterruptWindow:
		s_interrupt_window_ready = 1;
		s_interrupt_window_requested = 0;
		return WHPX_RUN_CONTINUE;

	case WHvRunVpExitReasonX64MsrAccess:
		return whpx_handle_msr_access(&exit_context);

	case WHvRunVpExitReasonX64Halt:
		return WHPX_RUN_HALT;
	case WHvRunVpExitReasonCanceled:
		return WHPX_RUN_CANCELED;
	default:
		return WHPX_RUN_UNSUPPORTED;
	}
}

UINT8 i386whpxfunc_interrupt(UINT8 vector)
{
	WHV_REGISTER_VALUE value;
	WHV_REGISTER_NAME name;
	HRESULT hr;

	if (!s_partition || !s_vcpu_created) return FAILURE;

	/*
	 * HAXM exposes ready_for_interrupt_injection/request_interrupt_window.
	 * Use WHPX deliverability notification for the same handshake instead of
	 * guessing readiness from IF and interrupt-shadow state.
	 */
	if (!s_interrupt_window_ready) {
		if (!s_interrupt_window_requested) {
			memset(&value, 0, sizeof(value));
			value.DeliverabilityNotifications.InterruptNotification = 1;
			name = WHvX64RegisterDeliverabilityNotifications;
			hr = WHvSetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
				&name, 1, &value);
			if (FAILED(hr)) {
				TRACEOUT(("WHPX: interrupt-window request failed hr=%08x",
					(UINT32)hr));
				return FAILURE;
			}
			s_interrupt_window_requested = 1;
		}
		return FAILURE;
	}

	memset(&value, 0, sizeof(value));
	value.PendingInterruption.InterruptionPending = 1;
	value.PendingInterruption.InterruptionType = WHvX64PendingInterrupt;
	value.PendingInterruption.InterruptionVector = vector;
	name = WHvRegisterPendingInterruption;
	hr = WHvSetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
		&name, 1, &value);
	if (FAILED(hr)) {
		TRACEOUT(("WHPX: interrupt inject failed vector=%02x hr=%08x",
			vector, (UINT32)hr));
		s_interrupt_window_ready = 0;
		s_interrupt_window_requested = 0;
		return FAILURE;
	}

	s_interrupt_window_ready = 0;
	return SUCCESS;
}

UINT8 i386whpxfunc_set_registers(const WHPX_CPU_STATE *state)
{
	WHV_REGISTER_VALUE values[sizeof(s_cpu_registers) / sizeof(s_cpu_registers[0])];
	unsigned i;
	memset(values, 0, sizeof(values));
	for (i = 0; i < 16; i++) values[i].Reg64 = state->regs[i];
	values[16].Reg64 = state->rip;
	values[17].Reg64 = state->rflags;
	whpx_segment_to_whv(&state->cs, &values[18].Segment);
	whpx_segment_to_whv(&state->ss, &values[19].Segment);
	whpx_segment_to_whv(&state->ds, &values[20].Segment);
	whpx_segment_to_whv(&state->es, &values[21].Segment);
	whpx_segment_to_whv(&state->fs, &values[22].Segment);
	whpx_segment_to_whv(&state->gs, &values[23].Segment);
	whpx_segment_to_whv(&state->ldt, &values[24].Segment);
	whpx_segment_to_whv(&state->tr, &values[25].Segment);
	values[26].Table.Base = state->gdt.base;
	values[26].Table.Limit = (UINT16)state->gdt.limit;
	values[27].Table.Base = state->idt.base;
	values[27].Table.Limit = (UINT16)state->idt.limit;
	values[28].Reg64 = state->cr0; values[29].Reg64 = state->cr2;
	values[30].Reg64 = state->cr3; values[31].Reg64 = state->cr4;
	values[32].Reg64 = state->dr0; values[33].Reg64 = state->dr1;
	values[34].Reg64 = state->dr2; values[35].Reg64 = state->dr3;
	values[36].Reg64 = state->dr6; values[37].Reg64 = state->dr7;
	values[38].Reg64 = state->efer;
	values[39].Reg64 = state->sysenter_cs;
	values[40].Reg64 = state->sysenter_eip;
	values[41].Reg64 = state->sysenter_esp;
	return SUCCEEDED(WHvSetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
		s_cpu_registers, (UINT32)(sizeof(s_cpu_registers) / sizeof(s_cpu_registers[0])),
		values)) ? SUCCESS : FAILURE;
}

UINT8 i386whpxfunc_get_registers(WHPX_CPU_STATE *state)
{
	WHV_REGISTER_VALUE values[sizeof(s_cpu_registers) / sizeof(s_cpu_registers[0])];
	unsigned i;
	if (FAILED(WHvGetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
		s_cpu_registers, (UINT32)(sizeof(s_cpu_registers) / sizeof(s_cpu_registers[0])),
		values))) return FAILURE;
	memset(state, 0, sizeof(*state));
	for (i = 0; i < 16; i++) state->regs[i] = values[i].Reg64;
	state->rip = values[16].Reg64;
	state->rflags = values[17].Reg64;
	whpx_segment_from_whv(&values[18].Segment, &state->cs);
	whpx_segment_from_whv(&values[19].Segment, &state->ss);
	whpx_segment_from_whv(&values[20].Segment, &state->ds);
	whpx_segment_from_whv(&values[21].Segment, &state->es);
	whpx_segment_from_whv(&values[22].Segment, &state->fs);
	whpx_segment_from_whv(&values[23].Segment, &state->gs);
	whpx_segment_from_whv(&values[24].Segment, &state->ldt);
	whpx_segment_from_whv(&values[25].Segment, &state->tr);
	state->gdt.base = values[26].Table.Base; state->gdt.limit = values[26].Table.Limit;
	state->idt.base = values[27].Table.Base; state->idt.limit = values[27].Table.Limit;
	state->cr0 = values[28].Reg64; state->cr2 = values[29].Reg64;
	state->cr3 = values[30].Reg64; state->cr4 = values[31].Reg64;
	state->dr0 = values[32].Reg64; state->dr1 = values[33].Reg64;
	state->dr2 = values[34].Reg64; state->dr3 = values[35].Reg64;
	state->dr6 = values[36].Reg64; state->dr7 = values[37].Reg64;
	state->efer = (UINT32)values[38].Reg64;
	state->sysenter_cs = (UINT32)values[39].Reg64;
	state->sysenter_eip = values[40].Reg64;
	state->sysenter_esp = values[41].Reg64;
	return SUCCESS;
}

UINT8 i386whpxfunc_set_fpu(const WHPX_FPU_STATE *state)
{
	WHV_REGISTER_VALUE values[sizeof(s_fpu_registers) / sizeof(s_fpu_registers[0])];
	unsigned i;
	memset(values, 0, sizeof(values));
	for (i = 0; i < 8; i++) memcpy(&values[i], state->fp_mmx[i], 16);
	for (i = 0; i < 16; i++) memcpy(&values[8 + i], state->xmm[i], 16);
	values[24].FpControlStatus.FpControl = state->control;
	values[24].FpControlStatus.FpStatus = state->status;
	values[24].FpControlStatus.FpTag = state->tag;
	values[24].FpControlStatus.LastFpOp = state->opcode;
	values[24].FpControlStatus.LastFpRip = state->instruction_pointer;
	values[25].XmmControlStatus.LastFpRdp = state->data_pointer;
	values[25].XmmControlStatus.XmmStatusControl = state->mxcsr;
	values[25].XmmControlStatus.XmmStatusControlMask = state->mxcsr_mask ?
		state->mxcsr_mask : 0xffff;
	return SUCCEEDED(WHvSetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
		s_fpu_registers, (UINT32)(sizeof(s_fpu_registers) / sizeof(s_fpu_registers[0])),
		values)) ? SUCCESS : FAILURE;
}

UINT8 i386whpxfunc_get_fpu(WHPX_FPU_STATE *state)
{
	WHV_REGISTER_VALUE values[sizeof(s_fpu_registers) / sizeof(s_fpu_registers[0])];
	unsigned i;
	if (FAILED(WHvGetVirtualProcessorRegisters(s_partition, WHPX_VP_INDEX,
		s_fpu_registers, (UINT32)(sizeof(s_fpu_registers) / sizeof(s_fpu_registers[0])),
		values))) return FAILURE;
	memset(state, 0, sizeof(*state));
	for (i = 0; i < 8; i++) memcpy(state->fp_mmx[i], &values[i], 16);
	for (i = 0; i < 16; i++) memcpy(state->xmm[i], &values[8 + i], 16);
	state->control = values[24].FpControlStatus.FpControl;
	state->status = values[24].FpControlStatus.FpStatus;
	state->tag = values[24].FpControlStatus.FpTag;
	state->opcode = values[24].FpControlStatus.LastFpOp;
	state->instruction_pointer = values[24].FpControlStatus.LastFpRip;
	state->data_pointer = values[25].XmmControlStatus.LastFpRdp;
	state->mxcsr = values[25].XmmControlStatus.XmmStatusControl;
	state->mxcsr_mask = values[25].XmmControlStatus.XmmStatusControlMask;
	return SUCCESS;
}

#endif /* SUPPORT_IA32_WHPX */
