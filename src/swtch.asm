[bits 32]

; Context switch
;
;   void swtch(struct context **old, struct context *new);
;
; Save the current registers on the stack, creating a struct context,
; and save its address in *old. Switch stacks to new and pop previously-
; saved registers.

global swtch
swtch:
    mov eax, [esp+4]    ; old (pointer to pointer)
    mov edx, [esp+8]    ; new (pointer to context)

    ; Save callee-saved registers on old stack
    push ebp
    push ebx
    push esi
    push edi

    ; Switch stacks
    mov [eax], esp      ; *old = esp (save old stack pointer)
    mov esp, edx        ; esp = new (load new stack pointer)

    ; Load new callee-saved registers
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                 ; Return to new context's saved eip
