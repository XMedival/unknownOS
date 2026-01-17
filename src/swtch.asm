[bits 64]

; 64-bit Context switch
;
;   void swtch(struct context **old, struct context *new);
;
; Save the current registers on the stack, creating a struct context,
; and save its address in *old. Switch stacks to new and pop previously-
; saved registers.
;
; System V AMD64 ABI callee-saved registers: rbx, rbp, r12-r15
; Arguments: rdi = old, rsi = new

global swtch
swtch:
    ; Save callee-saved registers on old stack
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Switch stacks
    mov [rdi], rsp      ; *old = rsp (save old stack pointer)
    mov rsp, rsi        ; rsp = new (load new stack pointer)

    ; Load new callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret                 ; Return to new context's saved rip
