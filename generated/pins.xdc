## =============================================================================
## Nexys Video RISC-V SoC & Tetris - constraints (pins.xdc)
## Board: Digilent Nexys Video (Artix-7 XC7A200T-1SBG484C)
## =============================================================================

## ---- System Clock (100 MHz) ----
set_property -dict { PACKAGE_PIN R4    IOSTANDARD LVCMOS33 } [get_ports { sys_clk }]
create_clock -period 10.000 -name sys_clk [get_ports sys_clk]

## ---- UART ----
set_property -dict { PACKAGE_PIN AA19  IOSTANDARD LVCMOS33 } [get_ports { uart_rx }]
set_property -dict { PACKAGE_PIN V18   IOSTANDARD LVCMOS33 } [get_ports { uart_tx }]

## ---- LEDs ----
set_property -dict { PACKAGE_PIN T14   IOSTANDARD LVCMOS25 } [get_ports { led[0] }]
set_property -dict { PACKAGE_PIN T15   IOSTANDARD LVCMOS25 } [get_ports { led[1] }]
set_property -dict { PACKAGE_PIN T16   IOSTANDARD LVCMOS25 } [get_ports { led[2] }]
set_property -dict { PACKAGE_PIN U16   IOSTANDARD LVCMOS25 } [get_ports { led[3] }]
set_property -dict { PACKAGE_PIN V15   IOSTANDARD LVCMOS25 } [get_ports { led[4] }]
set_property -dict { PACKAGE_PIN W16   IOSTANDARD LVCMOS25 } [get_ports { led[5] }]
set_property -dict { PACKAGE_PIN W15   IOSTANDARD LVCMOS25 } [get_ports { led[6] }]
set_property -dict { PACKAGE_PIN Y13   IOSTANDARD LVCMOS25 } [get_ports { led[7] }]

## ---- Buttons ----
set_property -dict { PACKAGE_PIN B22   IOSTANDARD LVCMOS12 } [get_ports { btn[0] }] ;# btnC (Center) - Tetromino Döndür
set_property -dict { PACKAGE_PIN C22   IOSTANDARD LVCMOS12 } [get_ports { btn[1] }] ;# btnL (Left)   - Sola Hareket
set_property -dict { PACKAGE_PIN B21   IOSTANDARD LVCMOS12 } [get_ports { btn[2] }] ;# btnR (Right)  - Sağa Hareket
set_property -dict { PACKAGE_PIN D21   IOSTANDARD LVCMOS12 } [get_ports { btn[3] }] ;# btnD (Down)   - Hızlı Düşür
set_property -dict { PACKAGE_PIN D22   IOSTANDARD LVCMOS12 } [get_ports { btn[4] }] ;# btnU (Up)     - Sıfırla

## ---- HDMI TX (Output to Monitor) ----
## TMDS Clock
set_property -dict { PACKAGE_PIN T1    IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_clk_p }]
set_property -dict { PACKAGE_PIN U1    IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_clk_n }]

## TMDS Data Channel 0 (Blue)
set_property -dict { PACKAGE_PIN W1    IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_p[0] }]
set_property -dict { PACKAGE_PIN Y1    IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_n[0] }]

## TMDS Data Channel 1 (Green)
set_property -dict { PACKAGE_PIN AA1   IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_p[1] }]
set_property -dict { PACKAGE_PIN AB1   IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_n[1] }]

## TMDS Data Channel 2 (Red)
set_property -dict { PACKAGE_PIN AB3   IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_p[2] }]
set_property -dict { PACKAGE_PIN AB2   IOSTANDARD TMDS_33 } [get_ports { hdmi_tx_n[2] }]

## ---- Bitstream Configuration ----
set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
