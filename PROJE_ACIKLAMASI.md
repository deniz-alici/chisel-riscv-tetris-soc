# 🎮 RISC-V SoC Tetris Projesi — Baştan Sona Her Şeyin Açıklaması

> Bu döküman, bu projede ne yaptığımızı **hiçbir teknik bilgiye sahip olmayan birine** anlatır gibi baştan sona açıklar.

---

## 📖 Bu Proje Ne?

Bir cümleyle: **Kendi işlemcimizi sıfırdan tasarladık, üzerine Tetris oyunu yazdık ve bunu gerçek bir elektronik karta (FPGA) yükleyerek HDMI ile monitörde oynanabilir hale getirdik.**

Bunu biraz daha açalım:

- Bilgisayarların içinde bir **işlemci (CPU)** var — Intel, AMD gibi. Biz kendi küçük işlemcimizi sıfırdan tasarladık.
- Bu işlemci üzerinde çalışacak bir **Tetris oyunu** C dilinde yazdık.
- Sonra bu ikisini birleştirip bir **FPGA** adlı özel karta yükledik.
- Kartın HDMI çıkışını monitöre bağlayınca, ekranda Tetris oyunu görünüyor ve kartın butonlarıyla oynanabiliyor!

---

## 🤔 Temel Kavramlar (Basitçe)

### İşlemci (CPU) Nedir?
Bilgisayarın beynidir. "2+3 yap", "şu bilgiyi buraya kaydet", "ekrana şu rengi çiz" gibi komutları çalıştırır. Bizim işlemcimiz **RISC-V** adlı açık kaynaklı bir komut seti kullanır — yani işlemcimiz hangi komutları anlayacak, bunu RISC-V standardı belirler.

### FPGA Nedir?
**Field Programmable Gate Array** — yani "sahada programlanabilir kapı dizisi." Düşün ki LEGO gibi bir şey: içinde milyonlarca küçücük elektronik parça (kapı) var ve sen bunları istediğin şekilde bağlayarak kendi donanımını oluşturabiliyorsun. Biz bu LEGO'larla kendi işlemcimizi inşa ettik.

### Chisel Nedir?
Donanım tasarlamak için bir programlama dili. Normal programlama dillerinde (Python, C) yazılım yazarsın. Chisel'da ise **donanım** tasarlarsın — kablolar, kapılar, bellek birimleri gibi şeyleri kodla tanımlarsın. Chisel, yazdığın kodu **Verilog** adlı bir dile çevirir, Verilog da FPGA'nın anlayacağı formata dönüştürülür.

### SoC (System-on-Chip) Nedir?
"Tek çip üzerinde sistem" demek. Telefonundaki işlemci aslında bir SoC'dur — içinde CPU, grafik birimi, bellek kontrolcüsü, USB/Bluetooth gibi her şey tek bir çipin içinde. Biz de küçük bir SoC yaptık: CPU + ekran kontrolcüsü + buton okuyucu + seri haberleşme birimi, hepsi bir arada.

### Bare-metal Nedir?
İşletim sistemi olmadan doğrudan donanım üzerinde çalışan yazılım demek. Windows, Linux gibi bir aracı yok — işlemci açıldığında doğrudan senin kodunu çalıştırıyor. Telefonun açılır açılmaz Android'i yüklerken, bizim işlemcimiz açılır açılmaz Tetris'i yüklüyor.

---

## 🏗️ Projeyi Adım Adım Ne Yaptık?

### Adım 1: Proje Ortamını Hazırladık

**Ne yaptık:** Bilgisayarımızda bir proje klasörü oluşturduk ve gerekli araçları kurduk.

**Dosyalar:**
- `build.sbt` → Projenin "tarifi" gibi. Hangi kütüphaneleri kullanacağımızı, Chisel versiyonunu vs. tanımlar.
- `.gitignore` → Git'e "bu dosyaları takip etme" der (derleme çıktıları gibi gereksiz dosyalar).

**Araçlar:**
- **SBT (Scala Build Tool):** Chisel kodlarını derleyen araç. "Yemek yapmak için fırın" gibi düşün.
- **Git & GitHub:** Kodlarımızı kaydedip bulutta sakladığımız sistem. Her değişikliği kaydediyoruz, böylece geriye dönebiliriz.

