/*
 * ============================================================
 *  RISC-V SoC Tetris - Nexys Video FPGA
 *  Chisel ile tasarlanmış özel RV32I işlemci üzerinde çalışır
 * ============================================================
 *
 *  Ekran: 320x240 piksel, 8-bit renk (R3G3B2)
 *  Kontrol: Nexys Video üzerindeki 5 buton
 *  Çıkış: HDMI üzerinden monitöre
 *
 *  Bu dosya bare-metal olarak çalışır (işletim sistemi yoktur).
 *  İşlemci açılır açılmaz doğrudan bu kodu çalıştırır.
 */

#include "soc.h"

// =============================================
//  YAPILANDIRMA SABİTLERİ
// =============================================

// Oyun tahtası boyutları (blok cinsinden)
#define BOARD_COLS     10
#define BOARD_ROWS     20

// Her bir kare bloğun piksel boyutu
#define BLOCK_SIZE     10

// Oyun tahtasının ekrandaki konumu (piksel cinsinden)
#define BOARD_X_OFFSET 60
#define BOARD_Y_OFFSET 20

// Bilgi paneli (skor, sonraki blok) konumu
#define INFO_X         185
#define INFO_Y         30

// =============================================
//  RENK PALETİ
// =============================================

// Her Tetromino tipi için özel renk (R3G3B2 formatı)
const unsigned char tetromino_colors[8] = {
    COLOR_BLACK,    // 0: Boş hücre
    0x1F,           // 1: I-Blok → Açık Mavi (Cyan)
    0xFC,           // 2: O-Blok → Sarı
    0xE3,           // 3: T-Blok → Mor (Magenta)
    0x1C,           // 4: S-Blok → Yeşil
    0xE0,           // 5: Z-Blok → Kırmızı
    0x03,           // 6: J-Blok → Mavi
    0xFF            // 7: L-Blok → Beyaz
};

// =============================================
//  TETROMİNO ŞEKİL TANIMLARI
// =============================================

// Her şekil 4x4'lük bir grid içinde bit maskesi olarak saklanır
// 4 rotasyon durumu: 0°, 90°, 180°, 270°
const unsigned short tetrominoes[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},   // I - Çubuk
    {0xCC00, 0xCC00, 0xCC00, 0xCC00},   // O - Kare (döndürme fark etmez)
    {0x4E00, 0x4640, 0x0E40, 0x4C40},   // T - T-Şekli
    {0x6C00, 0x4620, 0x06C0, 0x8C40},   // S - Zikzak-S
    {0xC600, 0x2640, 0x0C60, 0x4C80},   // Z - Zikzak-Z
    {0x8E00, 0x6440, 0x0E20, 0x44C0},   // J - Ters-L
    {0x2E00, 0x4460, 0x0E80, 0xC440}    // L - L-Şekli
};

// =============================================
//  OYUN DURUMU DEĞİŞKENLERİ
// =============================================

// Oyun tahtası: 0 = boş, 1-7 = renk indeksi
unsigned char board[BOARD_ROWS][BOARD_COLS] = {0};

// Aktif düşen blok durumu
int current_type     = 0;   // Tetromino tipi (0-6)
int current_rotation = 0;   // Rotasyon durumu (0-3)
int current_x        = 3;   // X konumu (sütun)
int current_y        = 0;   // Y konumu (satır)

// Sıradaki blok
int next_type = 0;

// Skor ve silinen satır sayısı
int score = 0;
int lines_cleared = 0;

// Oyun durumu: 0 = başlık ekranı, 1 = oyun, 2 = game over
int game_state = 0;

// =============================================
//  YARDIMCI FONKSİYONLAR
// =============================================

// Basit sözde rastgele sayı üreteci (Linear Congruential Generator)
unsigned int rand_state = 12345;
int my_rand() {
    rand_state = rand_state * 1103515245 + 12345;
    return (rand_state / 65536) & 32767;
}

