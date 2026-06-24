package riscv

import chisel3._

class Counter(maxVal: Int) extends Module {
  val io = IO(new Bundle {
    val out = Output(UInt(32.W))
  })

  val count = RegInit(0.U(32.W))
  when(count === maxVal.U) {
    count := 0.U
  }.otherwise {
    count := count + 1.U
  }

  io.out := count
}

object CounterMain extends App {
  emitVerilog(new Counter(100))
}
