/*
 * ============================================================
 *  RISC-V SoC Tetris - Nexys Video FPGA
 *  Chisel ile tasarlanmış özel RV32I işlemci üzerinde çalışır
 * ============================================================
 *
 *  Ekran: 320x240 piksel, 8-bit renk (R3G3B2)
 *  Kontrol: Nexys Video üzerindeki 5 buton
 *  Çıkış: HDMI üzerinden monitöre
 */

#include "soc.h"

#define BOARD_COLS     10
#define BOARD_ROWS     20
#define BLOCK_SIZE     10
#define BOARD_X_OFFSET 60
#define BOARD_Y_OFFSET 20
#define INFO_X         185
#define INFO_Y         30

const unsigned char tetromino_colors[8] = {
    COLOR_BLACK, 0x1F, 0xFC, 0xE3, 0x1C, 0xE0, 0x03, 0xFF
};

const unsigned short tetrominoes[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    {0xCC00, 0xCC00, 0xCC00, 0xCC00},
    {0x4E00, 0x4640, 0x0E40, 0x4C40},
    {0x6C00, 0x4620, 0x06C0, 0x8C40},
    {0xC600, 0x2640, 0x0C60, 0x4C80},
    {0x8E00, 0x6440, 0x0E20, 0x44C0},
    {0x2E00, 0x4460, 0x0E80, 0xC440}
};

unsigned char board[BOARD_ROWS][BOARD_COLS] = {0};
unsigned char old_board[BOARD_ROWS][BOARD_COLS] = {0};

int current_type = 0, current_rotation = 0, current_x = 3, current_y = 0;
int old_x = -1, old_y = -1, old_rotation = -1, old_type = -1;
int next_type = 0;

int score = 0, lines_cleared = 0;
int game_state = 0; 
int pause_selection = 0;

unsigned int rand_state = 12345;
int modulo(int a, int b) {
    if (a < 0) a = -a;
    while (a >= b) a -= b;
    return a;
}

int rand_tetromino() {
    unsigned int r;
    do {
        rand_state = rand_state * 1103515245u + 12345u;
        r = (rand_state >> 24) & 0xFFu;
    } while (r >= 252u);
    return modulo((int)r, 7);
}

void uart_print_num(int n) {
    if (n == 0) { uart_putc('0'); return; }
    char buf[10]; int i = 0, temp = n;
    if (temp < 0) { uart_putc('-'); temp = -temp; }
    while (temp > 0 && i < 10) { buf[i++] = '0' + modulo(temp, 10); temp = temp / 10; }
    for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
}

void draw_hline(int x, int y, int len, unsigned char color) {
    for (int i = 0; i < len; i++) draw_pixel(x + i, y, color);
}

void draw_vline(int x, int y, int len, unsigned char color) {
    for (int i = 0; i < len; i++) draw_pixel(x, y + i, color);
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
    draw_hline(x, y, w, color); draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color); draw_vline(x + w - 1, y, h, color);
}

void fill_rect(int x, int y, int w, int h, unsigned char color) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) draw_pixel(x + col, y + row, color);
    }
}

void draw_board_block(int col, int row, unsigned char color) {
    int sx = BOARD_X_OFFSET + col * BLOCK_SIZE;
    int sy = BOARD_Y_OFFSET + row * BLOCK_SIZE;
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            if (color == COLOR_BLACK) draw_pixel(sx + x, sy + y, 0x00);
            else if (x == 0 || y == 0) draw_pixel(sx + x, sy + y, COLOR_WHITE);
            else if (x == BLOCK_SIZE - 1 || y == BLOCK_SIZE - 1) draw_pixel(sx + x, sy + y, 0x49);
            else draw_pixel(sx + x, sy + y, color);
        }
    }
}

void draw_preview_block(int px, int py, unsigned char color) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) draw_pixel(px + x, py + y, color);
    }
}

const unsigned char digit_bitmaps[10][5] = {
    {0x0E, 0x11, 0x11, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x06, 0x08, 0x1F}, {0x0E, 0x11, 0x06, 0x11, 0x0E},
    {0x12, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x1E},
    {0x0E, 0x10, 0x1E, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x04},
    {0x0E, 0x11, 0x0E, 0x11, 0x0E}, {0x0E, 0x11, 0x0F, 0x01, 0x0E}
};

void draw_digit(int x, int y, int digit, unsigned char color) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            if (digit_bitmaps[digit][row] & (1 << (4 - col))) draw_pixel(x + col, y + row, color);
        }
    }
}

