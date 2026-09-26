/* GNU ARM GCC linker script for the STM32L4R5ZIT6 "nucleo-l4r5" board
 * (NUCLEO-L4R5ZI).
 *
 *   FLASH : 2048 KB @ 0x08000000
 *   SRAM1 :  192 KB @ 0x20000000
 *   SRAM2 :   64 KB @ 0x10000000
 *   SRAM3 :  384 KB @ 0x20040000   (main data + stack region)
 *
 * Symbols consumed by startup_stm32l4r5xx.s:
 *   _estack, _sidata, _sdata, _edata, _sbss, _ebss
 * Symbols consumed by syscalls.c (_sbrk): end / _end
 */

ENTRY(Reset_Handler)

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 192K   /* SRAM1 */
  RAM2  (xrw) : ORIGIN = 0x10000000, LENGTH = 64K    /* SRAM2 */
  RAM3  (xrw) : ORIGIN = 0x20040000, LENGTH = 384K   /* SRAM3 */
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
}

_estack        = ORIGIN(RAM3) + LENGTH(RAM3);  /* top of SRAM3, stack grows down */
_Min_Heap_Size = 0x400;
_Min_Stack_Size = 0x1000;

/* RAM-execution region defaults. Projects that run benchmark kernels from a
 * RAM region override these by placing code in a `.ram_code` output section
 * (their own linker script). For all normal projects the symbols are 0, so
 * the startup copy-in loop treats them as empty (no-op). */
PROVIDE(_sram_code = 0);
PROVIDE(_eram_code = 0);
PROVIDE(_sram_code_idata = 0);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    __data_start__ = _sdata;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
    __data_end__ = _edata;
  } >RAM3 AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM3

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM3

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
