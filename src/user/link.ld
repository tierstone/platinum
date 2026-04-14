ENTRY(user_init_main)

PHDRS
{
    text PT_LOAD FLAGS(5);
}

SECTIONS
{
    . = 0x02100000;

    .text : ALIGN(0x1000)
    {
        *(.text*)
        *(.rodata*)
    } :text

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
        *(.eh_frame_hdr*)
        *(.llvm_addrsig)
    }
}
