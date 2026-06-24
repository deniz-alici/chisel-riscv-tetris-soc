package riscv

import chisel3._
import chisel3.util._

class InstructionDecode extends Module {
  val io = IO(new Bundle {
    val inst          = Input(UInt(32.W))
    
    // Yazmaç Adresleri
    val rs1Addr       = Output(UInt(5.W))
    val rs2Addr       = Output(UInt(5.W))
    val rdAddr        = Output(UInt(5.W))
    
    // Genişletilmiş Anlık Değer (Immediate)
    val imm           = Output(UInt(32.W))
    
    // Kontrol Sinyalleri
    val aluOp         = Output(UInt(4.W))
    val aluSrcB       = Output(Bool())    // false: Reg, true: Imm
    val rfWriteEnable = Output(Bool())    // Register File yazma aktif
    val memWrite      = Output(Bool())    // Hafıza yazma aktif
    val memRead       = Output(Bool())    // Hafıza okuma aktif
    val memToReg      = Output(Bool())    // false: ALU, true: Mem
    val isBranch      = Output(Bool())    // Dallanma buyruğu
    val isJump        = Output(Bool())    // JAL veya JALR
    val isJalr        = Output(Bool())    // JALR buyruğu
    val isLui         = Output(Bool())    // LUI buyruğu
    val isAuipc       = Output(Bool())    // AUIPC buyruğu
  })

  // Opcode, Funct3, Funct7 ayrıştırma
  val opcode = io.inst(6, 0)
  val funct3 = io.inst(14, 12)
  val funct7 = io.inst(31, 25)

  io.rs1Addr := io.inst(19, 15)
  io.rs2Addr := io.inst(24, 20)
  io.rdAddr  := io.inst(11, 7)

  // Immediate (Anlık Değer) Üretici (Sign Extension)
  val immI = Cat(Fill(20, io.inst(31)), io.inst(31, 20))
  val immS = Cat(Fill(20, io.inst(31)), io.inst(31, 25), io.inst(11, 7))
  val immB = Cat(Fill(19, io.inst(31)), io.inst(31), io.inst(7), io.inst(30, 25), io.inst(11, 8), 0.U(1.W))
  val immU = Cat(io.inst(31, 12), 0.U(12.W))
  val immJ = Cat(Fill(11, io.inst(31)), io.inst(31), io.inst(19, 12), io.inst(20), io.inst(30, 21), 0.U(1.W))

  // Varsayılan kontrol sinyalleri
  io.imm           := immI
  io.aluOp         := ALUOp.ADD
  io.aluSrcB       := false.B
  io.rfWriteEnable := false.B
  io.memWrite      := false.B
  io.memRead       := false.B
  io.memToReg      := false.B
  io.isBranch      := false.B
  io.isJump        := false.B
  io.isJalr        := false.B
  io.isLui         := false.B
  io.isAuipc       := false.B

  // Opcode çözme mantığı
  switch(opcode) {
    // R-Type (Register-Register ALU)
    is("b0110011".U) {
      io.rfWriteEnable := true.B
      io.aluSrcB       := false.B
      
      // Funct3 ve Funct7'ye göre ALU işlemini belirleme
      switch(funct3) {
        is("b000".U) { io.aluOp := Mux(funct7(5), ALUOp.SUB, ALUOp.ADD) } // ADD veya SUB
        is("b001".U) { io.aluOp := ALUOp.SLL }
        is("b010".U) { io.aluOp := ALUOp.SLT }
        is("b011".U) { io.aluOp := ALUOp.SLTU }
        is("b100".U) { io.aluOp := ALUOp.XOR }
        is("b101".U) { io.aluOp := Mux(funct7(5), ALUOp.SRA, ALUOp.SRL) } // SRL veya SRA
        is("b110".U) { io.aluOp := ALUOp.OR }
        is("b111".U) { io.aluOp := ALUOp.AND }
      }
    }

    // I-Type ALU (Immediate ALU)
    is("b0010011".U) {
      io.rfWriteEnable := true.B
      io.aluSrcB       := true.B
      io.imm           := immI

      switch(funct3) {
        is("b000".U) { io.aluOp := ALUOp.ADD } // ADDI
        is("b001".U) { io.aluOp := ALUOp.SLL } // SLLI
        is("b010".U) { io.aluOp := ALUOp.SLT } // SLTI
        is("b011".U) { io.aluOp := ALUOp.SLTU } // SLTIU
        is("b100".U) { io.aluOp := ALUOp.XOR } // XORI
        is("b101".U) { io.aluOp := Mux(funct7(5), ALUOp.SRA, ALUOp.SRL) } // SRLI / SRAI
        is("b110".U) { io.aluOp := ALUOp.OR }  // ORI
        is("b111".U) { io.aluOp := ALUOp.AND } // ANDI
      }
    }

    // Load (I-Type Loads)
    is("b0000011".U) {
      io.rfWriteEnable := true.B
      io.aluSrcB       := true.B
      io.imm           := immI
      io.aluOp         := ALUOp.ADD // Adres hesabı: Base Reg + Offset
      io.memRead       := true.B
      io.memToReg      := true.B
    }

    // Store (S-Type Stores)
    is("b0100011".U) {
      io.aluSrcB  := true.B
      io.imm      := immS
      io.aluOp    := ALUOp.ADD // Adres hesabı: Base Reg + Offset
      io.memWrite := true.B
    }

    // Branch (B-Type)
    is("b1100011".U) {
      io.aluSrcB  := false.B
      io.imm      := immB
      io.isBranch := true.B
      
      // Dallanma koşulu kontrolü için ALU çıkarma işlemi yapar
      io.aluOp    := ALUOp.SUB
    }

    // LUI (U-Type)
    is("b0110111".U) {
      io.rfWriteEnable := true.B
      io.aluSrcB       := true.B
      io.imm           := immU
      io.aluOp         := ALUOp.COPY_B
      io.isLui         := true.B
    }

    // AUIPC (U-Type)
    is("b0010111".U) {
      io.rfWriteEnable := true.B
      io.aluSrcB       := true.B
      io.imm           := immU
      io.aluOp         := ALUOp.ADD
      io.isAuipc       := true.B
    }

    // JAL (J-Type)
    is("b1101111".U) {
      io.rfWriteEnable := true.B
      io.imm           := immJ
      io.isJump        := true.B
    }

    // JALR (I-Type)
    is("b1100111".U) {
      io.rfWriteEnable := true.B
      io.aluSrcB       := true.B
      io.imm           := immI
      io.aluOp         := ALUOp.ADD
      io.isJump        := true.B
      io.isJalr        := true.B
    }
  }
}