// RV32I'da donanımsal bölme yok, yazılımsal mod alma
int modulo(int a, int b) {
    if (a < 0) a = -a;
    while (a >= b) {
        a -= b;
    }
    return a;
}

// Sayıyı desimal olarak UART'a yazdır
void uart_print_num(int n) {
    if (n == 0) {
        uart_putc('0');
        return;
    }
    char buf[10];
    int i = 0;
    int temp = n;
    if (temp < 0) {
        uart_putc('-');
        temp = -temp;
    }
    while (temp > 0 && i < 10) {
        buf[i++] = '0' + modulo(temp, 10);
        temp = temp / 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        uart_putc(buf[j]);
    }
}

// =============================================
//  ÇİZİM FONKSİYONLARI
// =============================================

// Yatay çizgi çiz
void draw_hline(int x, int y, int len, unsigned char color) {
    for (int i = 0; i < len; i++) {
        draw_pixel(x + i, y, color);
    }
}

// Dikey çizgi çiz
void draw_vline(int x, int y, int len, unsigned char color) {
    for (int i = 0; i < len; i++) {
        draw_pixel(x, y + i, color);
    }
}

// Dikdörtgen çiz (sadece kenarlar)
void draw_rect(int x, int y, int w, int h, unsigned char color) {
    draw_hline(x, y, w, color);
    draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

// İçi dolu dikdörtgen çiz
void fill_rect(int x, int y, int w, int h, unsigned char color) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            draw_pixel(x + col, y + row, color);
        }
    }
}

// Tek bir tetromino kare bloğu çiz (3D görünüm efektli)
void draw_board_block(int col, int row, unsigned char color) {
    int sx = BOARD_X_OFFSET + col * BLOCK_SIZE;
    int sy = BOARD_Y_OFFSET + row * BLOCK_SIZE;

    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            if (color == COLOR_BLACK) {
                // Boş hücre: koyu arka plan
                draw_pixel(sx + x, sy + y, 0x00);
            } else if (x == 0 || y == 0) {
                // Üst ve sol kenar: parlak (ışık efekti)
                draw_pixel(sx + x, sy + y, COLOR_WHITE);
            } else if (x == BLOCK_SIZE - 1 || y == BLOCK_SIZE - 1) {
                // Alt ve sağ kenar: koyu (gölge efekti)
                draw_pixel(sx + x, sy + y, 0x49);
            } else {
                // İç kısım: bloğun rengi
                draw_pixel(sx + x, sy + y, color);
            }
        }
    }
}

// Küçük önizleme bloğu çiz (4x4 piksel)
void draw_preview_block(int px, int py, unsigned char color) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            draw_pixel(px + x, py + y, color);
        }
    }
}

// 5x5 piksellik basit karakter çiz (sadece rakamlar 0-9)
// Her rakam 5 satırlık bir bitmap ile temsil edilir
const unsigned char digit_bitmaps[10][5] = {
    {0x0E, 0x11, 0x11, 0x11, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x06, 0x08, 0x1F},  // 2
    {0x0E, 0x11, 0x06, 0x11, 0x0E},  // 3
    {0x12, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x1E},  // 5
    {0x0E, 0x10, 0x1E, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x04},  // 7
    {0x0E, 0x11, 0x0E, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x0F, 0x01, 0x0E}   // 9
};

void draw_digit(int x, int y, int digit, unsigned char color) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            if (digit_bitmaps[digit][row] & (1 << (4 - col))) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

// Sayıyı ekrana çiz (6 haneli gösterim)
void draw_number(int x, int y, int num, unsigned char color) {
    char digits[6] = {0};
    int temp = num;
    if (temp < 0) temp = 0;

    for (int i = 5; i >= 0; i--) {
        digits[i] = modulo(temp, 10);
        temp = temp / 10;
    }

    // Baştaki sıfırları atla (en az 1 hane göster)
    int started = 0;
    for (int i = 0; i < 6; i++) {
        if (digits[i] != 0 || i == 5) started = 1;
        if (started) {
            draw_digit(x + i * 6, y, digits[i], color);
        }
    }
}

