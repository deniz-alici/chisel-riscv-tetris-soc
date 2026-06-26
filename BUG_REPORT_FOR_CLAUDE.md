# TETRIS SOC - BUG REPORT & CONTEXT FOR CLAUDE

Merhaba Claude, bu proje özel tasarlanmış bir RISC-V SoC (Chisel ile yazılmış) üzerinde çalışan "bare-metal" bir Tetris oyunudur. İşletim sistemi yok, donanım register'larına doğrudan yazıyoruz (soc.h kullanılarak). Saat hızı 50MHz.

## 📌 Son Yapılan Mimari Değişiklikler (Context)
1. **Flickering (Titreme) Çözümü:** 
   Oyun döngüsündeki delay() fonksiyonu tamamen kaldırıldı. İşlemci artık asenkron bir sayaç (drop_timer) kullanarak çalışıyor.
   Ekranın her karede baştan çizilmesi engellendi. "Smart Redraw" eklendi: Sadece oard ile old_board dizisinde farklı olan bloklar çiziliyor.
   Düşen hareketli blok için ise old_x, old_y, old_rotation değişkenleri tutuluyor. Blok hareket ettiğinde önce eski konumu siliniyor, sonra yeni konumu çiziliyor.
2. **Oyun Hızı ve Kontroller:**
   Oyunun genel düşüş limiti hızlandırıldı (drop_speed = 180000).
   Aşağı tuşuna (btnD) basılı tutulduğunda ışınlanmak yerine drop_timer += 15 eklenerek 15x hızda akıcı şekilde inmesi sağlandı.
   Duraklatma menüsü (btnU ile açılır) "Devam Et" ve "Çıkış" seçenekleri ekranda yan yana (sağ-sol tuşlarıyla seçilecek şekilde) konumlandırıldı.

## 🚨 Çözmeni İstediğimiz Mevcut Bug'lar

**BUG 1: Buton Hassasiyeti (Debounce Eksikliği)**
delay() kaldırıldığı için artık mekanik tuş sekmeleri (button bounce) oyuna anında yansıyor. Orta tuşa 1 kere basıldığında 3-4 kez dönüyor. tn okuma kısmına (unsigned int pressed = btn & ~last_btn;) basit ve non-blocking bir yazılımsal debounce sayacı eklemen gerekiyor.

**BUG 2: Ghost Bloklar ve Sol Duvarın Boyanması**
Tuşlardaki sekme yüzünden sola veya sağa basıldığında blok anında duvara çarpıyor. Çarptığında eski konumları tam silinemediği için ekranda iz bırakıyor (ghost blocks / yığılma). Bu yığılan pikseller gerçek diziye (oard[][]) kaydolmadığı için yeni gelen bloklar içinden geçebiliyor.
Bu durum büyük ihtimalle tuş sekmesinden dolayı current_x'in bir döngüde çok kez değişmesi, veya place_tetromino() ve draw_board_block() içinde x < 0 veya x >= BOARD_COLS güvenlik kontrollerinin eksik olmasından kaynaklanıyor.

## 🛠️ Beklenen Görev
Lütfen sw/main.c dosyasını inceleyerek:
1. Ana döngüye debounce_timer mantığını entegre et.
2. place_tetromino ve çizim fonksiyonlarındaki x, y array boundary limitlerini kesinleştir.
3. Hızlı tuş basımlarında ghost blok kalmasını engelleyecek silme mantığını (old_x) güvenli hale getir.
