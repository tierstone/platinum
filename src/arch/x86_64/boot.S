.global efi_main
.global arch_load_gdt
.global arch_load_idt
.global arch_load_cr3
.global arch_enable_interrupts
.global arch_task_enter
.global arch_enter_user
.global arch_halt_forever
.global arch_isr_invalid_opcode
.global arch_isr_general_protection
.global arch_isr_page_fault
.global arch_irq0_entry
.global arch_syscall_entry
.global arch_load_tr
.global arch_user_test_entry

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

arch_load_idt:
    lidt (%rcx)
    ret

arch_load_cr3:
    movq %rcx, %cr3
    ret

arch_enable_interrupts:
    sti
    ret

arch_load_tr:
    ltr %cx
    ret

arch_task_enter:
    movq %rcx, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq

arch_enter_user:
    pushq $0x1b
    pushq %rdx
    pushfq
    pushq $0x23
    pushq %rcx
    iretq

arch_halt_forever:
    cli
1:
    hlt
    jmp 1b

arch_isr_invalid_opcode:
    movl $6, %ecx
    subq $40, %rsp
    call kernel_trap
    addq $40, %rsp
    jmp arch_halt_forever

arch_isr_general_protection:
    popq %rdx
    movl $13, %ecx
    subq $40, %rsp
    call kernel_trap_error
    addq $40, %rsp
    jmp arch_halt_forever

arch_isr_page_fault:
    popq %rdx
    movl $14, %ecx
    subq $40, %rsp
    call kernel_trap_error
    addq $40, %rsp
    jmp arch_halt_forever

arch_irq0_entry:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rcx
    subq $40, %rsp
    call kernel_timer_tick
    addq $40, %rsp
    movq %rax, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq

arch_syscall_entry:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rcx
    subq $40, %rsp
    call kernel_syscall_entry
    addq $40, %rsp
    movq %rax, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq

arch_user_test_entry:
    movl $'U', %edi
    movl $0, %eax
    int $0x80
    movl $2, %eax
    int $0x80
    movq %rax, %rbx
    movl $4, %ecx
1:
    movl $2, %eax
    int $0x80
    cmpq %rbx, %rax
    je 1b
    movq %rax, %rbx
    movl $'u', %edi
    movl $0, %eax
    int $0x80
    subq $1, %rcx
    jne 1b
    movl $3, %eax
    int $0x80
2:
    jmp 2b
