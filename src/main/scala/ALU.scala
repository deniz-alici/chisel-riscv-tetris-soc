package riscv

import chisel3._
import chisel3.util._

object ALUOp {
  val ADD  = 0.U(4.W)
  val SUB  = 1.U(4.W)
  val SLL  = 2.U(4.W)
  val SLT  = 3.U(4.W)
  val SLTU = 4.U(4.W)
  val XOR  = 5.U(4.W)
  val SRL  = 6.U(4.W)
  val SRA  = 7.U(4.W)
  val OR   = 8.U(4.W)
  val AND  = 9.U(4.W)
  val COPY_B = 10.U(4.W) // LUI veya doğrudan B değerini geçirmek için
}

class ALU extends Module {
  val io = IO(new Bundle {
    val op   = Input(UInt(4.W))
    val inA  = Input(UInt(32.W))
    val inB  = Input(UInt(32.W))
    val out  = Output(UInt(32.W))
    val zero = Output(Bool())
  })

  val outResult = Wire(UInt(32.W))
  outResult := 0.U

  switch(io.op) {
    is(ALUOp.ADD)  { outResult := io.inA + io.inB }
    is(ALUOp.SUB)  { outResult := io.inA - io.inB }
    is(ALUOp.SLL)  { outResult := io.inA << io.inB(4, 0) }
    is(ALUOp.SLT)  { outResult := (io.inA.asSInt < io.inB.asSInt).asUInt }
    is(ALUOp.SLTU) { outResult := (io.inA < io.inB).asUInt }
    is(ALUOp.XOR)  { outResult := io.inA ^ io.inB }
    is(ALUOp.SRL)  { outResult := io.inA >> io.inB(4, 0) }
    is(ALUOp.SRA)  { outResult := (io.inA.asSInt >> io.inB(4, 0)).asUInt }
    is(ALUOp.OR)   { outResult := io.inA | io.inB }
    is(ALUOp.AND)  { outResult := io.inA & io.inB }
    is(ALUOp.COPY_B){ outResult := io.inB }
  }

  io.out  := outResult
  io.zero := (outResult === 0.U)
}
