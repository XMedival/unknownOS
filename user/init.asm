[bits 64]

; Simple init program for 64-bit mode
; Uses System V AMD64 calling convention for syscalls:
; syscall number in rax, args in rdi, rsi, rdx, r10, r8, r9

section .text
global _start

_start:
    ; Test: write "Hello from userspace!\n" to stdout
    ; sys_write(fd=1, buf=msg, len=msglen)
    mov rax, 2          ; SYS_write
    mov rdi, 1          ; fd = stdout
    lea rsi, [rel msg]  ; buffer (RIP-relative addressing)
    mov rdx, msglen     ; length
    int 64              ; syscall via interrupt

    ; Exit with code 42
    ; sys_exit(status=42)
    mov rax, 1          ; SYS_exit
    mov rdi, 42         ; exit code
    int 64              ; syscall

    ; Should never reach here
.hang:
    jmp .hang

section .rodata
msg: db "Hello from userspace!", 10
msglen equ $ - msg