void draw_number(int x, int y, int num, unsigned char color) {
    char digits[6] = {0}; int temp = num;
    if (temp < 0) temp = 0;
    for (int i = 5; i >= 0; i--) { digits[i] = modulo(temp, 10); temp = temp / 10; }
    int started = 0;
    for (int i = 0; i < 6; i++) {
        if (digits[i] != 0 || i == 5) started = 1;
        if (started) draw_digit(x + i * 6, y, digits[i], color);
    }
}

void draw_title_screen() {
    clear_screen(0x00);
    draw_hline(20, 20, 280, 0x1F); draw_hline(20, 22, 280, 0x03);

    int tx = 65, ty = 40; unsigned char tc = 0xFC; 
    fill_rect(tx, ty, 18, 6, tc); fill_rect(tx + 6, ty + 6, 6, 18, tc); tx += 22;
    tc = 0x1C; fill_rect(tx, ty, 6, 24, tc); fill_rect(tx, ty, 16, 6, tc); fill_rect(tx, ty + 9, 12, 6, tc); fill_rect(tx, ty + 18, 16, 6, tc); tx += 20;
    tc = 0x1F; fill_rect(tx, ty, 18, 6, tc); fill_rect(tx + 6, ty + 6, 6, 18, tc); tx += 22;
    tc = 0xE0; fill_rect(tx, ty, 6, 24, tc); fill_rect(tx, ty, 14, 6, tc); fill_rect(tx + 14, ty + 6, 6, 6, tc); fill_rect(tx, ty + 12, 14, 6, tc); fill_rect(tx + 10, ty + 18, 6, 6, tc); tx += 22;
    tc = 0xE3; fill_rect(tx, ty, 14, 6, tc); fill_rect(tx + 4, ty + 6, 6, 12, tc); fill_rect(tx, ty + 18, 14, 6, tc); tx += 18;
    tc = 0xFC; fill_rect(tx, ty, 16, 6, tc); fill_rect(tx, ty + 6, 6, 6, tc); fill_rect(tx, ty + 12, 16, 6, tc); fill_rect(tx + 10, ty + 18, 6, 6, tc); fill_rect(tx, ty + 24, 16, 6, tc);

    draw_hline(20, 72, 280, 0x03); draw_hline(20, 74, 280, 0x1F);

    fill_rect(20, 90, 10, 10, 0xE0); fill_rect(30, 90, 10, 10, 0xE0); fill_rect(20, 100, 10, 10, 0xE0); fill_rect(30, 100, 10, 10, 0xE0); 
    fill_rect(270, 90, 10, 30, 0x1F); 
    fill_rect(20, 160, 10, 30, 0x1C); fill_rect(30, 180, 10, 10, 0x1C); 

    fill_rect(80, 90, 160, 18, 0x03); draw_rect(80, 90, 160, 18, 0x1F);
    
    int iy = 120;
    fill_rect(50, iy, 220, 14, 0x49); draw_rect(50, iy, 220, 14, COLOR_WHITE);
    iy = 145; int spacing = 18;
    fill_rect(60, iy, 10, 10, 0x1C); iy += spacing; 
    fill_rect(60, iy, 10, 10, 0x1C); iy += spacing; 
    fill_rect(60, iy, 10, 10, 0xFC); iy += spacing; 
    fill_rect(60, iy, 10, 10, 0xE0); iy += spacing; 
    fill_rect(60, iy, 10, 10, 0xE3);                

    fill_rect(40, 222, 240, 14, 0x00); draw_rect(40, 222, 240, 14, 0xFC);
    draw_rect(10, 10, 300, 228, 0x49); draw_rect(12, 12, 296, 224, 0x03);
}

void draw_game_over_screen() {
    for (int y = 60; y < 180; y++) {
        for (int x = 40; x < 280; x++) draw_pixel(x, y, 0x00);
    }
    draw_rect(45, 65, 230, 110, COLOR_WHITE); draw_rect(47, 67, 226, 106, 0xE0); fill_rect(48, 68, 224, 104, 0x00);
    fill_rect(70, 80, 14, 4, 0xE0); fill_rect(70, 80, 4, 20, 0xE0); fill_rect(80, 80, 4, 20, 0xE0); fill_rect(70, 96, 14, 4, 0xE0); 
    fill_rect(88, 80, 4, 10, 0xE0); fill_rect(100, 80, 4, 10, 0xE0); fill_rect(92, 90, 4, 10, 0xE0); 
    fill_rect(108, 80, 4, 20, 0xE0); fill_rect(120, 80, 4, 20, 0xE0); fill_rect(108, 96, 16, 4, 0xE0); 
    fill_rect(128, 80, 4, 20, 0xE0); fill_rect(140, 80, 4, 20, 0xE0); fill_rect(132, 84, 4, 4, 0xE0); fill_rect(136, 88, 4, 4, 0xE0); 

    fill_rect(90, 115, 140, 16, 0x03); draw_rect(90, 115, 140, 16, 0x1F);
    draw_number(160, 120, score, COLOR_WHITE);
    fill_rect(75, 145, 170, 14, 0x49); draw_rect(75, 145, 170, 14, 0xFC);
}

