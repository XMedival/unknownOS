[bits 32]
[extern trap]

; Kernel data segment selector (SEG_KDATA << 3)
%define SEG_KDATA_SEL 0x10

; Macro for exceptions WITHOUT error code pushed by CPU
%macro ISR_NOERRCODE 1
global vector%1
vector%1:
    push dword 0        ; dummy error code
    push dword %1       ; trap number
    jmp alltraps
%endmacro

; Macro for exceptions WITH error code pushed by CPU
%macro ISR_ERRCODE 1
global vector%1
vector%1:
    push dword %1       ; trap number (error code already pushed by CPU)
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
    push dword 0        ; dummy error code
    push dword 64       ; syscall number
    jmp alltraps

; Common trap handler
alltraps:
    ; Save segment registers (reverse order so ds is at lowest address)
    push gs
    push fs
    push es
    push ds

    ; Save general purpose registers
    pushad

    ; Load kernel data segment
    mov ax, SEG_KDATA_SEL
    mov ds, ax
    mov es, ax

    ; Call C trap handler with trapframe pointer
    push esp
    call trap
    add esp, 4

    ; Fall through to trapret

; Return from trap - also used by forkret for new processes
global trapret
trapret:
    ; Restore general purpose registers
    popad

    ; Restore segment registers
    pop ds
    pop es
    pop fs
    pop gs

    ; Remove trap number and error code from stack
    add esp, 8

    ; iret pops: EIP, CS, EFLAGS, and (if crossing rings) ESP, SS
    iret

; Entry point for new processes after first context switch
; The context->eip is set to forkret, which then falls through to trapret
global forkret
forkret:
    ; Stack pointer already points to trapframe
    ; Just jump to trapret to restore state and enter user mode
    jmp trapret
