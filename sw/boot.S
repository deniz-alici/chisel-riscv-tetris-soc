.section .text.init
.global _start

_start:
    # Stack pointer'ı RAM'in en üst adresine konumlandırıyoruz (0x00008000)
    # Yığın (stack) aşağıya doğru büyüyecek
    la sp, _stack_top

    # C dilindeki main fonksiyonuna zıplıyoruz
    jal ra, main

    # Eğer program main'den çıkarsa sonsuz döngüye gir
halt:
    j halt
