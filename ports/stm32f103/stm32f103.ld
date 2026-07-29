ENTRY(Reset_Handler)

MEMORY
{
    /* 按最小的STM32F103C4配置，确保同一固件兼容C4及更大型号。 */
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 16K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 6K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

ASSERT(_ebss <= ORIGIN(RAM) + LENGTH(RAM), "RAM overflow")