// =============================================
//  BAŞLIK EKRANI (SPLASH SCREEN)
// =============================================

void draw_title_screen() {
    clear_screen(0x00);  // Siyah arka plan

    // === Üst dekoratif çizgi ===
    draw_hline(20, 20, 280, 0x1F);    // Cyan çizgi
    draw_hline(20, 22, 280, 0x03);    // Mavi çizgi

    // === "TETRIS" yazısı (büyük blok harflerle piksel piksel) ===
    // Her harf yaklaşık 20x24 piksel, toplam ~160px genişlik
    // Merkez: (320 - 160) / 2 = 80 civarı
    int tx = 65;
    int ty = 40;
    unsigned char tc = 0xFC; // Sarı

    // T harfi
    fill_rect(tx, ty, 18, 4, tc);
    fill_rect(tx + 7, ty + 4, 4, 20, tc);
    tx += 22;

    // E harfi
    fill_rect(tx, ty, 4, 24, tc);
    fill_rect(tx, ty, 16, 4, tc);
    fill_rect(tx, ty + 10, 12, 4, tc);
    fill_rect(tx, ty + 20, 16, 4, tc);
    tx += 20;

    // T harfi
    fill_rect(tx, ty, 18, 4, tc);
    fill_rect(tx + 7, ty + 4, 4, 20, tc);
    tx += 22;

    // R harfi
    fill_rect(tx, ty, 4, 24, tc);
    fill_rect(tx, ty, 14, 4, tc);
    fill_rect(tx + 14, ty + 4, 4, 8, tc);
    fill_rect(tx, ty + 10, 14, 4, tc);
    fill_rect(tx + 10, ty + 14, 4, 10, tc);
    tx += 20;

    // I harfi
    fill_rect(tx, ty, 14, 4, tc);
    fill_rect(tx + 5, ty + 4, 4, 16, tc);
    fill_rect(tx, ty + 20, 14, 4, tc);
    tx += 18;

    // S harfi
    fill_rect(tx, ty, 16, 4, tc);
    fill_rect(tx, ty + 4, 4, 8, tc);
    fill_rect(tx, ty + 10, 16, 4, tc);
    fill_rect(tx + 12, ty + 14, 4, 6, tc);
    fill_rect(tx, ty + 20, 16, 4, tc);

    // === Alt dekoratif çizgi ===
    draw_hline(20, 72, 280, 0x03);
    draw_hline(20, 74, 280, 0x1F);

    // === Bilgi paneli ===
    int iy = 90;

    // "RISC-V SoC" etiketi
    // Basit piksel yazı yerine renkli kutularla gösterelim
    fill_rect(80, iy, 160, 18, 0x03);        // Mavi arka plan kutu
    draw_rect(80, iy, 160, 18, 0x1F);        // Cyan kenarlık

    // === Kontrol bilgileri ===
    iy = 120;

    // Kontrol başlığı kutusu
    fill_rect(50, iy, 220, 14, 0x49);
    draw_rect(50, iy, 220, 14, COLOR_WHITE);

    // Buton açıklamaları (renkli kutucuklarla)
    iy = 145;
    int spacing = 18;

    // Sol buton
    fill_rect(60, iy, 10, 10, 0x1C);  // Yeşil kare
    iy += spacing;

    // Sağ buton
    fill_rect(60, iy, 10, 10, 0x1C);
    iy += spacing;

    // Orta buton
    fill_rect(60, iy, 10, 10, 0xFC);  // Sarı kare
    iy += spacing;

    // Aşağı buton
    fill_rect(60, iy, 10, 10, 0xE0);  // Kırmızı kare
    iy += spacing;

    // Yukarı buton
    fill_rect(60, iy, 10, 10, 0xE3);  // Mor kare

    // === "Başlamak için herhangi bir butona bas" ===
    // Yanıp sönen efekt için basit bir kutu
    fill_rect(40, 222, 240, 14, 0x00);
    draw_rect(40, 222, 240, 14, 0xFC);  // Sarı kenarlıklı kutu

    // === Alt dekoratif çerçeve ===
    draw_rect(10, 10, 300, 228, 0x49);   // Dış çerçeve (gri)
    draw_rect(12, 12, 296, 224, 0x03);   // İç çerçeve (mavi)

    uart_puts("=================================\r\n");
    uart_puts("  RISC-V SoC TETRIS\r\n");
    uart_puts("  Chisel + Nexys Video FPGA\r\n");
    uart_puts("=================================\r\n");
    uart_puts("Baslamak icin bir butona basin.\r\n");
}

