package soc

import chisel3._
import chisel3.util._

class HdmiController extends Module {
  val io = IO(new Bundle {
    // İşlemci Arayüzü (MMIO - Dual Port Bellek)
    // 320x240 = 76800 bayt ekran belleği (Base: 0x90000000)
    val addr        = Input(UInt(32.W))
    val writeData   = Input(UInt(32.W))
    val writeEnable = Input(Bool())
    
    // Video Çıkış Sinyalleri (VGA Seviyesinde, Dışarıda HDMI IP'sine bağlanacak)
    val pixelClk    = Input(Clock())     // 25 MHz Piksel Saati
    val hsync       = Output(Bool())
    val vsync       = Output(Bool())
    val vde         = Output(Bool())     // Video Data Enable (Aktif Alan)
    val rgb         = Output(UInt(24.W)) // 24-bit RGB (R:8, G:8, B:8)
  })

  // 320x240 = 76800 bayt Framebuffer Belleği (Dual-Port BRAM)
  // Renk formatı: 8-bit (R3G3B2) -> 3-bit Red, 3-bit Green, 2-bit Blue
  val fbMem = SyncReadMem(76800, UInt(8.W))

  // ==========================================
  // CPU PORT (Yazma İşlemi)
  // ==========================================
  // 0x90000000 ile 0x90012BFF arası adresler (76800 bayt)
  // Scala'nın taşıma hatası yapmaması için Long tipi (L) kullanıyoruz
  val isFbAddr = io.addr >= 0x90000000L.U && io.addr < 0x90012C00L.U
  val writeAddr = io.addr - 0x90000000L.U

  when(io.writeEnable && isFbAddr) {
    fbMem.write(writeAddr, io.writeData(7, 0))
  }

  // ==========================================
  // VIDEO PORT (Okuma ve Zamanlama İşlemi) - pixelClk saat bölgesinde çalışır
  // ==========================================
  withClock(io.pixelClk) {
    // 640x480 @ 60Hz VGA Zamanlama Parametreleri
    val H_ACTIVE      = 640.U(11.W)
    val H_FRONT_PORCH = 16.U(11.W)
    val H_SYNC        = 96.U(11.W)
    val H_BACK_PORCH  = 48.U(11.W)
    val H_TOTAL       = 800.U(11.W)

    val V_ACTIVE      = 480.U(11.W)
    val V_FRONT_PORCH = 10.U(11.W)
    val V_SYNC        = 2.U(11.W)
    val V_BACK_PORCH  = 33.U(11.W)
    val V_TOTAL       = 525.U(11.W)

    // Yatay ve Dikey Sayaçlar
    val hCount = RegInit(0.U(11.W))
    val vCount = RegInit(0.U(11.W))

    // Zamanlayıcı Sayaç Güncelleme
    hCount := hCount + 1.U
    when(hCount === H_TOTAL - 1.U) {
      hCount := 0.U
      vCount := vCount + 1.U
      when(vCount === V_TOTAL - 1.U) {
        vCount := 0.U
      }
    }

    // Senkronizasyon ve Aktif Alan Sinyalleri (Aktif Düşük - Active Low)
    val hsyncReg = RegNext(~(hCount >= (H_ACTIVE + H_FRONT_PORCH) && hCount < (H_ACTIVE + H_FRONT_PORCH + H_SYNC)))
    val vsyncReg = RegNext(~(vCount >= (V_ACTIVE + V_FRONT_PORCH) && vCount < (V_ACTIVE + V_FRONT_PORCH + V_SYNC)))
    val vdeReg   = RegNext(hCount < H_ACTIVE && vCount < V_ACTIVE)

    io.hsync := hsyncReg
    io.vsync := vsyncReg
    io.vde   := vdeReg

    // Framebuffer Okuma Adresi Hesaplama (320x240 çözünürlüğe ölçekleme: X/2 ve Y/2)
    // Okuma işlemi 1 saat döngüsü gecikmeli olacağı için şimdiki sayaç değerlerini kullanıyoruz
    val nextH = Mux(hCount === H_TOTAL - 1.U, 0.U, hCount + 1.U)
    val nextV = Mux(hCount === H_TOTAL - 1.U, Mux(vCount === V_TOTAL - 1.U, 0.U, vCount + 1.U), vCount)

    val fbX = nextH >> 1
    val fbY = nextV >> 1
    val readAddr = fbY * 320.U + fbX

    // Framebuffer'dan rengi oku (1 clock gecikmeyle veri gelir)
    val pixelColor = fbMem.read(readAddr, nextH < H_ACTIVE && nextV < V_ACTIVE)

    // R3G3B2 formatından 24-bit RGB formatına genişletme
    // R: pixelColor(7,5) -> 8-bit yapmak için sola dayayıp tekrarlıyoruz
    // G: pixelColor(4,2) -> 8-bit
    // B: pixelColor(1,0) -> 8-bit
    val r8 = Cat(pixelColor(7, 5), pixelColor(7, 5), pixelColor(7, 6))
    val g8 = Cat(pixelColor(4, 2), pixelColor(4, 2), pixelColor(4, 3))
    val b8 = Cat(pixelColor(1, 0), pixelColor(1, 0), pixelColor(1, 0), pixelColor(1, 0))

    // Aktif alandaysak okunan rengi bas, yoksa siyah (0.U)
    io.rgb := Mux(vdeReg, Cat(r8, g8, b8), 0.U(24.W))
  }
}
