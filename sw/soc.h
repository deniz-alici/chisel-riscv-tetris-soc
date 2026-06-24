#ifndef SOC_H
#define SOC_H

// MMIO Adres Tanımlamaları
#define GPIO_LEDS     ((volatile unsigned int*)0x80000000)
#define GPIO_BTNS     ((volatile unsigned int*)0x80000004)

#define UART_DATA     ((volatile unsigned int*)0x80000010)
#define UART_STATUS   ((volatile unsigned int*)0x80000014)

#define VRAM_BASE     ((volatile unsigned char*)0x90000000)
#define VRAM_WIDTH    320
#define VRAM_HEIGHT   240

// 8-bit Renk Kodları (R3G3B2 formatında)
#define COLOR_BLACK   0x00
#define COLOR_RED     0xE0   // 111 000 00
#define COLOR_GREEN   0x1C   // 000 111 00
#define COLOR_BLUE    0x03   // 000 000 11
#define COLOR_YELLOW  0xFC   // 111 111 00
#define COLOR_CYAN    0x1F   // 000 111 11
#define COLOR_MAGENTA 0xE3   // 111 000 11
#define COLOR_WHITE   0xFF   // 111 111 11
#define COLOR_GRAY    0x49   // Orta gri

// Basit gecikme fonksiyonu (Saat döngüsü bazlı)
static inline void delay(volatile int cycles) {
    while (cycles > 0) {
        cycles--;
    }
}

// UART karakter gönderme
static inline void uart_putc(char c) {
    // tx_busy (bit 0) 0 olana kadar bekle
    while (*UART_STATUS & 0x1);
    *UART_DATA = c;
}

// UART metin gönderme
static inline void uart_puts(const char* s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// Ekrana piksel çizme
static inline void draw_pixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < VRAM_WIDTH && y >= 0 && y < VRAM_HEIGHT) {
        VRAM_BASE[y * VRAM_WIDTH + x] = color;
    }
}

// Ekranı temizleme (tek renk ile doldurma)
static inline void clear_screen(unsigned char color) {
    for (int i = 0; i < VRAM_WIDTH * VRAM_HEIGHT; i++) {
        VRAM_BASE[i] = color;
    }
}

#endif // SOC_H
