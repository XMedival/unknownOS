[bits 64]
[extern trap]

; Kernel data segment selector (SEG_KDATA << 3)
%define SEG_KDATA_SEL 0x10

; Macro for exceptions WITHOUT error code pushed by CPU
%macro ISR_NOERRCODE 1
global vector%1
vector%1:
    push qword 0        ; dummy error code
    push qword %1       ; trap number
    jmp alltraps
%endmacro

; Macro for exceptions WITH error code pushed by CPU
%macro ISR_ERRCODE 1
global vector%1
vector%1:
    push qword %1       ; trap number (error code already pushed by CPU)
    jmp alltraps
%endmacro

; CPU Exceptions 0-21
ISR_NOERRCODE 0     ; #DE Divide Error
ISR_NOERRCODE 1     ; #DB Debug
ISR_NOERRCODE 2     ; NMI
ISR_NOERRCODE 3     ; #BP Breakpoint
ISR_NOERRCODE 4     ; #OF Overflow
ISR_NOERRCODE 5     ; #BR Bound Range Exceeded
ISR_NOERRCODE 6     ; #UD Invalid Opcode
ISR_NOERRCODE 7     ; #NM Device Not Available
ISR_ERRCODE   8     ; #DF Double Fault
ISR_NOERRCODE 9     ; Coprocessor Segment Overrun (reserved)
ISR_ERRCODE   10    ; #TS Invalid TSS
ISR_ERRCODE   11    ; #NP Segment Not Present
ISR_ERRCODE   12    ; #SS Stack-Segment Fault
ISR_ERRCODE   13    ; #GP General Protection Fault
ISR_ERRCODE   14    ; #PF Page Fault
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; #MF x87 FPU Error
ISR_ERRCODE   17    ; #AC Alignment Check
ISR_NOERRCODE 18    ; #MC Machine Check
ISR_NOERRCODE 19    ; #XM SIMD Floating-Point
ISR_NOERRCODE 20    ; #VE Virtualization Exception
ISR_ERRCODE   21    ; #CP Control Protection

; Reserved exceptions 22-31
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; Hardware IRQs 32-47 (remapped PIC)
ISR_NOERRCODE 32    ; IRQ0 - Timer
ISR_NOERRCODE 33    ; IRQ1 - Keyboard
ISR_NOERRCODE 34    ; IRQ2 - Cascade
ISR_NOERRCODE 35    ; IRQ3 - COM2
ISR_NOERRCODE 36    ; IRQ4 - COM1
ISR_NOERRCODE 37    ; IRQ5 - LPT2
ISR_NOERRCODE 38    ; IRQ6 - Floppy
ISR_NOERRCODE 39    ; IRQ7 - LPT1 / Spurious
ISR_NOERRCODE 40    ; IRQ8 - RTC
ISR_NOERRCODE 41    ; IRQ9 - Free
ISR_NOERRCODE 42    ; IRQ10 - Free
ISR_NOERRCODE 43    ; IRQ11 - Free
ISR_NOERRCODE 44    ; IRQ12 - PS/2 Mouse
ISR_NOERRCODE 45    ; IRQ13 - FPU
ISR_NOERRCODE 46    ; IRQ14 - Primary ATA
ISR_NOERRCODE 47    ; IRQ15 - Secondary ATA

; Syscall vector (64)
global vector64
vector64:
    push qword 0        ; dummy error code
    push qword 64       ; syscall number
    jmp alltraps

; Common trap handler
; Stack layout on entry (after our pushes):
;   SS, RSP, RFLAGS, CS, RIP (pushed by CPU)
;   Error code (pushed by CPU or us)
;   Trap number (pushed by us)
alltraps:
    ; Save general purpose registers (build trapframe)
    ; Order must match struct trapframe in x86.h
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; In 64-bit mode, we don't need to load segment registers
    ; The kernel uses a flat memory model

    ; Call C trap handler with trapframe pointer
    ; First argument (rdi) = pointer to trapframe
    mov rdi, rsp
    call trap

    ; Fall through to trapret

; Return from trap - also used by forkret for new processes
global trapret
trapret:
    ; Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove trap number and error code from stack
    add rsp, 16

    ; iretq pops: RIP, CS, RFLAGS, RSP, SS (always in 64-bit mode)
    iretq

; Entry point for new processes after first context switch
; The context->rip is set to forkret, which then falls through to trapret
global forkret
forkret:
    ; Stack pointer already points to trapframe
    ; Just jump to trapret to restore state and enter user mode
    jmp trapret
