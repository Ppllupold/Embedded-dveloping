MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 96K
}

/* Top of RAM -- the stack starts here and grows downward. Referenced
 * by name in startup.c to seed the vector table's initial SP value. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* Must be the very first thing in FLASH -- the CPU reads entry 0
   * (initial SP) and entry 1 (Reset_Handler address) directly from
   * address 0x08000000 at power-on. KEEP() stops the linker from
   * discarding it as "unused" (nothing else in the program calls into
   * it directly, so it would otherwise look like dead code). */
  .isr_vector : {
    KEEP(*(.isr_vector))
  } > FLASH

  .text : { *(.text*) } > FLASH

  .data : {
    _data_start = .;
    *(.data*)
    _data_end = .;
  } > RAM AT> FLASH

  /* LOADADDR() gives the actual FLASH address where .data's initial
   * values physically live -- this is what Reset_Handler copies FROM. */
  _data_load_start = LOADADDR(.data);

  .bss : {
    _bss_start = .;
    *(.bss*)
    _bss_end = .;
  } > RAM
}
