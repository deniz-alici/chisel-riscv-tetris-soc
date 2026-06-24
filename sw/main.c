#include "soc.h"

// Oyun Boyutları
#define BOARD_COLS 10
#define BOARD_ROWS 20
#define BLOCK_SIZE 10
#define BOARD_X_OFFSET 110
#define BOARD_Y_OFFSET 20

// Tetromino Renkleri (R3G3B2 formatında)
const unsigned char tetromino_colors[8] = {
    COLOR_BLACK,
    COLOR_CYAN,
    COLOR_YELLOW,
    COLOR_MAGENTA,
    COLOR_GREEN,
    COLOR_RED,
    COLOR_BLUE,
    COLOR_WHITE
};

// Tetromino Şekil Tanımları (Her satır 4 rotasyonu temsil eder)
const unsigned short tetrominoes[7][4] = {
    // I (Çubuk)
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    // O (Kutu)
    {0xCC00, 0xCC00, 0xCC00, 0xCC00},
    // T (T-Şekli)
    {0x4E00, 0x4640, 0x0E40, 0x4C40},
    // S (Zikzak S)
    {0x6C00, 0x4620, 0x06C0, 0x8C40},
    // Z (Zikzak Z)
    {0xC600, 0x2640, 0x0C60, 0x4C80},
    // J (Ters L)
    {0x8E00, 0x6440, 0x0E20, 0x44C0},
    // L (L-Şekli)
    {0x2E00, 0x4460, 0x0E80, 0xC440}
};

// Oyun Tahtası (0: boş, 1-7: renk indeksleri)
unsigned char board[BOARD_ROWS][BOARD_COLS] = {0};

// Aktif Tetromino Durumu
int current_type = 0;
int current_rotation = 0;
int current_x = 3;
int current_y = 0;

// Basit Sözde Rastgele Sayı Üreteci (LCG)
unsigned int rand_state = 12345;
int my_rand() {
    rand_state = rand_state * 1103515245 + 12345;
    return (rand_state / 65536) & 32767;
}

// Donanımsal bölme ünitemiz olmadığı için (RV32I) basit bir mod alma fonksiyonu yazıyoruz
int modulo(int a, int b) {
    if (a < 0) a = -a;
    while (a >= b) {
        a -= b;
    }
    return a;
}

// Tek bir kare blok çizme
void draw_board_block(int col, int row, unsigned char color) {
    int start_x = BOARD_X_OFFSET + col * BLOCK_SIZE;
    int start_y = BOARD_Y_OFFSET + row * BLOCK_SIZE;
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            // Blokların sınırlarını belirginleştirmek için kenarlıkları beyaz yapıyoruz
            if (x == 0 || y == 0 || x == BLOCK_SIZE - 1 || y == BLOCK_SIZE - 1) {
                draw_pixel(start_x + x, start_y + y, (color == COLOR_BLACK) ? COLOR_BLACK : COLOR_WHITE);
            } else {
                draw_pixel(start_x + x, start_y + y, color);
            }
        }
    }
}

// Ekranı ve Oyunu Yeniden Çizme
void redraw_game() {
    // Oyun çerçevesini çiz
    for (int y = 0; y < BOARD_ROWS * BLOCK_SIZE + 2; y++) {
        draw_pixel(BOARD_X_OFFSET - 1, BOARD_Y_OFFSET - 1 + y, COLOR_WHITE);
        draw_pixel(BOARD_X_OFFSET + BOARD_COLS * BLOCK_SIZE, BOARD_Y_OFFSET - 1 + y, COLOR_WHITE);
    }
    for (int x = 0; x < BOARD_COLS * BLOCK_SIZE + 2; x++) {
        draw_pixel(BOARD_X_OFFSET - 1 + x, BOARD_Y_OFFSET - 1, COLOR_WHITE);
        draw_pixel(BOARD_X_OFFSET - 1 + x, BOARD_Y_OFFSET + BOARD_ROWS * BLOCK_SIZE, COLOR_WHITE);
    }

    // Oturmuş blokları çiz
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            draw_board_block(c, r, tetromino_colors[board[r][c]]);
        }
    }

    // Hareket eden aktif bloğu çiz
    unsigned short shape = tetrominoes[current_type][current_rotation];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                draw_board_block(current_x + c, current_y + r, tetromino_colors[current_type + 1]);
            }
        }
    }
}

