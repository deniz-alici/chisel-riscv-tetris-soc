/*
 * ============================================================
 *  RV32I için minimal libgcc yardımcıları
 * ============================================================
 *
 *  RV32I taban komut kümesinde donanımsal çarpma/bölme (M eklentisi) YOKTUR.
 *  C kodu (main.c) `*`, `/`, `%` kullandığında derleyici bunları
 *  __mulsi3 / __divsi3 / __modsi3 gibi libgcc rutinlerine çevirir.
 *
 *  Proje `-nostdlib` ile derlendiği için libgcc otomatik linklenmez.
 *  Ayrıca chipyard'ın riscv64 toolchain'inde rv32i libgcc multilib'i de yoktur.
 *  Bu yüzden gereken rutinleri burada kendimiz sağlıyoruz; hepsi yalnızca
 *  toplama/çıkarma/kaydırma/karşılaştırma kullanır (özyineleme/operatör yok).
 */

unsigned int __mulsi3(unsigned int a, unsigned int b) {
    unsigned int r = 0;
    while (b != 0) {
        if (b & 1u) r += a;
        a <<= 1;
        b >>= 1;
    }
    return r;
}

unsigned int __udivsi3(unsigned int n, unsigned int d) {
    if (d == 0) return 0xFFFFFFFFu;   // tanımsız; donmamak için sınır değer
    unsigned int q = 0, r = 0;
    int i;
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) {
            r -= d;
            q |= (1u << i);
        }
    }
    return q;
}

unsigned int __umodsi3(unsigned int n, unsigned int d) {
    if (d == 0) return n;
    unsigned int r = 0;
    int i;
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) {
            r -= d;
        }
    }
    return r;
}

int __divsi3(int a, int b) {
    int neg = 0;
    unsigned int ua = (a < 0) ? (neg ^= 1, (unsigned int)(-a)) : (unsigned int)a;
    unsigned int ub = (b < 0) ? (neg ^= 1, (unsigned int)(-b)) : (unsigned int)b;
    unsigned int uq = __udivsi3(ua, ub);
    return neg ? -(int)uq : (int)uq;
}

int __modsi3(int a, int b) {
    int neg = (a < 0);
    unsigned int ua = (a < 0) ? (unsigned int)(-a) : (unsigned int)a;
    unsigned int ub = (b < 0) ? (unsigned int)(-b) : (unsigned int)b;
    unsigned int ur = __umodsi3(ua, ub);
    return neg ? -(int)ur : (int)ur;
}
