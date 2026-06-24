package riscv

import chisel3._
import chisel3.util._

class Core extends Module {
  val io = IO(new Bundle {
    // Buyruk Hafızası Arayüzü (Instruction Memory)
    val instAddr      = Output(UInt(32.W))
    val inst          = Input(UInt(32.W))

    // Veri Hafızası Arayüzü (Data Memory)
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

  // 1. BUYRUK ALMA (FETCH)
  io.instAddr := pc
  decoder.io.inst := io.inst

  // 2. BUYRUK ÇÖZME & YAZMAÇ OKUMA (DECODE & REGISTER READ)
  regFile.io.rs1Addr := decoder.io.rs1Addr
  regFile.io.rs2Addr := decoder.io.rs2Addr

  // 3. YÜRÜTME (EXECUTE - ALU)
  alu.io.op := decoder.io.aluOp
  
  // ALU Giriş A Seçici (AUIPC için PC, yoksa rs1)
  alu.io.inA := Mux(decoder.io.isAuipc, pc, regFile.io.rs1Data)
  
  // ALU Giriş B Seçici (Immediate veya rs2)
  alu.io.inB := Mux(decoder.io.aluSrcB, decoder.io.imm, regFile.io.rs2Data)

  // 4. HAFIZA ERİŞİMİ (MEMORY ACCESS)
  // Bellek adresi her zaman 32-bit word hizalı olmalıdır (Son 2 biti sıfırlıyoruz)
  io.memAddr := alu.io.out & ~3.U(32.W)
  
  val byteSel = alu.io.out(1, 0)
  val funct3  = decoder.io.inst(14, 12)

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
  io.memWriteEnable := decoder.io.memWrite
  io.memReadEnable  := decoder.io.memRead

  // Bellekten Okunan Veriyi Hizalama (Load hizalama & sign/zero extension)
  val rawData    = io.memReadData
  val loadedData = Wire(UInt(32.W))
  loadedData := rawData

  switch(funct3) {
    is("b000".U) { // LB (Load Byte - İşaretli)
      val byte = MuxLookup(byteSel, rawData(7, 0))(Seq(
        0.U -> rawData(7, 0),
        1.U -> rawData(15, 8),
        2.U -> rawData(23, 16),
        3.U -> rawData(31, 24)
      ))
      loadedData := Cat(Fill(24, byte(7)), byte)
    }
    is("b001".U) { // LH (Load Halfword - İşaretli)
      val half = Mux(byteSel(1), rawData(31, 16), rawData(15, 0))
      loadedData := Cat(Fill(16, half(15)), half)
    }
    is("b010".U) { // LW (Load Word)
      loadedData := rawData
    }
    is("b100".U) { // LBU (Load Byte Unsigned)
      val byte = MuxLookup(byteSel, rawData(7, 0))(Seq(
        0.U -> rawData(7, 0),
        1.U -> rawData(15, 8),
        2.U -> rawData(23, 16),
        3.U -> rawData(31, 24)
      ))
      loadedData := Cat(0.U(24.W), byte)
    }
    is("b101".U) { // LHU (Load Halfword Unsigned)
      val half = Mux(byteSel(1), rawData(31, 16), rawData(15, 0))
      loadedData := Cat(0.U(16.W), half)
    }
  }

  // 5. GERİ YAZMA (WRITE BACK)
  regFile.io.writeEnable := decoder.io.rfWriteEnable
  regFile.io.rdAddr      := decoder.io.rdAddr
  
  // Yazmac Yazılacak Veri Seçici
  regFile.io.writeData := Mux(decoder.io.isJump, pc + 4.U,             // JAL/JALR için dönüş adresi (PC+4)
                            Mux(decoder.io.memToReg, loadedData,       // Load için bellek verisi
                              alu.io.out))                             // ALU sonucu (LUI, AUIPC, R/I ALU)

  // 6. PROGRAM SAYACI GÜNCELLEME (PC UPDATE)
  // Dallanma (Branch) karar mekanizması
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

  // Sonraki PC Değeri Seçici
  val nextPc = Mux(decoder.io.isJump,
                 Mux(decoder.io.isJalr,
                   (regFile.io.rs1Data + decoder.io.imm) & ~1.U(32.W), // JALR
                   pc + decoder.io.imm),                              // JAL
                 Mux(decoder.io.isBranch && isBranchTaken,
                   pc + decoder.io.imm,                               // Dallanma gerçekleşti
                   pc + 4.U))                                         // Normal akış (PC + 4)

  pc := nextPc
}
