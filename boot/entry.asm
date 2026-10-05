BITS 32
section .multiboot
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32
    dw 0
    dw 0
    dd 8
header_end:

section .text
extern kmain
extern boot_stack_top
global _start
_start:
    cli
    mov esp, boot_stack_top
    push ebx
    push eax
    call kmain
.hang:
    hlt
    jmp .hang

section .bss
align 16
boot_stack: resb 16384
boot_stack_top:
