package riscv

import chisel3._
import chisel3.util._

class Core extends Module {
  val io = IO(new Bundle {
    // Buyruk Hafızası Arayüzü (Instruction Memory) - SENKRON (1 cycle gecikmeli)
    val instAddr      = Output(UInt(32.W))
    val inst          = Input(UInt(32.W))

    // Veri Hafızası Arayüzü (Data Memory) - SENKRON (1 cycle gecikmeli)
    val memAddr       = Output(UInt(32.W))
    val memWriteData  = Output(UInt(32.W))
    val memWriteMask  = Output(UInt(4.W))  // Byte bazlı yazma yetkisi (Byte Write Enable)
    val memReadData   = Input(UInt(32.W))
    val memWriteEnable= Output(Bool())
    val memReadEnable = Output(Bool())
  })

  // Alt Modüllerin Tanımlanması
  val regFile = Module(new RegisterFile)
  val decoder = Module(new InstructionDecode)
  val alu     = Module(new ALU)

  // Program Sayacı (Program Counter - PC)
  val pc = RegInit(0.U(32.W))

  // ============================================================
  // ÇOK-CYCLE KONTROL FSM'i (senkron BRAM ile uyumlu)
  //   s_fetch : instAddr=pc sunulur; BRAM 1 cycle sonra komutu verir
  //   s_exec  : komut geçerli -> çöz/yürüt. Store burada yazılır.
  //             Load ise okumayı başlatır ve s_mem'e geçer.
  //   s_mem   : load verisi geçerli -> geri yazılır (yalnız load'lar)
  // Her komut 2 cycle (yük: 3). PC ve register yazımı yalnız 'complete'te.
  // ============================================================
  val s_fetch :: s_exec :: s_mem :: Nil = Enum(3)
  val state = RegInit(s_fetch)

  // 1. BUYRUK ALMA (FETCH) — adres tüm komut boyunca pc'de sabit
  io.instAddr := pc
  decoder.io.inst := io.inst

  // 2. BUYRUK ÇÖZME & YAZMAÇ OKUMA (DECODE & REGISTER READ)
  regFile.io.rs1Addr := decoder.io.rs1Addr
  regFile.io.rs2Addr := decoder.io.rs2Addr

  // 3. YÜRÜTME (EXECUTE - ALU)
  alu.io.op := decoder.io.aluOp
  alu.io.inA := Mux(decoder.io.isAuipc, pc, regFile.io.rs1Data)
  alu.io.inB := Mux(decoder.io.aluSrcB, decoder.io.imm, regFile.io.rs2Data)

  // 4. HAFIZA ERİŞİMİ (MEMORY ACCESS)
  io.memAddr := alu.io.out & ~3.U(32.W)

  val byteSel = alu.io.out(1, 0)
  val funct3  = decoder.io.inst(14, 12)

  val isLoad  = decoder.io.memRead
  val isStore = decoder.io.memWrite

  // Belleğe Yazılacak Veri ve Maske Hazırlığı (Store hizalama)
  val writeData = Wire(UInt(32.W))
  val writeMask = Wire(UInt(4.W))
  writeData := regFile.io.rs2Data
  writeMask := "b0000".U

  switch(funct3) {
    is("b000".U) { // SB (Store Byte)
      writeData := MuxLookup(byteSel, regFile.io.rs2Data(7, 0))(Seq(
        0.U -> regFile.io.rs2Data(7, 0),
        1.U -> (regFile.io.rs2Data(7, 0) << 8),
        2.U -> (regFile.io.rs2Data(7, 0) << 16),
        3.U -> (regFile.io.rs2Data(7, 0) << 24)
      ))
      writeMask := MuxLookup(byteSel, "b0001".U(4.W))(Seq(
        0.U -> "b0001".U,
        1.U -> "b0010".U,
        2.U -> "b0100".U,
        3.U -> "b1000".U
      ))
    }
    is("b001".U) { // SH (Store Halfword)
      writeData := Mux(byteSel(1), regFile.io.rs2Data(15, 0) << 16, regFile.io.rs2Data(15, 0))
      writeMask := Mux(byteSel(1), "b1100".U, "b0011".U)
    }
    is("b010".U) { // SW (Store Word)
      writeData := regFile.io.rs2Data
      writeMask := "b1111".U
    }
  }

