# RISC-V SoC Tetris — Çözüm Raporu

Bu belge, projeyi "kart üzerinde siyah ekran + LED yanmıyor" durumundan çalışır hale
getirmek için bulunan **kök nedenleri** ve uygulanan düzeltmeleri belgeler.

## Amaç
Chisel ile tasarlanmış özel bir **RV32I işlemci** (RISC-V) üzerinde, işletim sistemi
olmadan (bare-metal) bir **Tetris** oyunu çalıştırmak; görüntüyü **HDMI** ile Nexys Video
(Artix-7 XC7A200T) üzerinden monitöre vermek.

## Teşhis Yöntemi
Asıl kırılma noktası: tasarım **Verilator ile tam-sistem simülasyonu** yapılarak incelendi
(`sim_soc/`). CPU'nun UART çıkışı doğrudan TX pininden 115200 baud çözülerek okundu ve
framebuffer yazmaları sayıldı. Bu, "kartta neden ölü" sorusunu kesin yanıtladı.

## Bulunan Kök Nedenler ve Düzeltmeler

### 1. ⭐ Çekirdek + senkron bellek uyuşmazlığı (ASIL HATA)
Tek-cycle çekirdek, **senkron bellek** (`SyncReadMem`, 1-cycle gecikmeli) ile bağlıydı.
Komut-getirme ve load'lar 1 cycle kayıyor, dallanma adresleri ve yüklenen veri yanlış
oluyordu. **CPU hiç doğru çalışmıyordu** (sim'de UART tamamen sessizdi). Kılavuzdaki
crt0/framebuffer hatalarına sıra bile gelmiyordu.
- **Düzeltme:** Çekirdek **çok-cycle (multi-cycle) FSM**'e dönüştürüldü
  (`src/main/scala/Core.scala`): `s_fetch → s_exec → s_mem`. Datapath korundu, kontrol
  yeniden yazıldı. Senkron BRAM ile doğru çalışır; PC/register yazımı yalnız "complete"te.

### 2. ⭐ MMIO okuma s_mem'de geçersizdi
Çevrebirimler (UART status, GPIO buton) `readData`'yı yalnızca `readEnable` aktifken
sürüyor. FSM'de okuma-enable sadece `s_exec`'te aktifti, veri `s_mem`'de kullanılıyordu →
MMIO okumaları **0** dönüyordu. `while(*UART_STATUS & 0x1)` busy-bekleme hep "boşta"
görüp UART'ı taşırıyordu (bozuk çıktı).
- **Düzeltme:** `io.memReadEnable := (state =/= s_fetch) && isLoad` (s_exec + s_mem).

### 3. ROM bitstream'de initialize olmuyordu (kombinasyonel bellek denemesi)
Çekirdeği geçici olarak kombinasyonel `Mem`'e bağlayınca sim'de çalıştı ama **distributed
RAM `$readmemh` ile initialize olmadı** → kartta ROM boştu → CPU çöp çalıştırıyordu.
- **Düzeltme:** Bellekler **BRAM** (`SyncReadMem`) tutuldu (güvenilir init) + çekirdek
  multi-cycle yapıldı (madde 1). İkisi birlikte hem init hem mantık doğruluğunu sağlar.

### 4. crt0 eksikti (.data/.bss başlatılmıyordu)
`boot.S` sadece sp ayarlıyordu. Global değişkenler RAM'de çöp okuyordu.
- **Düzeltme:** `sw/boot.S`'e .data ROM→RAM kopyalama + .bss sıfırlama eklendi.
  `sw/link.ld`'ye `_data_rom/_data_start/_data_end/_bss_*` etiketleri + **RISC-V küçük
  bölümleri** (`.sdata→.data`, `.sbss→.bss`, `.srodata→.rodata`) eklendi.

### 5. ROM veri-okuma portu yoktu
Veri yolu ROM bölgesini (0x0–0x4000) haritalamıyordu → `.rodata` sabitleri
(şekil/renk tabloları) ve crt0'ın .data başlangıç değerleri hep 0 okunuyordu.
- **Düzeltme:** `SoCTop.scala`'ya ROM ikinci okuma portu + `isRom` mux dalı eklendi.

### 6. Framebuffer write-mask hatası
`HdmiController` her zaman `writeData[7:0]` yazıyordu; CPU'nun kaydırılmış SB byte'ları
(adres %4≠0) siyaha dönüşüyordu.
- **Düzeltme:** `writeMask` taşındı; doğru byte `OHToUInt`+`Mux1H` ile seçiliyor.

### 7. libgcc eksikti (RV32I bölme/çarpma)
`-nostdlib` + rv32i libgcc yokluğu → `__divsi3` link hatası.
- **Düzeltme:** `sw/libgcc_min.c` (yalnız toplama/kaydırma ile __div/__mod/__mul) +
  `compile.py` toolchain'i PATH'ten otomatik bulacak şekilde güncellendi.

### 0. ⭐⭐ ROM bitstream'de HİÇ initialize olmuyordu (ASIL "ölü kart" sebebi)
firtool, bellek init `$readmemh`'ini **`` `ifndef SYNTHESIS ``** koruması arkasına koyuyor
(sadece-simülasyon). Vivado sentezde `SYNTHESIS` makrosunu tanımladığı için `$readmemh`
**dışlanıyordu → ROM bitstream'de BOŞ → CPU çöp/sıfır çalıştırıyordu** (LED yok, ekran siyah).
Simülasyonda görünmüyordu çünkü Verilator `SYNTHESIS` tanımlamaz. **Orijinal tasarımda da
böyleydi** — board hiç çalışmamasının asıl sebebi buydu.
- **Düzeltme:** `vivado_build.tcl`'de `set_property verilog_define {ENABLE_INITIAL_MEM_}`
  ile makro sentezde tanımlandı → `$readmemh` sentezde de çalışır, ROM gerçekten yüklenir.
  Ayrıca `loadMemoryFromFileInline` + **mutlak yol** (`C:/chisel/.../program.hex`) kullanıldı.
- **Tanı yöntemi:** CPU/ROM'dan bağımsız bir tanı bitstream'i (LED'de saat sayacı + HDMI'da
  renk gradyanı) ile saat/MMCM/reset/LED ve HDMI/TMDS/pin yollarının **sağlam** olduğu
  kanıtlandı → sorun yalnız ROM/CPU'da kaldı.

### 8. UART TX/RX pinleri ters
`uart_tx`=V18 yanlıştı (Nexys Video'da FPGA TX = AA19).
- **Düzeltme:** `generated/pins.xdc`'de pinler düzeltildi.
- **Not:** UART gerçek baud'u ~28800'dür (`UartController(100MHz)` ama saat 25 MHz). UART
  okumak istenirse host **28800 baud** kullanmalı. Ekranı etkilemez.

## Doğrulama (simülasyon)
- UART banner + başlık ekranı mesajı tertemiz çözüldü (233 bayt).
- Framebuffer yazma sayısı: **92.448** → başlık ekranı çiziliyor.
- Bitstream timing: WNS pozitif (CPU 25 MHz @ MMCM).

## Derleme / Karta Atma
```
# 1) Yazılım (WSL'de, RISC-V gcc PATH'te):
python3 compile.py                    # -> sw/program.hex

# 2) Donanım Verilog (WSL'de):
sbt run                               # -> generated/SoCTop.sv

# 3) Bitstream (Windows, Vivado):
vivado -mode batch -source vivado_build.tcl

# 4) Karta programla (Windows, Vivado):
vivado -mode batch -source vivado_program.tcl
```

## Mimari Özet
- **CPU:** RV32I, çok-cycle (2–3 cycle/komut), 25 MHz.
- **Bellek:** 16 KB ROM (BRAM, program.hex init) + 16 KB RAM (BRAM, byte-maskeli).
- **MMIO:** GPIO (LED/buton) 0x8000_0000, UART 0x8000_0010, Framebuffer 0x9000_0000.
- **Video:** 320×240 8-bit (R3G3B2) framebuffer → 640×480@60 VGA zamanlama → TMDS/HDMI.