// =============================================
//  OYUN SONU EKRANI
// =============================================

void draw_game_over_screen() {
    // Yarı saydam efekt: ekranı karartma
    for (int y = 60; y < 180; y++) {
        for (int x = 40; x < 280; x++) {
            draw_pixel(x, y, 0x00);
        }
    }

    // Game Over kutusu
    draw_rect(45, 65, 230, 110, COLOR_WHITE);
    draw_rect(47, 67, 226, 106, 0xE0);  // Kırmızı iç kenarlık
    fill_rect(48, 68, 224, 104, 0x00);  // Siyah iç

    // "OYUN BİTTİ" yazısı (büyük blok harfler)
    // O
    fill_rect(70, 80, 14, 4, 0xE0);
    fill_rect(70, 80, 4, 20, 0xE0);
    fill_rect(80, 80, 4, 20, 0xE0);
    fill_rect(70, 96, 14, 4, 0xE0);
    // Y
    fill_rect(88, 80, 4, 10, 0xE0);
    fill_rect(100, 80, 4, 10, 0xE0);
    fill_rect(92, 90, 4, 10, 0xE0);
    // U
    fill_rect(108, 80, 4, 20, 0xE0);
    fill_rect(120, 80, 4, 20, 0xE0);
    fill_rect(108, 96, 16, 4, 0xE0);
    // N
    fill_rect(128, 80, 4, 20, 0xE0);
    fill_rect(140, 80, 4, 20, 0xE0);
    fill_rect(132, 84, 4, 4, 0xE0);
    fill_rect(136, 88, 4, 4, 0xE0);

    // Skor gösterimi
    fill_rect(90, 115, 140, 16, 0x03);
    draw_rect(90, 115, 140, 16, 0x1F);
    draw_number(160, 120, score, COLOR_WHITE);

    // "Tekrar: YUKARI butonu" kutusu
    fill_rect(75, 145, 170, 14, 0x49);
    draw_rect(75, 145, 170, 14, 0xFC);

    uart_puts("OYUN BITTI! Skor: ");
    uart_print_num(score);
    uart_puts("\r\n");
}

// =============================================
//  OYUN TAHTASI ÇİZİMİ
// =============================================

// Bilgi panelini çiz (skor, sonraki blok)
void draw_info_panel() {
    // Skor kutusu
    fill_rect(INFO_X, INFO_Y, 70, 30, 0x00);
    draw_rect(INFO_X, INFO_Y, 70, 30, 0x1F);

    // Skor sayısı
    draw_number(INFO_X + 10, INFO_Y + 14, score, 0xFC);

    // Silinen satır kutusu
    fill_rect(INFO_X, INFO_Y + 40, 70, 30, 0x00);
    draw_rect(INFO_X, INFO_Y + 40, 70, 30, 0x1C);

    // Silinen satır sayısı
    draw_number(INFO_X + 10, INFO_Y + 54, lines_cleared, 0x1C);

    // Sıradaki blok önizleme kutusu
    fill_rect(INFO_X, INFO_Y + 80, 70, 50, 0x00);
    draw_rect(INFO_X, INFO_Y + 80, 70, 50, 0x49);

    // Sıradaki bloğu çiz
    unsigned short next_shape = tetrominoes[next_type][0];
    int preview_x = INFO_X + 27;
    int preview_y = INFO_Y + 95;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (next_shape & (1 << (15 - (r * 4 + c)))) {
                draw_preview_block(preview_x + c * 5, preview_y + r * 5, tetromino_colors[next_type + 1]);
            }
        }
    }
}

