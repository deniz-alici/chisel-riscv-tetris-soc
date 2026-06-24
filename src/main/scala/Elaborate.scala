package riscv

import chisel3._
import soc.SoCTop

object Elaborate extends App {
  emitVerilog(new SoCTop, Array("--target-dir", "generated"))
}
