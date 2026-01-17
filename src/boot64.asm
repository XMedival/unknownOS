; boot64.asm - 32-bit to 64-bit (long mode) transition
; GRUB multiboot2 loads us in 32-bit protected mode
; We set up paging and switch to 64-bit long mode

[bits 32]

; Multiboot2 passes magic in EAX and info pointer in EBX
global _start32
extern _start

; Page table constants
PAGE_PRESENT    equ 0x01
PAGE_WRITE      equ 0x02
PAGE_SIZE_2MB   equ 0x80      ; PS bit for 2MB pages

; MSR numbers
MSR_EFER        equ 0xC0000080
EFER_LME        equ 0x100     ; Long Mode Enable

section .bss
align 4096
; Reserve space for page tables (identity map first 4GB using 2MB pages)
pml4:   resb 4096             ; Page Map Level 4
pdpt:   resb 4096             ; Page Directory Pointer Table
pd:     resb 4096 * 4         ; Page Directories (4 for 4GB)

; 64-bit stack
align 16
stack_bottom:
    resb 16384                ; 16KB stack
stack_top:

section .data
; Saved multiboot info (from 32-bit mode)
saved_magic: dd 0
saved_mbi:   dd 0

; 64-bit GDT
align 16
gdt64:
    dq 0                      ; Null descriptor
.code: equ $ - gdt64
    dq 0x00AF9A000000FFFF     ; 64-bit code: base=0, limit=0xFFFFF, P=1, DPL=0, S=1, type=0xA (exec/read), L=1 (64-bit), G=1
.data: equ $ - gdt64
    dq 0x00CF92000000FFFF     ; 64-bit data: base=0, limit=0xFFFFF, P=1, DPL=0, S=1, type=0x2 (read/write), G=1
.end:

gdt64_ptr:
    dw gdt64.end - gdt64 - 1  ; Limit
    dq gdt64                  ; Base (will be fixed up to 64-bit address)

section .text
_start32:
    ; Save multiboot info (EAX=magic, EBX=info ptr)
    ; Store in memory since transitioning to 64-bit corrupts upper bits
    mov [saved_magic], eax
    mov [saved_mbi], ebx

    ; Disable interrupts
    cli

    ; Set up page tables for identity mapping first 4GB
    ; Using 2MB pages for simplicity

    ; Clear page tables
    mov edi, pml4
    xor eax, eax
    mov ecx, (4096 * 6) / 4   ; Clear all page table memory
    rep stosd

    ; Set up PML4[0] -> PDPT
    mov edi, pml4
    mov eax, pdpt
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [edi], eax

    ; Set up PDPT[0-3] -> PD[0-3] (for 4GB)
    mov edi, pdpt
    mov eax, pd
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [edi], eax            ; PDPT[0] -> PD[0]
    add eax, 4096
    mov [edi + 8], eax        ; PDPT[1] -> PD[1]
    add eax, 4096
    mov [edi + 16], eax       ; PDPT[2] -> PD[2]
    add eax, 4096
    mov [edi + 24], eax       ; PDPT[3] -> PD[3]

    ; Set up PD entries (512 entries per PD, 2MB each = 1GB per PD)
    ; Map 4GB total (4 PDs x 512 entries x 2MB = 4GB)
    mov edi, pd
    mov eax, PAGE_PRESENT | PAGE_WRITE | PAGE_SIZE_2MB  ; 2MB page, present, writable
    mov ecx, 512 * 4          ; 512 entries * 4 PDs
.fill_pd:
    mov [edi], eax
    add eax, 0x200000         ; Next 2MB
    add edi, 8
    loop .fill_pd

    ; Load PML4 address into CR3
    mov eax, pml4
    mov cr3, eax

    ; Enable PAE (Physical Address Extension) in CR4
    mov eax, cr4
    or eax, 1 << 5            ; CR4.PAE
    mov cr4, eax

    ; Enable Long Mode in EFER MSR
    mov ecx, MSR_EFER
    rdmsr
    or eax, EFER_LME
    wrmsr

    ; Enable paging (this activates long mode since EFER.LME is set)
    mov eax, cr0
    or eax, 1 << 31           ; CR0.PG
    mov cr0, eax

    ; Load 64-bit GDT
    lgdt [gdt64_ptr]

    ; Far jump to 64-bit code segment
    jmp gdt64.code:_start64

[bits 64]
_start64:
    ; Now in 64-bit long mode!

    ; Set up 64-bit data segments
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack
    mov rsp, stack_top

    ; Clear direction flag
    cld

    ; Load arguments from saved locations (clear upper 32 bits)
    xor rdi, rdi
    xor rsi, rsi
    mov edi, [saved_magic]    ; First arg: magic (zero-extended to 64-bit)
    mov esi, [saved_mbi]      ; Second arg: mbi pointer (zero-extended)

    ; Call C entry point: _start(magic, mbi)
    call _start

    ; If _start returns, halt
.halt:
    cli
    hlt
    jmp .halt
