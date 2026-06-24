# =============================================================================
# Vivado TCL Script - RISC-V SoC Tetris Projesi
# Sadece FPGA Programlama
# =============================================================================

set project_name    "riscv_soc_tetris"
set project_dir     "C:/vivado_runs/riscv_soc_tetris"
set bitstream_file "$project_dir/$project_name.runs/impl_1/TopWrapper.bit"

if {![file exists $bitstream_file]} {
    puts "UYARI: Bitstream dosyasi beklenen yolda bulunamadi, aranıyor..."
    set bitstream_file [glob -nocomplain "$project_dir/$project_name.runs/impl_1/*.bit"]
    if {$bitstream_file eq ""} {
        puts "HATA: Bitstream dosyasi bulunamadi! Lutfen once vivado_build.tcl ile bitstream uretin."
        exit 1
    }
}
puts "Bitstream dosyasi: $bitstream_file"

# =============================================================================
# FPGA PROGRAMLAMA
# =============================================================================
puts "============================================"
puts " FPGA programlaniyor..."
puts "============================================"

open_hw_manager
connect_hw_server

# Hedef cihazı bul
open_hw_target

set hw_device [lindex [get_hw_devices xc7a200t*] 0]
if {$hw_device eq ""} {
    puts "HATA: Nexys Video FPGA bulunamadi!"
    puts "Kart USB ile baglanmis ve gucu acik mi kontrol edin."
    close_hw_manager
    exit 1
}

current_hw_device $hw_device
set_property PROGRAM.FILE $bitstream_file [current_hw_device]

program_hw_devices [current_hw_device]

puts ""
puts "============================================"
puts " ✅ BASARILI!"
puts " FPGA basariyla programlandi!"
puts " HDMI kablosunu monitore baglayarak"
puts " Tetris oyununu gorebilirsiniz."
puts "============================================"
puts ""
puts " Buton Kontrolleri:"
puts "   Orta  (btnC) -> Bloku dondur"
puts "   Sol   (btnL) -> Sola kaydir"
puts "   Sag   (btnR) -> Saga kaydir"
puts "   Asagi (btnD) -> Hizli dusur"
puts "   Yukari(btnU) -> Oyunu sifirla"
puts ""

close_hw_manager
