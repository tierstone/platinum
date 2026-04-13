.global efi_main
.global arch_load_gdt
.global arch_halt_forever

efi_main:
    subq $40, %rsp
    call kernel_main
    addq $40, %rsp
    jmp arch_halt_forever

arch_load_gdt:
    lgdt (%rcx)
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    movw %dx, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    ret

arch_halt_forever:
    cli
1:
    hlt
    jmp 1b
