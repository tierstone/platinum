.global efi_main
.global arch_halt_forever

efi_main:
    subq $40, %rsp
    call kernel_main
    addq $40, %rsp
    jmp arch_halt_forever

arch_halt_forever:
    cli
1:
    hlt
    jmp 1b
