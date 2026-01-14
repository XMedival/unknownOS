[bits 32]

global gdt_flush
gdt_flush:
    ; Args: (ignored, code_sel, data_sel)
    mov eax, [esp + 12]   ; data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS
    ; We need to push the code selector and return address, then retf
    mov eax, [esp + 8]    ; code selector
    push eax
    push .reload_cs
    retf

.reload_cs:
    ret
