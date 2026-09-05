; NP2IDLE.ASM
; ---------------------------------------------------------------------------
; NP2IDLE - Windows 9x idle HLT helper for Neko Project 21/W
;
; Build target:
;   Microsoft Windows 98 DDK + MASM 6.x
;
; WIN40COMPAT is defined before the DDK include files so that a VxD built
; with the Windows 98 DDK can still be loaded by Windows 95.
;
; Ring-3 control interface:
;   Device name: \\.\NP2IDLE0
;   IOCTL 1     : enable HLT idle handling
;   IOCTL 2     : disable HLT idle handling
;   IOCTL 3     : query state (DWORD 0/1 in output buffer)
;   IOCTL 4     : renew the temporary audio HLT-inhibit lease
;   IOCTL 5     : query audio inhibit state (DWORD 0/1)
;   IOCTL 6     : clear the current audio HLT-inhibit lease
;
; HLT is deferred out of Call_When_Idle by Call_Priority_VM_Event so that
; later idle callbacks are not blocked by NP2IDLE waiting for an interrupt.
; The tray application may renew a short audio inhibit lease while a WinMM
; WaveOut mixer line is active.  HLT is skipped until that lease expires.
;
; The device name deliberately has eight characters.  This avoids old
; Windows 95 CreateFile/VxD-name compatibility problems seen with some
; shorter DDB names.  The VxD file itself remains NP2IDLE.VXD.
; ---------------------------------------------------------------------------

.386p

WIN40COMPAT EQU 1

INCLUDE VMM.INC
INCLUDE VWIN32.INC

; Private DeviceIoControl codes.  0 is reserved for DIOC_OPEN/GETVERSION
; and -1 for DIOC_CLOSEHANDLE, so private commands begin at 1.
NP2IDLE_DIOC_ENABLE       EQU 1
NP2IDLE_DIOC_DISABLE      EQU 2
NP2IDLE_DIOC_GETSTATE     EQU 3
NP2IDLE_DIOC_AUDIO_BUSY   EQU 4
NP2IDLE_DIOC_GETAUDIO     EQU 5
NP2IDLE_DIOC_AUDIO_CLEAR   EQU 6

; The tray renews this lease while Windows reports an active WaveOut line.
; The lease is longer than the ring-3 polling interval so ordinary scheduler
; delay cannot briefly re-enable HLT during continuous playback.
NP2IDLE_AUDIO_INHIBIT_MS  EQU 1500

; Win32 error values returned in EAX by W32_DEVICEIOCONTROL.
NP2_ERR_INVALID_FUNCTION    EQU 1
NP2_ERR_INSUFFICIENT_BUFFER EQU 122

; Keep the ring-3-visible DDB/device name at exactly eight characters.
Declare_Virtual_Device NP2IDLE0, 1, 2, NP2IDLE_Control, \
                       Undefined_Device_ID, Undefined_Init_Order


; ---------------------------------------------------------------------------
; Locked data
; ---------------------------------------------------------------------------

VxD_LOCKED_DATA_SEG

        ALIGN 4
NP2IDLE_Enabled           dd 1        ; default: enabled
NP2IDLE_EventPending      dd 0        ; one deferred HLT event at most
NP2IDLE_AudioLastRenew    dd 0        ; Get_System_Time value of last audio lease
NP2IDLE_AudioInhibitValid dd 0        ; nonzero while the last-renew timestamp is valid

VxD_LOCKED_DATA_ENDS


; ---------------------------------------------------------------------------
; Locked code
; ---------------------------------------------------------------------------

VxD_LOCKED_CODE_SEG

BeginProc NP2IDLE_Control

        Control_Dispatch Device_Init,          NP2IDLE_Device_Init
        Control_Dispatch W32_DEVICEIOCONTROL, NP2IDLE_W32_DeviceIoControl

        clc
        ret

EndProc NP2IDLE_Control


; ---------------------------------------------------------------------------
; NP2IDLE_Idle_Callback
;
; Called by VMM when Windows and all VMs are idle.
;
; IMPORTANT:
;   Do NOT execute HLT directly in this callback.  Instead, schedule a
;   priority VM event with PEF_Always_Sched so that VMM cannot call the event
;   routine immediately from inside this callback.  We then return CF=1 and
;   allow all later idle callbacks to run before the deferred event becomes
;   eligible for execution.
;
; The idle callback only schedules work and returns.  HLT therefore cannot
; block callbacks that follow NP2IDLE in the current idle-handler chain.
; ---------------------------------------------------------------------------

