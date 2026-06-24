# RISC-V SoC Tetris - Hata Ayıklama ve Geliştirme Kılavuzu

Bu belge, projenin mevcut durumunu, alınan hataların nedenlerini ve projeyi devralacak kişinin (veya Claude'un) düzeltmesi gereken eksikleri listelemek için hazırlanmıştır.

## Mevcut Durum Özeti

- **Donanım Sentezi:** Chisel kodları Verilog'a çevrildi ve Vivado üzerinden başarıyla sentezlenerek bitstream (`TopWrapper.bit`) üretildi.
- **Saat (Clock) Uyumu:** HDMI zamanlaması için gerekli olan piksel saati (25 MHz) ve serileştirici saati (125 MHz) MMCM ile üretildi. Faz kaymasını önlemek için saat sinyali standart Xilinx `ODDR` modülü üzerinden HDMI portuna yönlendirildi. DRC hataları giderildi.
- **Yazılım:** Tetris oyunu C dilinde yazıldı, `riscv-none-elf-gcc` ile derlendi ve `program.hex` dosyasına dönüştürüldü.
- **Hafıza Başlatma:** `SoCTop.sv` içerisindeki `$readmemh` yolu Vivado'nun çalışma dizini kısıtlamalarına takılmaması için doğrudan `"program.hex"` olarak düzeltildi ve Vivado ROM'u başarıyla initialize etti.

**Ancak kart üzerinde butonlara basıldığında LED'ler yanmıyor ve HDMI ekranı siyah çıkıyor.**

---

## 🛑 Neden Çalışmıyor? (Tespit Edilen Kritik Buglar)

Claude ile çalışırken aşağıdaki 2 ana maddeyi mutlaka çözmeniz gerekiyor:

### 1. C Startup Code (crt0) Eksikliği (.data ve .bss başlatılmıyor)
Yazılımın derlendiği `boot.S` dosyasında sadece stack pointer (`sp`) ayarlanıp `main` fonksiyonuna zıplanıyor. 
Ancak `link.ld` dosyasında başlangıç değeri verilmiş global değişkenler (`.data`) ROM'da saklanıyor ve çalışma anında RAM'e (`0x4000` adresi) taşınması gerekiyor. Ayrıca `.bss` (sıfır olarak başlatılan değişkenler) bölgesinin sıfırlanması gerekiyor.

**Sorun:** `boot.S` içerisinde `.data` bölümünü ROM'dan RAM'e kopyalayan ve `.bss` bölümünü sıfırlayan assembly döngüleri yok! 
**Sonuç:** `game_state`, `current_x`, `board` gibi tüm global değişkenler RAM'den rastgele çöp (garbage) değerler okuyor. Bu da oyunun sonsuz döngüye girmesine veya daha ekrana hiçbir şey çizemeden çökmesine neden oluyor.

**Çözüm Önerisi:** `boot.S` dosyasına standart bir C çalışma zamanı başlatıcısı (crt0) ekleyin:
```assembly
    # .data kopyalama
    la t0, _data_rom
    la t1, _data_start
    la t2, _data_end
copy_loop:
    bge t1, t2, clear_bss
    lw t3, 0(t0)
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j copy_loop

clear_bss:
    la t0, _bss_start
    la t1, _bss_end
bss_loop:
    bge t0, t1, start_main
    sw zero, 0(t0)
    addi t0, t0, 4
    j bss_loop
```
*(Not: Bunun için `link.ld` dosyasında `_data_rom`, `_data_start`, `_data_end` etiketlerinin tanımlanması gerekecektir.)*

### 2. Chisel Bellek Maskesi (Write Mask) ve Bayt Kaydırma Uyuşmazlığı
`Core.scala` dosyasında `SB` (Store Byte) komutu çalıştırıldığında, yazılacak bayt bellek adresine göre doğru konuma (örneğin bit `[23:16]`) kaydırılıyor (`shift` işlemi).
Ancak `HdmiController.scala` dosyasında Framebuffer'a yazma işlemi yapılırken bu kaydırma ve maskeleme (`writeMask`) **dikkate alınmıyor**.

```scala
// HdmiController.scala içindeki hatalı satır:
fbMem.write(writeAddr, io.writeData(7, 0))
```
**Sorun:** CPU `draw_pixel` ile adresi 4'e tam bölünmeyen bir piksele (örneğin adres `0x90000001`) yazmak istediğinde, CPU veriyi `writeData[15:8]` içine koyuyor. Ancak `HdmiController` her zaman `writeData[7:0]` okuduğu için Framebuffer'a `0x00` (siyah renk) yazıyor!
**Sonuç:** Ekrana çizilen piksellerin %75'i donanımsal bir bug yüzünden siyaha boyanıyor.
**Çözüm Önerisi:** Framebuffer modülüne `memWriteMask` sinyalini taşıyın ve 32-bitlik `writeData`'nın hangi baytının yazılması gerektiğini bu maskeye göre dinamik olarak seçin. Veya Framebuffer'ı `Vec(4, UInt(8.W))` yaparak RAM gibi maskeli yazmayı destekleyecek şekilde güncelleyin.

### 3. Buton Okuma (GPIO)
`soc.h` içinde butonlar `#define GPIO_BTNS ((volatile unsigned int*)0x80000004)` olarak tanımlı.
C kodu `btn = *GPIO_BTNS;` satırında `LW` (Load Word) komutunu kullanıyor. Ancak butonlar sadece 5 bitlik bir değer. Eğer donanım geri dönüşte diğer 27 biti sıfırlamıyorsa (zero-extension), C tarafında rastgele yüksek bitler 1 olarak algılanıp sürekli bir butona basılıyormuş gibi davranabilir. `SoCTop.scala` veya `GpioController` içinde okunan buton değerinin üst bitlerinin sıfırlandığından emin olun.

---

## Nasıl Devam Edilmeli?

Arkadaşınız veya Claude şu adımları izlemelidir:
1. `sw/boot.S` ve `sw/link.ld` dosyalarını `.data` ve `.bss` başlatmasını yapacak şekilde güncelleyin.
2. `src/main/scala/soc/HdmiController.scala` dosyasında maskeli byte okuma/yazma donanım hatasını düzeltin.
3. Düzenlemelerden sonra projeyi tekrar derlemek için:
   - `python compile.py` (C kodunu derler)
   - `sbt run` (Chisel kodunu Verilog'a çevirir)
   - Vivado üzerinden `vivado_build.tcl` scriptini çalıştırarak yeni bitstream'i sentezleyin.

Her iki hata da çözüldüğünde LED'ler butonlara tepki verecek ve monitörde Tetris oyunu renkleriyle beraber sorunsuz görüntülenecektir.
