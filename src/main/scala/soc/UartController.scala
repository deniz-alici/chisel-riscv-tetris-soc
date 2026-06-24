package soc

import chisel3._
import chisel3.util._

class UartController(val clockFreqHz: Int = 100000000, val baudRate: Int = 115200) extends Module {
  val io = IO(new Bundle {
    // Bus Arayüzü (MMIO)
    val addr        = Input(UInt(32.W))
    val writeData   = Input(UInt(32.W))
    val writeEnable = Input(Bool())
    val readEnable  = Input(Bool())
    val readData    = Output(UInt(32.W))

    // Dış Dünya Seri Hatları (UART Pins)
    val tx          = Output(Bool())
    val rx          = Input(Bool())
  })

  // Baud Rate Bölücü Sınır Değeri (Örn: 100MHz / 115200 ≈ 868)
  val CLK_PER_BIT = (clockFreqHz / baudRate).U(16.W)

  // ==========================================
  // UART TRANSMITTER (TX) - Verici
  // ==========================================
  val txIdle :: txStart :: txData :: txStop :: Nil = Enum(4)
  val txState     = RegInit(txIdle)
  val txClkCount  = RegInit(0.U(16.W))
  val txBitIndex  = RegInit(0.U(3.W))
  val txBuffer    = RegInit(0.U(8.W))
  val txReg       = RegInit(true.B) // Serial hat boştayken HIGH (Idle)
  io.tx := txReg

  val txBusy = txState =/= txIdle

  when(txState === txIdle) {
    // İşlemciden yazma talebi geldiğinde ve TX meşgul değilse başla
    // Adres offset 0x10 (örn: 0x80000010) ise veri gönderilir
    when(io.writeEnable && (io.addr(7, 0) === 0x10.U)) {
      txBuffer   := io.writeData(7, 0)
      txState    := txStart
      txClkCount := 0.U
      txReg      := false.B // Başlangıç biti (Start Bit = LOW)
    }
  }.otherwise {
    txClkCount := txClkCount + 1.U
    when(txClkCount === CLK_PER_BIT - 1.U) {
      txClkCount := 0.U
      
      switch(txState) {
        is(txStart) {
          txState    := txData
          txBitIndex := 0.U
          txReg      := txBuffer(0) // İlk bit gönderilir
        }
        is(txData) {
          txBitIndex := txBitIndex + 1.U
          when(txBitIndex === 7.U) {
            txState := txStop
            txReg   := true.B // Durdurma biti (Stop Bit = HIGH)
          }.otherwise {
            // Bir sonraki biti gönder (Veriyi kaydırarak)
            val nextData = txBuffer >> (txBitIndex + 1.U)
            txReg := nextData(0)
          }
        }
        is(txStop) {
          txState := txIdle
        }
      }
    }
  }

  // ==========================================
  // UART RECEIVER (RX) - Alıcı
  // ==========================================
  val rxIdle :: rxStart :: rxData :: rxStop :: Nil = Enum(4)
  val rxState      = RegInit(rxIdle)
  val rxClkCount   = RegInit(0.U(16.W))
  val rxBitIndex   = RegInit(0.U(3.W))
  val rxBuffer     = RegInit(0.U(8.W))
  val rxValid      = RegInit(false.B) // Yeni veri alındı bayrağı
  val rxDataReg    = RegInit(0.U(8.W)) // Alınan veriyi tutan yazmaç

  // RX hattındaki dalgalanmaları önlemek için çift Flip-Flop (Debounce/Synchronizer)
  val rxSync = RegNext(RegNext(io.rx, true.B), true.B)

  when(rxState === rxIdle) {
    // Start bit algılandığında (HIGH -> LOW geçişi)
    when(!rxSync) {
      rxState    := rxStart
      rxClkCount := 0.U
    }
  }.otherwise {
    rxClkCount := rxClkCount + 1.U
    
    switch(rxState) {
      is(rxStart) {
        // Start bitinin tam ortasında doğruluğunu kontrol et (Baud periyodunun yarısında)
        when(rxClkCount === (CLK_PER_BIT >> 1) - 1.U) {
          when(!rxSync) { // Hâlâ LOW ise başlangıç geçerlidir
            rxClkCount := 0.U
            rxState    := rxData
            rxBitIndex := 0.U
          }.otherwise {
            rxState    := rxIdle // Parazit algılandı, boşa dön
          }
        }
      }
      is(rxData) {
        // Her bit periyodunun tam ortasında örnekleme yap
        when(rxClkCount === CLK_PER_BIT - 1.U) {
          rxClkCount := 0.U
          rxBuffer   := Cat(rxSync, rxBuffer(7, 1)) // Veriyi sağa kaydırarak ekle
          rxBitIndex := rxBitIndex + 1.U
          
          when(rxBitIndex === 7.U) {
            rxState := rxStop
          }
        }
      }
      is(rxStop) {
        // Stop bitinin ortasında işlemi tamamla
        when(rxClkCount === CLK_PER_BIT - 1.U) {
          when(rxSync) { // Stop biti HIGH olmalı
            rxDataReg := rxBuffer
            rxValid   := true.B
          }
          rxState := rxIdle
        }
      }
    }
  }

  // ==========================================
  // CPU BARA OKUMA VE YAZMA BAĞLANTISI (MMIO)
  // ==========================================
  io.readData := 0.U

  // İşlemci UART Data okuduğunda rxValid temizlenir
  val readDataReg = io.readEnable && (io.addr(7, 0) === 0x10.U)
  when(readDataReg) {
    rxValid := false.B
  }

  when(io.readEnable) {
    switch(io.addr(7, 0)) {
      // 0x80000010 -> UART Veri Okuma
      is(0x10.U) { io.readData := Cat(0.U(24.W), rxDataReg) }
      // 0x80000014 -> UART Durum Okuma (Bit 0: txBusy, Bit 1: rxValid)
      is(0x14.U) { io.readData := Cat(0.U(30.W), rxValid, txBusy) }
    }
  }
}
