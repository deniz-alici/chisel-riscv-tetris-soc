package soc

import chisel3._
import chisel3.util._
import riscv._

class SoCTop extends Module {
  val io = IO(new Bundle {
    // UART Arayüzü
    val uartTx      = Output(Bool())
    val uartRx      = Input(Bool())

    // GPIO Arayüzü
    val leds        = Output(UInt(8.W))
    val buttons     = Input(UInt(5.W))

    // HDMI/VGA Video Çıkış Arayüzü
    val videoPixelClk = Input(Clock())
    val videoHsync    = Output(Bool())
    val videoVsync    = Output(Bool())
    val videoVde      = Output(Bool())
    val videoRgb      = Output(UInt(24.W))
  })

  // 1. İŞLEMCİ ÇEKİRDEĞİ VE BİLEŞENLERİN TANIMLANMASI
  val cpu  = Module(new Core)
  val gpio = Module(new GpioController)
  val uart = Module(new UartController(100000000, 115200)) // 100 MHz, 115200 Baud
  val hdmi = Module(new HdmiController)

  // 2. ROM (BUYRUK BELLEĞİ) - 16 KB (4096 Word x 32-bit)
  // sw/program.hex dosyasından başlatılacak
  val rom = SyncReadMem(4096, UInt(32.W))
  // Chisel 6.0.0 uyumlu hex yükleme (Verilog sentezinde BRAM initialization olur)
  chisel3.util.experimental.loadMemoryFromFile(rom, "program.hex")

  // ROM Okuma (Instruction Fetch)
  // pc adresi 4 byte hizalıdır, bu yüzden 2 bit sağa kaydırarak word indeksini alıyoruz
  val cpuPcWordAddr = cpu.io.instAddr(13, 2)
  cpu.io.inst := rom.read(cpuPcWordAddr)

  // 3. RAM (VERİ BELLEĞİ) - 16 KB (4096 Word x 4 Byte)
  // Byte bazlı yazmayı (maske) desteklemek için Vec(4, UInt(8.W)) kullanıyoruz
  val ram = SyncReadMem(4096, Vec(4, UInt(8.W)))

  // 4. ADRES ÇÖZÜMLEME VE BARA YÖNLENDİRME (MEMORY MAPPER)
  val memAddr = cpu.io.memAddr
  
  // Bölgelerin Belirlenmesi
  // Büyük sayıların Scala tarafından negatif olarak algılanmasını önlemek için Long (L) kullanıyoruz
  val isRam   = memAddr >= 0x00004000.U && memAddr < 0x00008000.U // 16KB - 32KB arası RAM
  val isGpio  = memAddr >= 0x80000000L.U && memAddr < 0x80000010L.U // GPIO MMIO
  val isUart  = memAddr >= 0x80000010L.U && memAddr < 0x80000020L.U // UART MMIO
  val isHdmi  = memAddr >= 0x90000000L.U && memAddr < 0x90012C00L.U // Framebuffer MMIO

  // RAM Okuma/Yazma Kontrolleri
  val ramWordAddr = memAddr(13, 2)
  val ramWriteDataVec = VecInit(Seq(
    cpu.io.memWriteData(7, 0),
    cpu.io.memWriteData(15, 8),
    cpu.io.memWriteData(23, 16),
    cpu.io.memWriteData(31, 24)
  ))

  when(cpu.io.memWriteEnable && isRam) {
    ram.write(ramWordAddr, ramWriteDataVec, cpu.io.memWriteMask.asBools)
  }
  
  val ramReadDataVec = ram.read(ramWordAddr, cpu.io.memReadEnable && isRam)
  val ramReadData    = ramReadDataVec.asUInt

  // GPIO Bağlantıları
  gpio.io.addr        := memAddr
  gpio.io.writeData   := cpu.io.memWriteData
  gpio.io.writeEnable := cpu.io.memWriteEnable && isGpio
  gpio.io.readEnable  := cpu.io.memReadEnable && isGpio
  gpio.io.buttons     := io.buttons
  io.leds             := gpio.io.leds

  // UART Bağlantıları
  uart.io.addr        := memAddr
  uart.io.writeData   := cpu.io.memWriteData
  uart.io.writeEnable := cpu.io.memWriteEnable && isUart
  uart.io.readEnable  := cpu.io.memReadEnable && isUart
  io.uartTx           := uart.io.tx
  uart.io.rx          := io.uartRx

  // HDMI Framebuffer Bağlantıları (Port A - CPU yazma/okuma)
  hdmi.io.addr        := memAddr
  hdmi.io.writeData   := cpu.io.memWriteData
  hdmi.io.writeEnable := cpu.io.memWriteEnable && isHdmi
  
  // HDMI Framebuffer (Port B - Video Okuma ve Zamanlama)
  hdmi.io.pixelClk    := io.videoPixelClk
  io.videoHsync       := hdmi.io.hsync
  io.videoVsync       := hdmi.io.vsync
  io.videoVde         := hdmi.io.vde
  io.videoRgb         := hdmi.io.rgb

  // 5. İŞLEMCİ OKUMA VERİSİ MUX'I
  // İşlemci bellekten veri okumak istediğinde adres bölgesine göre veri döndürülür
  cpu.io.memReadData := Mux(isRam, ramReadData,
                          Mux(isGpio, gpio.io.readData,
                            Mux(isUart, uart.io.readData, 
                              0.U(32.W))))
}
