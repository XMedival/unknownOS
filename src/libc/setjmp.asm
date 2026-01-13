; setjmp/longjmp implementation for x86
; jmp_buf layout: [ebx, esi, edi, ebp, esp, eip] (6 x 4 bytes = 24 bytes)

section .text

; int setjmp(jmp_buf env)
; Returns 0 on direct call, val from longjmp otherwise
global setjmp
setjmp:
    mov eax, [esp + 4]      ; eax = env (pointer to jmp_buf)

    ; Save callee-saved registers
    mov [eax + 0], ebx      ; env[0] = ebx
    mov [eax + 4], esi      ; env[1] = esi
    mov [eax + 8], edi      ; env[2] = edi
    mov [eax + 12], ebp     ; env[3] = ebp

    ; Save stack pointer (after return)
    lea ecx, [esp + 4]      ; ecx = esp after setjmp returns
    mov [eax + 16], ecx     ; env[4] = esp

    ; Save return address
    mov ecx, [esp]          ; ecx = return address
    mov [eax + 20], ecx     ; env[5] = eip

    ; Return 0
    xor eax, eax
    ret

; void longjmp(jmp_buf env, int val)
; Never returns - jumps back to setjmp call site
global longjmp
longjmp:
    mov edx, [esp + 4]      ; edx = env (pointer to jmp_buf)
    mov eax, [esp + 8]      ; eax = val (return value)

    ; If val is 0, make it 1 (setjmp can't return 0 from longjmp)
    test eax, eax
    jnz .nonzero
    inc eax
.nonzero:

    ; Restore callee-saved registers
    mov ebx, [edx + 0]      ; ebx = env[0]
    mov esi, [edx + 4]      ; esi = env[1]
    mov edi, [edx + 8]      ; edi = env[2]
    mov ebp, [edx + 12]     ; ebp = env[3]

    ; Restore stack pointer
    mov esp, [edx + 16]     ; esp = env[4]

    ; Jump to saved return address (as if returning from setjmp)
    jmp [edx + 20]          ; jump to env[5]
