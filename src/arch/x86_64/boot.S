.global efi_main
efi_main:
    subq $48, %rsp
    andq $-16, %rsp
    call kernel_main
    cli
1:  hlt
    jmp 1b
