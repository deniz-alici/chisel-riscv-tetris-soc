# Chisel RISC-V SoC & Tetris on Nexys Video FPGA

This repository contains the design of a RISC-V (RV32I) System-on-Chip (SoC) implemented in **Chisel** (Hardware Construction Language in Scala), compiled to Verilog, and targeted for the **Nexys Video** (Xilinx Artix-7) FPGA development board.

The SoC runs a bare-metal C program (a simple Tetris game) and outputs video directly to a monitor via the Nexys Video board's **HDMI port**.

## Repository Structure

* `src/main/scala/` - Chisel source code for the RISC-V core and SoC peripherals
* `sw/` - Bare-metal C software (Tetris game, bootloader, linker scripts)
* `build.sbt` - Scala Build Tool configuration for the Chisel environment
* `README.md` - Project documentation

## Features

* **CPU Core:** Custom 32-bit single-cycle RISC-V (RV32I) processor core implemented in Chisel.
* **HDMI Output:** Simple framebuffer controller generating video signals for HDMI output.
* **Peripherals:** GPIO (LEDs & Buttons) and UART controllers.
* **Software:** Tetris game written in C, running directly on the custom RISC-V CPU core.