void draw_pause_menu() {
    fill_rect(80, 80, 160, 80, 0x00); draw_rect(80, 80, 160, 80, COLOR_WHITE); draw_rect(82, 82, 156, 76, 0x1F); 
    fill_rect(100, 90, 120, 14, 0x49); // DURAKLATILDI Text Placeholder
    
    // DEVAM ET (Sol)
    fill_rect(90, 120, 65, 20, pause_selection == 0 ? 0xFC : 0x00); 
    draw_rect(90, 120, 65, 20, 0xFC);
    fill_rect(115, 127, 10, 6, pause_selection == 0 ? 0x00 : 0xFC); // Small indicator
    
    // CIKIS (Sag)
    fill_rect(165, 120, 65, 20, pause_selection == 1 ? 0xE0 : 0x00); 
    draw_rect(165, 120, 65, 20, 0xE0);
    fill_rect(190, 127, 10, 6, pause_selection == 1 ? 0x00 : 0xE0); // Small indicator
}

void draw_info_panel(int force) {
    static int old_score = -1, old_lines = -1, old_next = -1;
    if (force || score != old_score) {
        fill_rect(INFO_X, INFO_Y, 70, 30, 0x00); draw_rect(INFO_X, INFO_Y, 70, 30, 0x1F);
        draw_number(INFO_X + 10, INFO_Y + 14, score, 0xFC); old_score = score;
    }
    if (force || lines_cleared != old_lines) {
        fill_rect(INFO_X, INFO_Y + 40, 70, 30, 0x00); draw_rect(INFO_X, INFO_Y + 40, 70, 30, 0x1C);
        draw_number(INFO_X + 10, INFO_Y + 54, lines_cleared, 0x1C); old_lines = lines_cleared;
    }
    if (force || next_type != old_next) {
        fill_rect(INFO_X, INFO_Y + 80, 70, 50, 0x00); draw_rect(INFO_X, INFO_Y + 80, 70, 50, 0x49);
        unsigned short next_shape = tetrominoes[next_type][0];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (next_shape & (1 << (15 - (r * 4 + c)))) {
                    draw_preview_block(INFO_X + 27 + c * 5, INFO_Y + 95 + r * 5, tetromino_colors[next_type + 1]);
                }
            }
        }
        old_next = next_type;
    }
}

void draw_game_frame() {
    int frame_x = BOARD_X_OFFSET - 2, frame_y = BOARD_Y_OFFSET - 2;
    int frame_w = BOARD_COLS * BLOCK_SIZE + 4, frame_h = BOARD_ROWS * BLOCK_SIZE + 4;
    draw_rect(frame_x, frame_y, frame_w, frame_h, COLOR_WHITE);
    draw_rect(frame_x - 1, frame_y - 1, frame_w + 2, frame_h + 2, 0x49);
}

void redraw_game(int force_all) {
    if (force_all) {
        draw_game_frame();
        for(int r=0; r<BOARD_ROWS; r++) for(int c=0; c<BOARD_COLS; c++) old_board[r][c] = 99;
    }

    if (old_x != -1 && (old_x != current_x || old_y != current_y || old_rotation != current_rotation || old_type != current_type)) {
        unsigned short shape = tetrominoes[old_type][old_rotation];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (shape & (1 << (15 - (r * 4 + c)))) {
                    int bx = old_x + c; int by = old_y + r;
                    if (by >= 0 && by < BOARD_ROWS && bx >= 0 && bx < BOARD_COLS) {
                        draw_board_block(bx, by, tetromino_colors[board[by][bx]]);
                    }
                }
            }
        }
    }

    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] != old_board[r][c]) {
                draw_board_block(c, r, tetromino_colors[board[r][c]]);
                old_board[r][c] = board[r][c];
            }
        }
    }

    unsigned short shape = tetrominoes[current_type][current_rotation];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                draw_board_block(current_x + c, current_y + r, tetromino_colors[current_type + 1]);
            }
        }
    }

    old_x = current_x; old_y = current_y; old_rotation = current_rotation; old_type = current_type;
    draw_info_panel(force_all);
}

int check_collision(int nx, int ny, int nrot) {
    unsigned short shape = tetrominoes[current_type][nrot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                int bx = nx + c; int by = ny + r;
                if (bx < 0 || bx >= BOARD_COLS || by >= BOARD_ROWS) return 1;
                if (by >= 0 && board[by][bx] != 0) return 1;
            }
        }
    }
    return 0;
}

