import subprocess
import os
import sys

def convert_bin_to_hex(bin_file, hex_file):
    if not os.path.exists(bin_file):
        print(f"Hata: {bin_file} bulunamadi!")
        sys.exit(1)
        
    try:
        with open(bin_file, "rb") as f:
            data = f.read()
            
        # 4-byte hizalama (Padding)
        while len(data) % 4 != 0:
            data += b"\x00"
            
        # RISC-V Little-Endian formatinda 32-bit kelimeleri hex olarak yaz (B3B2B1B0)
        with open(hex_file, "w") as f:
            for i in range(0, len(data), 4):
                word_bytes = data[i:i+4]
                word_val = (word_bytes[3] << 24) | (word_bytes[2] << 16) | (word_bytes[1] << 8) | word_bytes[0]
                f.write(f"{word_val:08x}\n")
                
        print(f"Verilog HEX formatina donusturuldu: {hex_file} ({len(data)//4} kelime)")
    except Exception as e:
        print(f"Donusturme hatasi: {e}")
        sys.exit(1)

def main():
    toolchain_dir = r"C:\Users\deniz\OneDrive\Masaüstü\tübitak_uzay\riscv-toolchain"
    gcc_path = None
    objcopy_path = None
    
    # riscv-none-elf-gcc.exe dosyasini tara
    for root, dirs, files in os.walk(toolchain_dir):
        if "riscv-none-elf-gcc.exe" in files:
            gcc_path = os.path.join(root, "riscv-none-elf-gcc.exe")
            objcopy_path = os.path.join(root, "riscv-none-elf-objcopy.exe")
            break
            
    if not gcc_path or not os.path.exists(gcc_path):
        print("Hata: riscv-none-elf-gcc.exe bulunamadi! Lutfen toolchain kurulumunu kontrol edin.")
        sys.exit(1)
        
    print(f"Derleyici bulundu: {gcc_path}")
    print(f"Objcopy bulundu: {objcopy_path}")
    
    # 1. Derleme
    print("\n[1/3] C ve Assembly kodlari RISC-V (RV32I) olarak derleniyor...")
    cmd_compile = [
        gcc_path,
        "-march=rv32i",
        "-mabi=ilp32",
        "-nostdlib",
        "-T", "sw/link.ld",
        "sw/boot.S",
        "sw/main.c",
        "-o", "sw/program.elf"
    ]
    res = subprocess.run(cmd_compile)
    if res.returncode != 0:
        print("HATA: Derleme basarisiz!")
        sys.exit(1)
        
    # 2. Binary donusumu
    print("[2/3] ELF dosyasi saf binary formatina donusturuluyor...")
    cmd_objcopy = [
        objcopy_path,
        "-O", "binary",
        "sw/program.elf",
        "sw/program.bin"
    ]
    res = subprocess.run(cmd_objcopy)
    if res.returncode != 0:
        print("HATA: Binary donusumu basarisiz!")
        sys.exit(1)
        
    # 3. Hex donusumu
    print("[3/3] Binary dosyasi Verilog HEX formatina donusturuluyor...")
    convert_bin_to_hex("sw/program.bin", "sw/program.hex")
    
    print("\nBasarili! program.hex dosyasi olusturuldu.")

if __name__ == "__main__":
    main()
