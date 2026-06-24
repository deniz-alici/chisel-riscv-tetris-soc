package riscv

import chisel3._

class RegisterFile extends Module {
  val io = IO(new Bundle {
    val rs1Addr     = Input(UInt(5.W))
    val rs2Addr     = Input(UInt(5.W))
    val rdAddr      = Input(UInt(5.W))
    val writeData   = Input(UInt(32.W))
    val writeEnable = Input(Bool())
    
    val rs1Data     = Output(UInt(32.W))
    val rs2Data     = Output(UInt(32.W))
  })

  // 32 adet 32-bitlik yazmaç deposu (x0-x31)
  val regs = RegInit(VecInit(Seq.fill(32)(0.U(32.W))))

  // Yazma işlemi (x0 yazmacına yazma işlemi engellenir)
  when(io.writeEnable && io.rdAddr =/= 0.U) {
    regs(io.rdAddr) := io.writeData
  }

  // Okuma işlemi (x0 her zaman 0 döner)
  io.rs1Data := Mux(io.rs1Addr === 0.U, 0.U, regs(io.rs1Addr))
  io.rs2Data := Mux(io.rs2Addr === 0.U, 0.U, regs(io.rs2Addr))
}
