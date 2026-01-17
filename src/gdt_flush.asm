[bits 64]

; 64-bit GDT segment reload
; void gdt_flush(uint64_t unused, uint64_t code_sel, uint64_t data_sel);
;
; Arguments (System V AMD64 ABI):
;   rdi = unused (was gdtr_ptr in 32-bit, not needed anymore)
;   rsi = code_sel
;   rdx = data_sel

global gdt_flush
gdt_flush:
    ; Load data segments
    mov ax, dx          ; data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS
    ; In 64-bit mode, we can't use retf the same way
    ; Instead, use a far return with the stack set up properly
    pop rax             ; Pop return address
    push rsi            ; Push code selector
    push rax            ; Push return address
    retfq               ; Far return to reload CS

; Note: In 64-bit mode, segment registers (except FS/GS) are largely ignored
; by the CPU for base/limit calculations. They still need valid selectors
; for privilege level checks.