// Ana oyun ekranını çiz
void draw_game_frame() {
    // Oyun tahtası çerçevesi (çift kenarlık)
    int frame_x = BOARD_X_OFFSET - 2;
    int frame_y = BOARD_Y_OFFSET - 2;
    int frame_w = BOARD_COLS * BLOCK_SIZE + 4;
    int frame_h = BOARD_ROWS * BLOCK_SIZE + 4;

    draw_rect(frame_x, frame_y, frame_w, frame_h, COLOR_WHITE);
    draw_rect(frame_x - 1, frame_y - 1, frame_w + 2, frame_h + 2, 0x49);
}

// Ekranı yeniden çiz (her kare)
void redraw_game() {
    // Oyun çerçevesini çiz
    draw_game_frame();

    // Oturmuş blokları çiz
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            draw_board_block(c, r, tetromino_colors[board[r][c]]);
        }
    }

    // Aktif düşen bloğu çiz
    unsigned short shape = tetrominoes[current_type][current_rotation];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                draw_board_block(current_x + c, current_y + r, tetromino_colors[current_type + 1]);
            }
        }
    }

    // Bilgi panelini güncelle
    draw_info_panel();
}

// =============================================
//  OYUN MEKANİKLERİ
// =============================================

// Çarpışma kontrolü
int check_collision(int nx, int ny, int nrot) {
    unsigned short shape = tetrominoes[current_type][nrot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                int bx = nx + c;
                int by = ny + r;

                // Tahta sınırları dışı
                if (bx < 0 || bx >= BOARD_COLS || by >= BOARD_ROWS)
                    return 1;

                // Başka blokla çakışma
                if (by >= 0 && board[by][bx] != 0)
                    return 1;
            }
        }
    }
    return 0;
}

// Bloğu tahtaya sabitle
void place_tetromino() {
    unsigned short shape = tetrominoes[current_type][current_rotation];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                int bx = current_x + c;
                int by = current_y + r;
                if (by >= 0 && by < BOARD_ROWS) {
                    board[by][bx] = current_type + 1;
                }
            }
        }
    }
}

// Dolu satırları temizle ve skor güncelle
void clear_lines() {
    int cleared = 0;
    for (int r = BOARD_ROWS - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] == 0) {
                full = 0;
                break;
            }
        }
        if (full) {
            cleared++;
            // Üstteki satırları aşağı kaydır
            for (int sy = r; sy > 0; sy--) {
                for (int sx = 0; sx < BOARD_COLS; sx++) {
                    board[sy][sx] = board[sy - 1][sx];
                }
            }
            // En üst satırı temizle
            for (int sx = 0; sx < BOARD_COLS; sx++) {
                board[0][sx] = 0;
            }
            r++;  // Aynı satırı tekrar kontrol et
        }
    }

    // Skor hesapla (1 satır = 100, 2 = 300, 3 = 500, 4 = 800)
    if (cleared > 0) {
        lines_cleared += cleared;
        if (cleared == 1) score += 100;
        else if (cleared == 2) score += 300;
        else if (cleared == 3) score += 500;
        else score += 800;  // Tetris! (4 satır birden)

        uart_puts("Satir silindi! Skor: ");
        uart_print_num(score);
        uart_puts("\r\n");
    }
}

