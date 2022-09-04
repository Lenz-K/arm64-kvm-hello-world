ENTRY(_start)

MEMORY
{
    RAM ( rxw ) : ORIGIN = 0x04000000, LENGTH = 0x02000000
}

SECTIONS
{
    . = 0x0;
    .startup . : { startup.o(.text) }
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss COMMON) }
    . = ALIGN(16);
    . = 0x04020000;
    stack_top = .;
}