BeginProc NP2IDLE_Idle_Callback

        cmp     dword ptr [NP2IDLE_Enabled], 0
        je      SHORT NP2IDLE_Idle_Done

        ; Only one deferred HLT event may be outstanding at a time.  Repeated
        ; idle callbacks while the event is pending simply continue the chain.
        cmp     dword ptr [NP2IDLE_EventPending], 0
        jne     SHORT NP2IDLE_Idle_Done

        mov     dword ptr [NP2IDLE_EventPending], 1

        ; Schedule the event for the System VM.  PEF_Always_Sched is the key:
        ; the callback must never run immediately inside Call_When_Idle.
        ; PEF_Wait_For_STI and PEF_Wait_Not_Crit additionally postpone it
        ; until the System VM has interrupts enabled and is not in a critical
        ; section/time-critical operation.
        xor     eax, eax                  ; no execution-priority boost
        VMMcall Get_Sys_VM_Handle          ; EBX = System VM handle
        mov     ecx, PEF_Wait_For_STI or PEF_Wait_Not_Crit or PEF_Always_Sched
        xor     edx, edx                  ; no reference data required
        mov     esi, OFFSET32 NP2IDLE_Deferred_Halt
        xor     edi, edi                  ; no timeout
        VMMcall Call_Priority_VM_Event

        ; With PEF_Always_Sched, ESI should contain an event handle.  If it is
        ; zero, do not leave the pending latch set forever.
        test    esi, esi
        jnz     SHORT NP2IDLE_Idle_Done
        mov     dword ptr [NP2IDLE_EventPending], 0

NP2IDLE_Idle_Done:
        stc                                ; continue idle-handler chain
        ret

EndProc NP2IDLE_Idle_Callback


; ---------------------------------------------------------------------------
; NP2IDLE_Deferred_Halt
;
; Deferred by Call_Priority_VM_Event from NP2IDLE_Idle_Callback.  Because the
; event was requested with PEF_Always_Sched, this routine is not called inline
; from the idle callback that scheduled it.
;
; Keep NP2IDLE_EventPending set until the HLT attempt is complete.  This
; prevents another idle callback from queueing a second deferred HLT while the
; first one is still active.
; ---------------------------------------------------------------------------

BeginProc NP2IDLE_Deferred_Halt

        cmp     dword ptr [NP2IDLE_Enabled], 0
        je      SHORT NP2IDLE_Deferred_Done

        ; Compare elapsed time since the most recent audio lease renewal.
        ; Unsigned subtraction works across the normal 32-bit clock wrap and
        ; does not depend on the sign bit of a long-running system uptime.
        cmp     dword ptr [NP2IDLE_AudioInhibitValid], 0
        je      NP2IDLE_Deferred_AudioClear
        VMMcall Get_System_Time
        sub     eax, dword ptr [NP2IDLE_AudioLastRenew]
        cmp     eax, NP2IDLE_AUDIO_INHIBIT_MS
        jb      NP2IDLE_Deferred_Done
        mov     dword ptr [NP2IDLE_AudioInhibitValid], 0
NP2IDLE_Deferred_AudioClear:

        ; PEF_Wait_For_STI describes the VM interrupt state.  Also verify the
        ; real ring-0 EFLAGS.IF immediately before HLT and never force STI.
        pushfd
        pop     eax
        test    eax, 00000200h             ; EFLAGS.IF
        jz      SHORT NP2IDLE_Deferred_Done

        hlt                                ; wake on the next interrupt

NP2IDLE_Deferred_Done:
        mov     dword ptr [NP2IDLE_EventPending], 0
        ret

EndProc NP2IDLE_Deferred_Halt


; ---------------------------------------------------------------------------
; NP2IDLE_W32_DeviceIoControl
;
; ESI -> DIOCParams (defined by VWIN32.INC)
;
; Successful W32_DEVICEIOCONTROL requests return ERROR_SUCCESS (EAX=0).
; Unsupported/bad requests return a Win32 error value in EAX.  Carry is
; cleared on return as required by the Win9x DeviceIoControl protocol.
; ---------------------------------------------------------------------------

BeginProc NP2IDLE_W32_DeviceIoControl

        ASSUME  esi:PTR DIOCParams

        ; DeviceIoControl normally supplies this pointer.  Clear it first
        ; when present, so commands with no output report zero bytes.
        mov     edx, [esi].lpcbBytesReturned
        test    edx, edx
        jz      SHORT NP2IDLE_DIOC_NoByteCount
        mov     dword ptr [edx], 0

