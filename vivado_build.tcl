# =============================================================================
# Vivado TCL Script - RISC-V SoC Tetris Projesi
# Otomatik: Proje oluştur → Sentez → Implementation → Bitstream → Program FPGA
# =============================================================================

# Proje dizinleri
set project_name    "riscv_soc_tetris"
set project_dir     "C:/vivado_runs/riscv_soc_tetris"
set src_dir         [pwd]
set generated_dir   "$src_dir/generated"
set sw_dir          "$src_dir/sw"

# FPGA part numarası (Nexys Video)
set part            "xc7a200tsbg484-1"

# =============================================================================
# 1. PROJE OLUŞTURMA
# =============================================================================
puts "============================================"
puts " ADIM 1: Vivado Projesi Olusturuluyor..."
puts "============================================"

# Eğer eski proje varsa sil
if {[file exists $project_dir]} {
    file delete -force $project_dir
}
file mkdir $project_dir

# Yeni proje oluştur
create_project $project_name $project_dir -part $part -force

# =============================================================================
# 2. KAYNAK DOSYALARINI EKLEME
# =============================================================================
puts "============================================"
puts " ADIM 2: Kaynak dosyalari ekleniyor..."
puts "============================================"

# ⭐ ROM init yolunu MUTLAKLAŞTIR (portatif): bağıl "program.hex" Vivado sentez
# çalışma dizininde bulunamıyor (CRITICAL WARNING -> ROM boş). Build anında [pwd]'den
# mutlak yola çeviriyoruz; böylece kaynak bağıl kalır, her makinede yerel yol kullanılır.
set svfile "$generated_dir/SoCTop.sv"
if {[file exists $svfile]} {
    set fp [open $svfile r]; set c [read $fp]; close $fp
    set c [string map [list "\$readmemh(\"program.hex\"" "\$readmemh(\"$sw_dir/program.hex\""] $c]
    set fp [open $svfile w]; puts -nonewline $fp $c; close $fp
    puts "ROM init yolu mutlaklastirildi: $sw_dir/program.hex"
}

# SystemVerilog / Verilog kaynak dosyaları
# NOT: SoCTop.sv kendi kendine yeterlidir (Core + tüm alt modülleri içerir).
# Core.sv fazlalıktır; eklenirse "module Core" çift tanımlanır -> bu yüzden eklenmiyor.
add_files -norecurse [list \
    "$generated_dir/SoCTop.sv" \
    "$generated_dir/TopWrapper.v" \
    "$generated_dir/tmds_channel_encoder.v" \
]

# Program HEX dosyası (ROM initialization)
add_files -norecurse "$sw_dir/program.hex"

# Constraints dosyası
add_files -fileset constrs_1 -norecurse "$generated_dir/pins.xdc"

# Top modülü ayarla
set_property top TopWrapper [current_fileset]

# ⭐ KRİTİK: ROM/BRAM init ($readmemh) firtool tarafından `ifndef SYNTHESIS arkasına
# konur (sadece-simülasyon). Sentezde de çalışıp ROM'u gerçekten initialize etmesi için
# ENABLE_INITIAL_MEM_ makrosunu tanımlıyoruz. Bu olmazsa kartta ROM BOŞ kalır (siyah ekran).
set_property verilog_define {ENABLE_INITIAL_MEM_} [current_fileset]

# Dosyaları güncelle
update_compile_order -fileset sources_1

puts "Kaynak dosyalari basariyla eklendi."

# =============================================================================
# 3. SENTEZ (SYNTHESIS)
# =============================================================================
puts "============================================"
puts " ADIM 3: Sentez basliyor..."
puts " (Bu islem 5-15 dakika surebilir)"
puts "============================================"

launch_runs synth_1 -jobs 4
wait_on_run synth_1

# Sentez sonucunu kontrol et
if {[get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {
    puts "HATA: Sentez basarisiz oldu!"
    puts "Durum: [get_property STATUS [get_runs synth_1]]"
    exit 1
}
puts "Sentez basariyla tamamlandi!"

# =============================================================================
# 4. IMPLEMENTATION (YERLEŞTIRME VE YÖNLENDİRME)
# =============================================================================
puts "============================================"
puts " ADIM 4: Implementation basliyor..."
puts " (Bu islem 10-20 dakika surebilir)"
puts "============================================"

launch_runs impl_1 -jobs 4
wait_on_run impl_1

if {[get_property STATUS [get_runs impl_1]] ne "route_design Complete!"} {
    puts "HATA: Implementation basarisiz oldu!"
    puts "Durum: [get_property STATUS [get_runs impl_1]]"
    exit 1
}
puts "Implementation basariyla tamamlandi!"

# =============================================================================
# 5. BITSTREAM ÜRETİMİ
# =============================================================================
puts "============================================"
puts " ADIM 5: Bitstream uretiliyor..."
puts " (Bu islem 3-5 dakika surebilir)"
puts "============================================"

launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

puts "Bitstream basariyla uretildi!"

# Bitstream dosyasının yolunu bul
set bitstream_file "$project_dir/$project_name.runs/impl_1/TopWrapper.bit"
if {![file exists $bitstream_file]} {
    puts "UYARI: Bitstream dosyasi beklenen yolda bulunamadi, aranıyor..."
    set bitstream_file [glob -nocomplain "$project_dir/$project_name.runs/impl_1/*.bit"]
    if {$bitstream_file eq ""} {
        puts "HATA: Bitstream dosyasi bulunamadi!"
        exit 1
    }
}
puts "Bitstream dosyasi: $bitstream_file"

puts ""
puts "============================================"
puts " ✅ SENTEZ VE BITSTREAM TAMAMLANDI!"
puts " Lutfen karti baglayin ve vivado_program.tcl"
puts " dosyasini calistirin."
puts "============================================"
puts ""

