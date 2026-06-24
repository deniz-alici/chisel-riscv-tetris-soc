MEMORY
{
  rom (rx)  : ORIGIN = 0x00000000, LENGTH = 16K
  ram (rwx) : ORIGIN = 0x00004000, LENGTH = 16K
}

SECTIONS
{
  /* Kod Bölümü (ROM) */
  .text : {
    _text_start = .;
    *(.text.init)      /* boot.S başlangıç kodunun en başta olmasını garanti eder */
    *(.text)
    *(.text.*)
    _text_end = .;
  } > rom

  /* Salt Okunur Sabitler (ROM). RISC-V küçük salt-okunur veriyi .srodata'ya koyar. */
  .rodata : {
    *(.rodata)
    *(.rodata.*)
    *(.srodata)
    *(.srodata.*)
  } > rom

  /* Başlangıç değeri atanmış değişkenler:
     VMA = RAM (çalışma anında buradan okunur), LMA = ROM (görüntü burada saklanır).
     boot.S içindeki crt0, _data_rom -> _data_start arası baytları RAM'e kopyalar. */
  .data : ALIGN(4) {
    _data_start = .;
    *(.data)
    *(.data.*)
    *(.sdata)       /* RISC-V küçük başlangıç değerli globaller (current_x, rand_state...) */
    *(.sdata.*)
    . = ALIGN(4);
    _data_end = .;
  } > ram AT > rom
  /* .data bölümünün ROM'daki yükleme adresi (kopyalama kaynağı) */
  _data_rom = LOADADDR(.data);

  /* Başlangıç değeri atanmamış değişkenler (BSS - RAM) */
  .bss : ALIGN(4) {
    _bss_start = .;
    *(.sbss)        /* RISC-V küçük sıfır-init globaller (game_state, score, current_y...) */
    *(.sbss.*)
    *(.scommon)
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > ram

  /* Yığın (Stack) Tepe Adresi (16 KB RAM'in sonu: 0x4000 + 16KB = 0x8000) */
  _stack_top = 0x00008000;
}