// Çarpışma Testi (Duvarlar veya diğer bloklar)
int check_collision(int next_x, int next_y, int next_rot) {
    unsigned short shape = tetrominoes[current_type][next_rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                int board_x = next_x + c;
                int board_y = next_y + r;
                
                // Sınır kontrolü
                if (board_x < 0 || board_x >= BOARD_COLS || board_y >= BOARD_ROWS) {
                    return 1;
                }
                
                // Başka bloğa çarpma kontrolü
                if (board_y >= 0 && board[board_y][board_x] != 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

// Bloğu tahtaya sabitleme
void place_tetromino() {
    unsigned short shape = tetrominoes[current_type][current_rotation];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                int board_x = current_x + c;
                int board_y = current_y + r;
                if (board_y >= 0 && board_y < BOARD_ROWS) {
                    board[board_y][board_x] = current_type + 1;
                }
            }
        }
    }
}

// Tamamlanmış dolu satırları temizleme
void clear_lines() {
    for (int r = BOARD_ROWS - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] == 0) {
                full = 0;
                break;
            }
        }
        if (full) {
            // Üstteki tüm satırları aşağı kaydır
            for (int sy = r; sy > 0; sy--) {
                for (int sx = 0; sx < BOARD_COLS; sx++) {
                    board[sy][sx] = board[sy-1][sx];
                }
            }
            // En üst satırı sıfırla
            for (int sx = 0; sx < BOARD_COLS; sx++) {
                board[0][sx] = 0;
            }
            r++; // Temizlenen satırdan dolayı aynı satırı tekrar kontrol et
        }
    }
}

// Oyunu sıfırlama
void reset_game() {
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            board[r][c] = 0;
        }
    }
    current_type = modulo(my_rand(), 7);
    current_rotation = 0;
    current_x = 3;
    current_y = 0;
    clear_screen(COLOR_BLACK);
}

int main() {
    reset_game();
    uart_puts("RISC-V SoC Tetris Baslatildi!\r\n");

    int ticks = 0;
    unsigned int last_btn_state = 0;

    while (1) {
        // Buton durumunu oku (MMIO 0x80000004)
        // Bit 0: Center (Döndür)
        // Bit 1: Left (Sola)
        // Bit 2: Right (Sağa)
        // Bit 3: Down (Hızlı Düşür)
        // Bit 4: Up (Sıfırla)
        unsigned int btn = *GPIO_BTNS;
        *GPIO_LEDS = btn; // Butonların durumunu LED'lere yansıt

        // Yükselen kenar buton kontrolü
        unsigned int btn_pressed = btn & ~last_btn_state;
        last_btn_state = btn;

        if (btn_pressed & 0x01) { // Center -> Döndür
            int next_rot = (current_rotation + 1) & 3; // Mod 4 yerine bitwise AND 3
            if (!check_collision(current_x, current_y, next_rot)) {
                current_rotation = next_rot;
            }
        }
        if (btn_pressed & 0x02) { // Left -> Sola Kaydır
            if (!check_collision(current_x - 1, current_y, current_rotation)) {
                current_x--;
            }
        }
        if (btn_pressed & 0x04) { // Right -> Sağa Kaydır
            if (!check_collision(current_x + 1, current_y, current_rotation)) {
                current_x++;
            }
        }
        if (btn & 0x08) { // Down -> Hızlı Düşür (Basılı tutulduğu sürece)
            if (!check_collision(current_x, current_y + 1, current_rotation)) {
                current_y++;
                ticks = 0;
            }
        }
        if (btn_pressed & 0x10) { // Up -> Oyunu Sıfırla
            reset_game();
            uart_puts("Oyun Sifirlandi!\r\n");
        }

        // Otomatik Yerçekimi Düşüşü
        ticks++;
        if (ticks >= 20) {
            ticks = 0;
            if (!check_collision(current_x, current_y + 1, current_rotation)) {
                current_y++;
            } else {
                place_tetromino();
                clear_lines();
                
                // Yeni tetromino üret
                current_type = modulo(my_rand(), 7);
                current_rotation = 0;
                current_x = 3;
                current_y = 0;

                // Oyun bitti mi kontrolü
                if (check_collision(current_x, current_y, current_rotation)) {
                    uart_puts("OYUN BITTI (GAME OVER)!\r\n");
                    reset_game();
                }
            }
        }

        redraw_game();
        delay(30000); // Yaklaşık 30-50ms oyun döngüsü gecikmesi
    }

    return 0;
}