---

### Adım 2: İşlemciyi (CPU) Tasarladık

Bu en kritik adım. Sıfırdan bir işlemci tasarladık. İşlemcimiz **tek çevrimli (single-cycle)** — yani her komut 1 saat vuruşunda tamamlanıyor (gerçek işlemcilerde pipeline var ama basitlik için bunu tercih ettik).

#### 2a. ALU (Aritmetik Mantık Birimi) — `ALU.scala`
**Ne yapar:** Matematiksel ve mantıksal işlemleri yapar.
- Toplama (2 + 3 = 5)
- Çıkarma (7 - 4 = 3)
- Karşılaştırma (5 < 8 → doğru)
- Mantıksal işlemler (AND, OR, XOR)
- Bit kaydırma (sayıyı sola/sağa kaydırma)

**Benzetme:** Hesap makinesi gibi düşün. İşlemcinin "hesap yapan" kısmı.

#### 2b. Register File (Yazmaç Grubu) — `RegisterFile.scala`
**Ne yapar:** 32 tane küçük, çok hızlı bellek hücresi. İşlemci, üzerinde çalıştığı sayıları burada tutar.
- `x0` yazmacı her zaman 0'dır (RISC-V kuralı).
- Diğer 31 tanesi geçici veri saklamak için kullanılır.

**Benzetme:** Masa üzerindeki not kağıtları gibi. Hesap yaparken ara sonuçları bunlara yazarsın, sonra okursun.

#### 2c. Instruction Decoder (Buyruk Çözücü) — `InstructionDecode.scala`
**Ne yapar:** İşlemciye gelen 32-bitlik komutu (sayı olarak) okur ve "bu komut toplama mı, çıkarma mı, belleğe yazma mı?" diye çözer. Sonra ALU'ya ve diğer birimlere uygun sinyalleri gönderir.

**Benzetme:** Bir tercüman gibi. Makine diliyle yazılmış komutu alıp, işlemcinin her parçasına "sen şunu yap, sen bunu yap" diye talimat verir.

#### 2d. Core (İşlemci Çekirdeği) — `Core.scala`
**Ne yapar:** Yukarıdaki tüm parçaları birleştirir ve şu döngüyü çalıştırır:
1. **Getir (Fetch):** Bellekten bir sonraki komutu oku.
2. **Çöz (Decode):** Komutu anlam, hangi işlem yapılacak belirle.
3. **Çalıştır (Execute):** ALU ile işlemi yap.
4. **Kaydet (Writeback):** Sonucu yazmaca veya belleğe yaz.
5. **Tekrarla:** Program sayacını (PC) güncelle, bir sonraki komuta geç.

**Benzetme:** Fabrikadaki montaj hattı gibi. Her adımda bir iş yapılır ve sonuç bir sonraki adıma geçer.

---

### Adım 3: Çevre Birimlerini Tasarladık

İşlemci tek başına hiçbir şey yapamaz — dış dünya ile iletişim kurması lazım. Bunun için çevre birimleri (peripheral) ekledik.

#### 3a. GPIO Controller — `GpioController.scala`
**Ne yapar:** 
- **LED'lere** veri yazar (ışık yakar/söndürür).
- **Butonlardan** veri okur (kullanıcı hangi butona bastı?).

İşlemci, belirli bir bellek adresine yazınca LED yanar; başka bir adresi okuyunca butonların durumunu öğrenir. Buna **MMIO (Memory-Mapped I/O)** denir — yani giriş/çıkış cihazları bellek adresleri üzerinden kontrol edilir.

#### 3b. UART Controller — `UartController.scala`
**Ne yapar:** İşlemci ile bilgisayar arasında **seri haberleşme** sağlar. USB kablosu üzerinden bilgisayardaki terminale (PuTTY gibi) yazı gönderebilirsin.
- Hız: 115200 baud (saniyede ~11.520 karakter)
- Örnek: İşlemci "OYUN BITTI!" yazısını terminale gönderir.

**Benzetme:** Telgraf makinesi gibi — tek bir hat üzerinden tık-tık-tık diye karakter gönderir.