  io.memWriteData   := writeData
  io.memWriteMask   := writeMask
  // Store yalnızca s_exec'te (tek cycle yazma).
  // Load okuma-enable'ı s_exec VE s_mem boyunca aktif: BRAM verisi s_mem'de hazır olur,
  // AYRICA MMIO çevrebirimleri (readData'yı yalnız readEnable'da sürer) s_mem'de de
  // geçerli veri döndürür. Yoksa MMIO okumaları (UART status, butonlar) 0 döner.
  io.memWriteEnable := (state === s_exec) && isStore
  io.memReadEnable  := (state =/= s_fetch) && isLoad

  // Bellekten Okunan Veriyi Hizalama (Load hizalama & sign/zero extension)
  val rawData    = io.memReadData
  val loadedData = Wire(UInt(32.W))
  loadedData := rawData

  switch(funct3) {
    is("b000".U) { // LB (İşaretli)
      val byte = MuxLookup(byteSel, rawData(7, 0))(Seq(
        0.U -> rawData(7, 0),
        1.U -> rawData(15, 8),
        2.U -> rawData(23, 16),
        3.U -> rawData(31, 24)
      ))
      loadedData := Cat(Fill(24, byte(7)), byte)
    }
    is("b001".U) { // LH (İşaretli)
      val half = Mux(byteSel(1), rawData(31, 16), rawData(15, 0))
      loadedData := Cat(Fill(16, half(15)), half)
    }
    is("b010".U) { // LW
      loadedData := rawData
    }
    is("b100".U) { // LBU
      val byte = MuxLookup(byteSel, rawData(7, 0))(Seq(
        0.U -> rawData(7, 0),
        1.U -> rawData(15, 8),
        2.U -> rawData(23, 16),
        3.U -> rawData(31, 24)
      ))
      loadedData := Cat(0.U(24.W), byte)
    }
    is("b101".U) { // LHU
      val half = Mux(byteSel(1), rawData(31, 16), rawData(15, 0))
      loadedData := Cat(0.U(16.W), half)
    }
  }

  // 5. GERİ YAZMA (WRITE BACK) — yalnızca komut tamamlandığında
  //    non-load: s_exec sonunda; load: s_mem sonunda (veri o an geçerli)
  val complete = (state === s_exec && !isLoad) || (state === s_mem)

  regFile.io.writeEnable := decoder.io.rfWriteEnable && complete
  regFile.io.rdAddr      := decoder.io.rdAddr
  regFile.io.writeData := Mux(decoder.io.isJump, pc + 4.U,             // JAL/JALR: dönüş adresi
                            Mux(decoder.io.memToReg, loadedData,       // Load verisi
                              alu.io.out))                             // ALU sonucu

  // 6. PROGRAM SAYACI GÜNCELLEME (PC UPDATE) — yalnızca tamamlanınca
  val isBranchTaken = Wire(Bool())
  isBranchTaken := false.B

  switch(funct3) {
    is("b000".U) { isBranchTaken := (regFile.io.rs1Data === regFile.io.rs2Data) } // BEQ
    is("b001".U) { isBranchTaken := (regFile.io.rs1Data =/= regFile.io.rs2Data) } // BNE
    is("b100".U) { isBranchTaken := (regFile.io.rs1Data.asSInt < regFile.io.rs2Data.asSInt) } // BLT
    is("b101".U) { isBranchTaken := (regFile.io.rs1Data.asSInt >= regFile.io.rs2Data.asSInt) } // BGE
    is("b110".U) { isBranchTaken := (regFile.io.rs1Data < regFile.io.rs2Data) } // BLTU
    is("b111".U) { isBranchTaken := (regFile.io.rs1Data >= regFile.io.rs2Data) } // BGEU
  }

  val nextPc = Mux(decoder.io.isJump,
                 Mux(decoder.io.isJalr,
                   (regFile.io.rs1Data + decoder.io.imm) & ~1.U(32.W), // JALR
                   pc + decoder.io.imm),                              // JAL
                 Mux(decoder.io.isBranch && isBranchTaken,
                   pc + decoder.io.imm,                               // Dallanma
                   pc + 4.U))                                         // Normal akış

  when(complete) {
    pc := nextPc
  }

  // ============================================================
  // FSM GEÇİŞLERİ
  // ============================================================
  switch(state) {
    is(s_fetch) { state := s_exec }
    is(s_exec)  { state := Mux(isLoad, s_mem, s_fetch) }
    is(s_mem)   { state := s_fetch }
  }
}
