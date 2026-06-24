// SoCTop tam-sistem simülasyonu (Verilator)
// Amac: CPU gercekten main()'i calistiriyor mu? UART TX pinini 115200 baud'da
// cozup banner'i okuyoruz (donanimdaki pin-swap'tan bagimsiz). Ayrica framebuffer'a
// (0x90000000) yazma olup olmadigini io uzerinden degil; UART banner CPU sagligini kanitlar.
#include "VSoCTop.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <string>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    VSoCTop* top = new VSoCTop;

    const long BITCYC = 868;        // 100 MHz / 115200 baud
    const long MAXCYC = 6000000;    // ~6M cycle ust sinir

    top->clock = 0; top->reset = 1;
    top->io_uartRx = 1;             // bos hat (idle high)
    top->io_buttons = 0;
    top->io_videoPixelClk = 0;
    top->eval();

    int  ust = 0;        // 0=idle, 1=alim
    long ucnt = 0;
    int  ubit = 0;
    unsigned ubyte = 0;
    int  prev_tx = 1;
    std::string out;
    long first_tx_activity = -1;
    unsigned long pcmin = 0xffffffff, pcmax = 0;
    unsigned lastpc = 0; long stuckcnt = 0; unsigned stuckpc = 0;

    for (long cyc = 0; cyc < MAXCYC; cyc++) {
        if (cyc == 20) top->reset = 0;

        // piksel saatini ana saatin yarisinda surdur (video domaini X kalmasin)
        top->io_videoPixelClk = (cyc & 1);

        // yukselen kenar
        top->clock = 1;
        top->eval();

        // ERKEN KOMUT IZI (ilk komutlar)
        if (cyc >= 20 && cyc <= 24) {
            const char* st = top->io_dbgState==0 ? "FETCH" : top->io_dbgState==1 ? "EXEC " : "MEM  ";
            printf("c%-4ld %s pc=0x%04x inst=0x%08x\n", cyc, st, top->io_dbgPc, top->io_dbgInst);
        }

        int tx = top->io_uartTx;
        if (cyc > 50) {
            if (first_tx_activity < 0 && tx == 0) first_tx_activity = cyc;
            if (ust == 0) {
                if (tx == 0 && prev_tx == 1) { ust = 1; ucnt = 0; ubit = 0; ubyte = 0; }
            } else {
                ucnt++;
                long sample = BITCYC * 3 / 2 + (long)ubit * BITCYC;
                if (ucnt == sample) {
                    if (ubit < 8) { ubyte |= (unsigned)(tx & 1) << ubit; ubit++; }
                    if (ubit == 8) {
                        char c = (char)ubyte;
                        out.push_back(c);
                        printf("[%02x %c]", ubyte & 0xff, (c >= 32 && c < 127) ? c : '.');
                        fflush(stdout);
                        ust = 0;
                    }
                }
            }
            prev_tx = tx;
        }

        // PC izleme
        if (cyc > 25) {
            unsigned pcv = top->io_dbgPc;
            if (pcv < pcmin) pcmin = pcv;
            if (pcv > pcmax) pcmax = pcv;
            if (pcv == stuckpc) { stuckcnt++; } else { stuckpc = pcv; stuckcnt = 0; }
            if (cyc % 300000 == 0) printf("\n[cyc %ld pc=0x%08x min=0x%x max=0x%x]\n", cyc, pcv, (unsigned)pcmin, (unsigned)pcmax);
        }

        // dusen kenar
        top->clock = 0;
        top->eval();

        if ((long)out.size() > 250) { printf("\n[yeterince alindi, %ld cycle]\n", cyc); break; }
    }

    printf("\n\n========================================\n");
    printf("UART ilk aktivite (TX=0): %s\n",
           first_tx_activity < 0 ? "HIC YOK (CPU UART'a hic yazmadi)" :
           ("cycle " + std::to_string(first_tx_activity)).c_str());
    printf("Toplam cozulen UART bayt: %zu\n", out.size());
    printf("PC araligi: min=0x%08x max=0x%08x  son=0x%08x  (ayni pc'de %ld cycle takili)\n",
           (unsigned)pcmin, (unsigned)pcmax, (unsigned)top->io_dbgPc, stuckcnt);
    printf("FRAMEBUFFER yazma sayisi: %u  (>0 ise ekrana ciziliyor)\n", (unsigned)top->io_dbgFbWrites);
    printf("========================================\n");
    delete top;
    return 0;
}
