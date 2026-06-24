.section .text.init
.global _start

_start:
    # Stack pointer'ı RAM'in en üst adresine konumlandırıyoruz (0x00008000)
    # Yığın (stack) aşağıya doğru büyüyecek
    la sp, _stack_top

    # ============================================================
    # crt0: C çalışma zamanı başlatması
    # ============================================================

    # --- 1) .data bölümünü ROM'dan RAM'e kopyala ---
    # Kaynak: _data_rom (ROM/LMA), Hedef: _data_start.._data_end (RAM/VMA)
    la   t0, _data_rom       # kaynak işaretçisi (ROM)
    la   t1, _data_start     # hedef işaretçisi (RAM)
    la   t2, _data_end       # hedef bitiş adresi
copy_data:
    bge  t1, t2, zero_bss    # hedef >= bitiş ise kopyalama tamam
    lw   t3, 0(t0)           # ROM'dan bir word oku
    sw   t3, 0(t1)           # RAM'e yaz
    addi t0, t0, 4
    addi t1, t1, 4
    j    copy_data

    # --- 2) .bss bölümünü sıfırla ---
zero_bss:
    la   t0, _bss_start
    la   t1, _bss_end
clear_bss:
    bge  t0, t1, run_main    # başlangıç >= bitiş ise sıfırlama tamam
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    clear_bss

    # --- 3) C main fonksiyonuna geç ---
run_main:
    jal ra, main

    # Eğer program main'den çıkarsa sonsuz döngüye gir
halt:
    j halt