#### 3c. HDMI Framebuffer — `HdmiController.scala`
**Ne yapar:** Ekrana görüntü gönderir!
- 320x240 piksel çözünürlük (HDMI üzerinden 640x480'e 2x ölçeklenir).
- 8-bit renk: Her piksel 1 byte = 3 bit kırmızı + 3 bit yeşil + 2 bit mavi = 256 farklı renk.
- **Framebuffer:** 76.800 byte'lık bir bellek alanı (320 × 240). İşlemci bu belleğe bir piksel rengi yazdığında, o piksel ekranda görünür.

**Benzetme:** Bir ızgara düşün. Her hücreye bir renk yazıyorsun. Bu ızgara saniyede 60 kere ekrana aktarılıyor — böylece canlı bir görüntü oluşuyor.

#### 3d. SoC Top — `SoCTop.scala`
**Ne yapar:** Yukarıdaki tüm parçaları (CPU + GPIO + UART + HDMI + Bellek) bir araya getirir ve **bellek haritasını** oluşturur:

| Adres Aralığı | Ne Var? |
|---|---|
| `0x00000000 - 0x00003FFF` | ROM (Program kodu — 16 KB) |
| `0x00004000 - 0x00007FFF` | RAM (Veri — 16 KB) |
| `0x80000000` | GPIO LED'ler |
| `0x80000004` | GPIO Butonlar |
| `0x80000010` | UART Veri |
| `0x80000014` | UART Durum |
| `0x90000000 - 0x90012BFF` | HDMI Framebuffer (76.800 byte) |

İşlemci bir adrese yazınca veya okuyunca, SoC Top bu adresin hangi birime ait olduğunu belirleyip doğru yere yönlendirir. Postacı gibi — zarfın üzerindeki adrese bakıp doğru kapıya bırakır.

---

### Adım 4: Tetris Oyununu Yazdık

İşlemcimiz hazır, çevre birimleri hazır — şimdi üzerinde çalışacak yazılımı (Tetris) yazdık.

#### 4a. Boot Kodu — `boot.S`
**Ne yapar:** İşlemci ilk açıldığında çalışan ilk birkaç satır kod. Stack pointer'ı (yığın işaretçisi) ayarlar ve C dilindeki `main` fonksiyonuna atlar.

**Benzetme:** Bilgisayarın BIOS'u gibi — sistem açıldığında "her şey hazır, artık asıl programı başlatıyorum" der.

#### 4b. Linker Script — `link.ld`
**Ne yapar:** Derleyiciye "kodun bellekte nereye yerleşeceğini" söyler. ROM adresleri, RAM adresleri, yığın boyutu gibi bilgileri içerir.

#### 4c. Donanım Soyutlama Katmanı — `soc.h`
**Ne yapar:** Donanım adreslerini C dilinde kolay kullanılabilir tanımlara dönüştürür.
- `draw_pixel(x, y, color)` → Ekrana piksel çizer
- `uart_puts("merhaba")` → Terminale yazı gönderir
- `*GPIO_BTNS` → Buton durumunu okur

#### 4d. Tetris Oyunu — `main.c`
**Ne yapar:** Asıl oyun mantığını içerir:

1. **Başlık Ekranı:** Oyun açıldığında "TETRIS" yazısı ve kontrol bilgilerinin gösterildiği bir karşılama ekranı.
2. **Oyun Döngüsü:** Her karede şunları yapar:
   - Butonları oku (sola, sağa, döndür, hızlı düşür)
   - Yerçekimi uygula (blok otomatik düşer)
   - Çarpışma kontrolü yap (blok duvara veya başka bloğa çarptı mı?)
   - Dolu satırları temizle ve skor güncelle
   - Ekranı yeniden çiz
3. **Game Over Ekranı:** Bloklar tavana ulaştığında oyun biter, skor gösterilir.
4. **Skor Sistemi:** 1 satır = 100, 2 satır = 300, 3 satır = 500, 4 satır (Tetris!) = 800 puan

**Kontroller:**
| Buton | İşlev |
|---|---|
| **Orta (btnC)** | Bloğu döndür |
| **Sol (btnL)** | Sola kaydır |
| **Sağ (btnR)** | Sağa kaydır |
| **Aşağı (btnD)** | Hızlı düşür (basılı tut) |
| **Yukarı (btnU)** | Oyunu sıfırla |

---

### Adım 5: C Kodunu İşlemcimizin Anlayacağı Hale Getirdik

C dilinde yazdığımız Tetris kodu, doğrudan işlemcide çalışamaz. Onu **makine diline** (1'ler ve 0'lar) çevirmemiz gerekiyor.

#### Derleme Süreci:
```
main.c (C kodu)
    ↓ RISC-V GCC Derleyici
program.elf (çalıştırılabilir dosya)
    ↓ objcopy
program.bin (ham ikili veri)
    ↓ bin2hex.py
program.hex (Chisel'in ROM'a gömeceği format)
```

- **GCC Derleyici:** C kodunu makine koduna çevirir. Ama normal GCC değil — **RISC-V GCC** kullanıyoruz, çünkü bizim işlemcimiz RISC-V komutlarını anlıyor.
- **HEX dosyası:** Sonuç olarak bir HEX dosyası oluşuyor. Bu dosya, Chisel kodumuz tarafından okunarak işlemcimizin ROM belleğine gömülüyor.

---

### Adım 6: Verilog Kodu Ürettik

Chisel kodlarımızı gerçek FPGA'nın anlayacağı formata çevirdik.

```
Chisel kodu (.scala dosyaları)
    ↓ sbt run
Verilog/SystemVerilog kodu (.sv dosyaları)
    ↓ Vivado
Bitstream (.bit dosyası)
    ↓ FPGA Programlama
FPGA üzerinde çalışan donanım!
```

**Üretilen dosyalar:**
- `SoCTop.sv` → Tüm SoC'un SystemVerilog kodu (~76 KB)
- `TopWrapper.v` → FPGA'ya özgü saat üreteci ve HDMI fiziksel bağlantılar
- `tmds_channel_encoder.v` → HDMI sinyallerini kodlayan modül
- `pins.xdc` → Hangi FPGA pini = hangi fiziksel bağlantı (HDMI, buton, LED, UART)

---

### Adım 7: FPGA'ya Yükleme (Vivado ile)

**Vivado**, Xilinx (şimdi AMD) FPGA'lar için kullanılan tasarım aracıdır. Adımlar:

1. Vivado'da yeni proje oluştur, **Nexys Video** kartını seç.
2. Üretilen 3 Verilog dosyasını projeye ekle.
3. Pin tanımlamaları dosyasını (`.xdc`) ekle.
4. **Sentez (Synthesis):** Verilog kodunu FPGA'nın mantık kapılarına dönüştür.
5. **Yerleştirme (Implementation):** Kapıları FPGA çipinin fiziksel konumlarına yerleştir.
6. **Bitstream Üretimi:** FPGA'ya yüklenecek son dosyayı oluştur.
7. **Programlama:** USB ile bilgisayara bağlı kartı programla.
8. **Sonuç:** HDMI'dan monitöre Tetris oyunu görüntüsü gelir! 🎉

---

## 📂 Proje Dosya Yapısı

```
chisel-riscv-tetris-soc/
│
├── build.sbt                    ← Proje yapılandırma dosyası
├── compile.py                   ← C kodunu derleme betiği
│
├── src/main/scala/              ← Chisel donanım kodları
│   ├── ALU.scala                ← Aritmetik mantık birimi
│   ├── RegisterFile.scala       ← 32 yazmaç (hızlı bellek)
│   ├── InstructionDecode.scala  ← Komut çözücü
│   ├── Core.scala               ← İşlemci çekirdeği (CPU)
│   ├── Elaborate.scala          ← Verilog üretme betiği
│   └── soc/
│       ├── GpioController.scala ← LED ve buton kontrolcüsü
│       ├── UartController.scala ← Seri haberleşme (terminal)
│       ├── HdmiController.scala ← Ekran kontrolcüsü (HDMI)
│       └── SoCTop.scala         ← Her şeyi birleştiren üst modül
│
├── sw/                          ← C yazılım kodları
│   ├── boot.S                   ← İlk başlatma kodu (Assembly)
│   ├── link.ld                  ← Bellek haritası tanımı
│   ├── soc.h                    ← Donanım erişim kütüphanesi
│   ├── main.c                   ← Tetris oyun kodu
│   ├── program.hex              ← Derlenmiş makine kodu
│   └── bin2hex.py               ← Binary→HEX dönüştürücü
│
└── generated/                   ← Üretilen FPGA dosyaları
    ├── SoCTop.sv                ← Ana SoC Verilog kodu
    ├── TopWrapper.v             ← FPGA sarmalayıcı (saat + HDMI)
    ├── tmds_channel_encoder.v   ← HDMI kodlayıcı
    └── pins.xdc                 ← FPGA pin tanımlamaları
```

---

## 🔄 Veri Akışı: Butona Bastığında Ne Olur?

Diyelim ki **Sol Butona** bastın. Perde arkasında şunlar oluyor:

```
1. Butonun elektrik sinyali FPGA'nın fiziksel pinine ulaşır
       ↓
2. GPIO Controller bu sinyali okur ve "buton yazmaç"ına kaydeder
       ↓
3. CPU, döngüsünde GPIO adresini (0x80000004) okur
       ↓
4. C kodu: "2. bit 1 mi? Evet → sol buton basılmış"
       ↓
5. Tetris mantığı: "current_x'i 1 azalt (sola kaydır)"
       ↓
6. Çarpışma kontrolü: "Yeni pozisyon uygun mu? Evet"
       ↓
7. draw_board_block() → Framebuffer'a yeni piksel renkleri yaz
       ↓
8. HDMI Controller, framebuffer'ı okuyarak HDMI sinyali üretir
       ↓
9. Monitörde blok sola kayar! 🎮
```

Bu sürecin tamamı **bir saat vuruşunda** (yaklaşık 10 nanosaniye = 0,00000001 saniye) başlar ve birkaç yüz saat vuruşu içinde tamamlanır. Yani göz açıp kapayıncaya kadar olup biter!

---

## ❓ Sık Sorulan Sorular

### Bu projede işletim sistemi var mı?
**Hayır.** Bu bir "bare-metal" projedir. İşlemci açılır açılmaz doğrudan Tetris kodunu çalıştırır. Windows veya Linux gibi bir aracı yoktur. İşletim sistemi eklemek ayrı bir proje olarak planlanmıştır.

### Klavye veya fare bağlayabilir miyim?
**Şu an hayır.** Nexys Video kartının 5 fiziksel butonu kullanılıyor. Klavye/fare bağlamak için USB Host Controller tasarlamak gerekir — bu çok daha karmaşık bir iş. İşletim sistemi projesiyle birlikte eklenebilir.

### Bu işlemci gerçek bir Intel/AMD kadar güçlü mü?
**Hayır, çok daha basit.** Bizim işlemcimiz:
- Tek çevrimli (pipeline yok)
- 100 MHz civarı çalışıyor
- Sadece temel RV32I komutlarını destekliyor
- Çarpma/bölme donanımı yok

Ama eğitim amaçlı mükemmel — her parçasına hakimsin!

### FPGA kapattığımda program silinir mi?
**Evet.** FPGA'lar genellikle uçucu bellekle çalışır. Kartı kapattığında program silinir. Her açılışta yeniden programlaman gerekir. (Bazı kartlarda Flash belleğe kalıcı yazma da mümkündür.)

---

## 🎯 Bu Projede Kullanılan Teknolojiler

| Teknoloji | Ne İçin Kullanıldı |
|---|---|
| **Chisel** | İşlemci ve SoC donanım tasarımı |
| **Scala** | Chisel'in üzerinde çalıştığı programlama dili |
| **SBT** | Scala/Chisel derleme aracı |
| **RISC-V ISA** | İşlemcimizin komut seti standardı |
| **C dili** | Tetris oyun yazılımı |
| **RISC-V GCC** | C kodunu RISC-V makine koduna derleme |
| **Vivado** | FPGA sentez ve programlama |
| **Git/GitHub** | Versiyon kontrolü ve kaynak kod depolama |
| **Nexys Video** | Hedef FPGA kartı (Xilinx Artix-7 200T) |

---

> 📝 **Not:** Bu proje, donanım tasarımının temellerini öğrenmek için mükemmel bir başlangıç noktasıdır. Sıfırdan bir işlemci tasarlayıp üzerinde yazılım çalıştırmak, bilgisayarların nasıl çalıştığını derinlemesine anlamanı sağlar.
