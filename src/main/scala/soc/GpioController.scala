package soc

import chisel3._

class GpioController extends Module {
  val io = IO(new Bundle {
    // Bus Arayüzü
    val addr        = Input(UInt(32.W))
    val writeData   = Input(UInt(32.W))
    val writeEnable = Input(Bool())
    val readEnable  = Input(Bool())
    val readData    = Output(UInt(32.W))

    // Dış Dünya Sinyalleri
    val leds        = Output(UInt(8.W))
    val buttons     = Input(UInt(5.W))
  })

  // LED yazmacı (8-bit)
  val ledReg = RegInit(0.U(8.W))
  io.leds := ledReg

  // Okuma ve Yazma Mantığı
  io.readData := 0.U

  when(io.writeEnable) {
    // Adres offset 0x0 (örn: 0x80000000) ise LED yaz
    when(io.addr(3, 0) === 0.U) {
      ledReg := io.writeData(7, 0)
    }
  }

  when(io.readEnable) {
    // Adres offset 0x4 (örn: 0x80000004) ise Butonları oku
    when(io.addr(3, 0) === 4.U) {
      io.readData := io.buttons
    }
  }
}