NP2IDLE_DIOC_NoByteCount:
        mov     eax, [esi].dwIoControlCode

        ; CreateFile("\\\\.\\NP2IDLE0") probes the VxD with DIOC_OPEN.
        cmp     eax, DIOC_OPEN
        je      NP2IDLE_DIOC_Success

        ; CloseHandle generates this notification.  No per-handle state exists.
        cmp     eax, DIOC_CLOSEHANDLE
        je      NP2IDLE_DIOC_Success

        cmp     eax, NP2IDLE_DIOC_ENABLE
        je      NP2IDLE_is_DIOC_Enable

        cmp     eax, NP2IDLE_DIOC_DISABLE
        je      NP2IDLE_is_DIOC_Disable

        cmp     eax, NP2IDLE_DIOC_GETSTATE
        je      NP2IDLE_is_DIOC_GetState

        cmp     eax, NP2IDLE_DIOC_AUDIO_BUSY
        je      NP2IDLE_is_DIOC_AudioBusy

        cmp     eax, NP2IDLE_DIOC_GETAUDIO
        je      NP2IDLE_is_DIOC_GetAudio

        cmp     eax, NP2IDLE_DIOC_AUDIO_CLEAR
        je      NP2IDLE_is_DIOC_AudioClear

        mov     eax, NP2_ERR_INVALID_FUNCTION
        clc
        ret

NP2IDLE_is_DIOC_Enable:
        mov     dword ptr [NP2IDLE_Enabled], 1
        jmp     NP2IDLE_DIOC_Success

NP2IDLE_is_DIOC_Disable:
        mov     dword ptr [NP2IDLE_Enabled], 0
        jmp     NP2IDLE_DIOC_Success

NP2IDLE_is_DIOC_GetState:
        ; STATUS requires a DWORD output buffer.
        cmp     [esi].cbOutBuffer, 4
        jb      NP2IDLE_DIOC_SmallBuffer

        mov     edi, [esi].lpvOutBuffer
        test    edi, edi
        jz      NP2IDLE_DIOC_SmallBuffer

        mov     eax, [NP2IDLE_Enabled]
        and     eax, 1
        mov     dword ptr [edi], eax

        mov     edx, [esi].lpcbBytesReturned
        test    edx, edx
        jz      NP2IDLE_DIOC_Success
        mov     dword ptr [edx], 4
        jmp     NP2IDLE_DIOC_Success

NP2IDLE_is_DIOC_AudioBusy:
        ; Renew, rather than latch, the inhibit state.  If the ring-3 monitor
        ; disappears, normal HLT behavior automatically returns when the lease expires.
        VMMcall Get_System_Time
        mov     dword ptr [NP2IDLE_AudioLastRenew], eax
        mov     dword ptr [NP2IDLE_AudioInhibitValid], 1
        jmp     NP2IDLE_DIOC_Success

NP2IDLE_is_DIOC_AudioClear:
        mov     dword ptr [NP2IDLE_AudioInhibitValid], 0
        jmp     NP2IDLE_DIOC_Success

NP2IDLE_is_DIOC_GetAudio:
        cmp     [esi].cbOutBuffer, 4
        jb      NP2IDLE_DIOC_SmallBuffer

        mov     edi, [esi].lpvOutBuffer
        test    edi, edi
        jz      NP2IDLE_DIOC_SmallBuffer

        xor     ecx, ecx
        cmp     dword ptr [NP2IDLE_AudioInhibitValid], 0
        je      NP2IDLE_DIOC_AudioStateReady
        push    esi
        push    edi
        VMMcall Get_System_Time
        pop     edi
        pop     esi
        sub     eax, dword ptr [NP2IDLE_AudioLastRenew]
        cmp     eax, NP2IDLE_AUDIO_INHIBIT_MS
        jae     NP2IDLE_DIOC_AudioExpired
        inc     ecx
        jmp     NP2IDLE_DIOC_AudioStateReady
NP2IDLE_DIOC_AudioExpired:
        mov     dword ptr [NP2IDLE_AudioInhibitValid], 0
NP2IDLE_DIOC_AudioStateReady:
        mov     dword ptr [edi], ecx

        mov     edx, [esi].lpcbBytesReturned
        test    edx, edx
        jz      NP2IDLE_DIOC_Success
        mov     dword ptr [edx], 4
        jmp     NP2IDLE_DIOC_Success

NP2IDLE_DIOC_SmallBuffer:
        mov     eax, NP2_ERR_INSUFFICIENT_BUFFER
        clc
        ret

NP2IDLE_DIOC_Success:
        xor     eax, eax                  ; ERROR_SUCCESS
        clc
        ret

        ASSUME  esi:NOTHING

EndProc NP2IDLE_W32_DeviceIoControl

VxD_LOCKED_CODE_ENDS


; ---------------------------------------------------------------------------
; Initialization code
; ---------------------------------------------------------------------------

VxD_INIT_CODE_SEG

BeginProc NP2IDLE_Device_Init

        mov     esi, OFFSET32 NP2IDLE_Idle_Callback
        VMMcall Call_When_Idle

        ; Preserve Call_When_Idle's CF result.  Initialization fails if VMM
        ; could not register the callback.
        ret

EndProc NP2IDLE_Device_Init

VxD_INIT_CODE_ENDS

END