// Yeni tetromino üret
void spawn_new_tetromino() {
    current_type = next_type;
    next_type = modulo(my_rand(), 7);
    current_rotation = 0;
    current_x = 3;
    current_y = 0;
}

// Oyun tahtasını sıfırla
void reset_game() {
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            board[r][c] = 0;
        }
    }
    score = 0;
    lines_cleared = 0;
    next_type = modulo(my_rand(), 7);
    spawn_new_tetromino();
    clear_screen(COLOR_BLACK);
}

// =============================================
//  ANA PROGRAM GİRİŞ NOKTASI
// =============================================

int main() {
    // UART üzerinden başlangıç mesajı gönder
    uart_puts("\r\n");
    uart_puts("================================\r\n");
    uart_puts("  RISC-V SoC Tetris v1.0\r\n");
    uart_puts("  Chisel + Nexys Video FPGA\r\n");
    uart_puts("================================\r\n");

    // Başlık ekranını göster
    game_state = 0;
    draw_title_screen();

    int ticks = 0;
    unsigned int last_btn = 0;

    // === ANA OYUN DÖNGÜSÜ ===
    while (1) {
        // Buton durumunu oku
        unsigned int btn = *GPIO_BTNS;
        *GPIO_LEDS = btn;  // LED'lere yansıt

        // Yükselen kenar tespiti (buton yeni basılmış mı?)
        unsigned int pressed = btn & ~last_btn;
        last_btn = btn;

        // ========== DURUM 0: BAŞLIK EKRANI ==========
        if (game_state == 0) {
            if (pressed != 0) {
                // Herhangi bir butona basıldı → oyunu başlat
                game_state = 1;
                reset_game();
                uart_puts("Oyun basliyor!\r\n");
            }
            delay(50000);
            continue;
        }

        // ========== DURUM 2: GAME OVER EKRANI ==========
        if (game_state == 2) {
            if (pressed & 0x10) {
                // Yukarı buton → tekrar başlat
                game_state = 1;
                reset_game();
                uart_puts("Oyun yeniden baslatildi!\r\n");
            }
            delay(50000);
            continue;
        }

        // ========== DURUM 1: OYUN AKTİF ==========

        // --- Buton kontrolleri ---
        if (pressed & 0x01) {
            // Orta buton → Döndür
            int next_rot = (current_rotation + 1) & 3;
            if (!check_collision(current_x, current_y, next_rot)) {
                current_rotation = next_rot;
            }
        }
        if (pressed & 0x02) {
            // Sol buton → Sola kaydır
            if (!check_collision(current_x - 1, current_y, current_rotation)) {
                current_x--;
            }
        }
        if (pressed & 0x04) {
            // Sağ buton → Sağa kaydır
            if (!check_collision(current_x + 1, current_y, current_rotation)) {
                current_x++;
            }
        }
        if (btn & 0x08) {
            // Aşağı buton (basılı tutunca) → Hızlı düşür
            if (!check_collision(current_x, current_y + 1, current_rotation)) {
                current_y++;
                ticks = 0;
            }
        }
        if (pressed & 0x10) {
            // Yukarı buton → Oyunu sıfırla
            reset_game();
            uart_puts("Oyun sifirlandi!\r\n");
        }

        // --- Yerçekimi (otomatik düşüş) ---
        ticks++;
        if (ticks >= 20) {
            ticks = 0;
            if (!check_collision(current_x, current_y + 1, current_rotation)) {
                current_y++;
            } else {
                // Blok yere oturdu
                place_tetromino();
                clear_lines();
                spawn_new_tetromino();

                // Game Over kontrolü
                if (check_collision(current_x, current_y, current_rotation)) {
                    game_state = 2;
                    draw_game_over_screen();
                    continue;
                }
            }
        }

        // --- Ekranı yeniden çiz ---
        redraw_game();
        delay(30000);
    }

    return 0;
}
