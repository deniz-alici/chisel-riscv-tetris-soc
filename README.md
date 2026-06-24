# 🎮 Chisel RISC-V SoC & Tetris — Nexys Video FPGA

Chisel (Scala tabanlı donanım tanımlama dili) ile sıfırdan tasarlanmış **RISC-V (RV32I) işlemci çekirdeği ve SoC**, **Nexys Video** (Xilinx Artix-7 200T) FPGA kartını hedef alır. Üzerinde bare-metal C ile yazılmış **Tetris oyunu** çalışır ve HDMI üzerinden monitöre görüntü gönderir.

> 📖 Projenin detaylı Türkçe açıklaması için: [PROJE_ACIKLAMASI.md](PROJE_ACIKLAMASI.md)

---

## ✨ Özellikler

| Bileşen | Açıklama |
|---|---|
| **CPU** | Tek çevrimli (single-cycle) RV32I işlemci çekirdeği |
| **Bellek** | 16 KB ROM + 16 KB RAM |
| **HDMI Çıkışı** | 320×240 piksel framebuffer (640×480 2x ölçekleme), 8-bit R3G3B2 renk |
| **GPIO** | 8 LED + 5 Buton (Nexys Video üzerindeki fiziksel butonlar) |
| **UART** | 115200 baud seri haberleşme (debug terminali) |
| **Yazılım** | Bare-metal C Tetris oyunu — başlık ekranı, skor sistemi, game over |

## 🎯 Buton Kontrolleri

| Buton | İşlev |
|---|---|
| Orta (btnC) | Bloğu döndür |
| Sol (btnL) | Sola kaydır |
| Sağ (btnR) | Sağa kaydır |
| Aşağı (btnD) | Hızlı düşür (basılı tut) |
| Yukarı (btnU) | Oyunu sıfırla |

---

## 📂 Proje Yapısı

```
├── src/main/scala/              ← Chisel donanım kodları
│   ├── ALU.scala                ← Aritmetik mantık birimi
│   ├── RegisterFile.scala       ← 32 yazmaç grubu
│   ├── InstructionDecode.scala  ← Komut çözücü
│   ├── Core.scala               ← İşlemci çekirdeği
│   ├── Elaborate.scala          ← Verilog üretme betiği
│   └── soc/
│       ├── GpioController.scala ← LED/Buton kontrolcüsü
│       ├── UartController.scala ← Seri haberleşme
│       ├── HdmiController.scala ← HDMI framebuffer
│       └── SoCTop.scala         ← SoC üst modül
│
├── sw/                          ← Bare-metal C yazılımı
│   ├── main.c                   ← Tetris oyunu
│   ├── soc.h                    ← Donanım erişim katmanı
│   ├── boot.S                   ← Başlatma kodu (Assembly)
│   ├── link.ld                  ← Linker script
│   └── program.hex              ← Derlenmiş makine kodu
│
├── generated/                   ← Üretilen FPGA dosyaları
│   ├── SoCTop.sv                ← SystemVerilog çıktısı
│   ├── TopWrapper.v             ← FPGA sarmalayıcı
│   ├── tmds_channel_encoder.v   ← HDMI kodlayıcı
│   └── pins.xdc                ← Pin tanımlamaları
│
├── PROJE_ACIKLAMASI.md          ← Detaylı Türkçe açıklama
├── build.sbt                    ← Proje yapılandırması
└── compile.py                   ← C derleme betiği
```

---

## 🚀 Derleme ve Çalıştırma

### 1. C Kodunu Derle
```powershell
python compile.py
```

### 2. Verilog Üret
```powershell
sbt "runMain riscv.Elaborate"
```

### 3. FPGA'ya Yükle (Vivado)
1. Vivado'da yeni proje oluştur → Nexys Video kartını seç
2. `generated/` altındaki 3 Verilog dosyasını ekle (TopWrapper = Top Module)
3. `generated/pins.xdc` dosyasını Constraints olarak ekle
4. Sentez → Implementation → Generate Bitstream
5. Hardware Manager → Program Device

---

## 🛠️ Gereksinimler

- [SBT](https://www.scala-sbt.org/) (Scala Build Tool)
- [RISC-V GCC Toolchain](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack)
- [Vivado](https://www.xilinx.com/products/design-tools/vivado.html) (Sentez ve FPGA programlama için)
- Python 3 (compile.py için)

---

## 📄 Lisans

Bu proje eğitim amaçlıdır.
