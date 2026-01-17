[bits 32]

; Simple init program that calls exit(42)

section .text
global _start

_start:
    ; Test: write "Hello from userspace!\n" to stdout
    ; sys_write(1, msg, len)
    mov eax, 2          ; SYS_write
    mov ebx, 1          ; fd = stdout
    mov ecx, msg        ; buffer
    mov edx, msglen     ; length
    int 64              ; syscall

    ; mov eax, 3
    ; int 64

    ; Exit with code 42
    mov eax, 1          ; SYS_exit
    mov ebx, 42         ; exit code
    int 64              ; syscall

    ; Should never reach here
    jmp $

section .rodata
msg: db "Hello from userspace!", 10
msglen equ $ - msg
