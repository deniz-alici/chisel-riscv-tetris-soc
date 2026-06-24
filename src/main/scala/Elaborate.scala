package riscv

import chisel3._

object Elaborate extends App {
  emitVerilog(new Core, Array("--target-dir", "generated"))
}
