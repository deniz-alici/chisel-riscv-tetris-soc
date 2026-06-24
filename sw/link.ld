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

  /* Salt Okunur Sabitler (ROM) */
  .rodata : {
    *(.rodata)
    *(.rodata.*)
  } > rom

  /* Başlangıç değeri atanmış değişkenler (ROM'da saklanır, RAM'e kopyalanmazsa doğrudan ROM'dan da okunabilir) */
  .data : {
    *(.data)
    *(.data.*)
  } > ram AT > rom

  /* Başlangıç değeri atanmamış değişkenler (BSS - RAM) */
  .bss : {
    _bss_start = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    _bss_end = .;
  } > ram

  /* Yığın (Stack) Tepe Adresi (16 KB RAM'in sonu: 0x4000 + 16KB = 0x8000) */
  _stack_top = 0x00008000;
}