void place_tetromino() {
    unsigned short shape = tetrominoes[current_type][current_rotation];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape & (1 << (15 - (r * 4 + c)))) {
                int bx = current_x + c; int by = current_y + r;
                if (by >= 0 && by < BOARD_ROWS) {
                    board[by][bx] = current_type + 1;
                    old_board[by][bx] = 99; 
                }
            }
        }
    }
}

void clear_lines() {
    int cleared = 0;
    for (int r = BOARD_ROWS - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] == 0) { full = 0; break; }
        }
        if (full) {
            cleared++;
            for (int sy = r; sy > 0; sy--) {
                for (int sx = 0; sx < BOARD_COLS; sx++) { board[sy][sx] = board[sy - 1][sx]; old_board[sy][sx] = 99; }
            }
            for (int sx = 0; sx < BOARD_COLS; sx++) { board[0][sx] = 0; old_board[0][sx] = 99; }
            r++; 
        }
    }
    if (cleared > 0) {
        lines_cleared += cleared;
        if (cleared == 1) score += 100;
        else if (cleared == 2) score += 300;
        else if (cleared == 3) score += 500;
        else score += 800;
    }
}

void spawn_new_tetromino() {
    current_type = next_type;
    next_type = rand_tetromino();
    current_rotation = 0; current_x = 3; current_y = 0;
    old_x = -1; 
}

void reset_game() {
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) { board[r][c] = 0; old_board[r][c] = 99; }
    }
    score = 0; lines_cleared = 0;
    next_type = rand_tetromino();
    spawn_new_tetromino();
    clear_screen(COLOR_BLACK);
}

int main() {
    game_state = 0;
    draw_title_screen();

    unsigned int last_btn = 0;
    unsigned int drop_timer = 0;
    
    // Oyun hizini arttirdik (350000 -> 180000)
    unsigned int drop_speed = 180000; 

    while (1) {
        unsigned int btn = *GPIO_BTNS;
        *GPIO_LEDS = btn; 
        unsigned int pressed = btn & ~last_btn;
        last_btn = btn;

        if (game_state == 0) {
            if (pressed != 0) {
                game_state = 1;
                reset_game();
                redraw_game(1);
            }
            continue;
        }

        if (game_state == 2) {
            if (pressed & 0x10) { 
                game_state = 1;
                reset_game();
                redraw_game(1);
            }
            continue;
        }

        if (game_state == 3) {
            if (pressed & 0x02) { pause_selection = 0; draw_pause_menu(); } 
            if (pressed & 0x04) { pause_selection = 1; draw_pause_menu(); } 
            if (pressed & 0x01) { 
                if (pause_selection == 0) {
                    game_state = 1;
                    clear_screen(COLOR_BLACK);
                    redraw_game(1); 
                } else {
                    game_state = 0;
                    draw_title_screen();
                }
            }
            continue;
        }

        int needs_redraw = 0;

        if (pressed & 0x01) { 
            int next_rot = (current_rotation + 1) & 3;
            if (!check_collision(current_x, current_y, next_rot)) {
                current_rotation = next_rot; needs_redraw = 1;
            }
        }
        if (pressed & 0x02) { 
            if (!check_collision(current_x - 1, current_y, current_rotation)) {
                current_x--; needs_redraw = 1;
            }
        }
        if (pressed & 0x04) { 
            if (!check_collision(current_x + 1, current_y, current_rotation)) {
                current_x++; needs_redraw = 1;
            }
        }
        if (pressed & 0x10) { 
            game_state = 3;
            pause_selection = 0;
            draw_pause_menu();
            continue;
        }

        // Yumusak Asagi Dusurme Hizlandiricisi (Isinlanma Yerine Sadece Hizlanir)
        int timer_increment = 1;
        if (btn & 0x08) { 
            timer_increment = 15; // Asagi tusu basiliyken 15x daha hizli dus
        }
        drop_timer += timer_increment;

        if (drop_timer > drop_speed) {
            drop_timer = 0;
            if (!check_collision(current_x, current_y + 1, current_rotation)) {
                current_y++;
                needs_redraw = 1;
            } else {
                place_tetromino();
                clear_lines();
                spawn_new_tetromino();
                needs_redraw = 1;
                
                if (check_collision(current_x, current_y, current_rotation)) {
                    game_state = 2;
                    draw_game_over_screen();
                    continue;
                }
            }
        }

        if (needs_redraw) {
            redraw_game(0); 
        }
    }
    return 0;
}
