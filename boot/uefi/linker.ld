OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0x0;
    ImageBase = .;

    .text : {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }
    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    }
    .data : {
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        *(.got)
        *(.got.plt)
        _DYNAMIC = .;
    }
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    .reloc : {
        *(.reloc)
    }
}