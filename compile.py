import subprocess
import os
import sys
import shutil

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
    gcc_path = None
    objcopy_path = None

    # 1) Once PATH'te bir RISC-V gcc ara (WSL chipyard: riscv64-unknown-elf-gcc,
    #    Windows xpack: riscv-none-elf-gcc). Boylece sabit yola bagimli degiliz.
    for prefix in ("riscv64-unknown-elf-", "riscv-none-elf-", "riscv32-unknown-elf-"):
        found = shutil.which(prefix + "gcc")
        if found:
            gcc_path = found
            objcopy_path = shutil.which(prefix + "objcopy")
            break

    # 2) PATH'te yoksa, bilinen kurulum dizinlerini tara (son care)
    if not gcc_path:
        fallback_dirs = [
            r"C:\Users\deniz\OneDrive\Masaüstü\tübitak_uzay\riscv-toolchain",
        ]
        for toolchain_dir in fallback_dirs:
            if not os.path.isdir(toolchain_dir):
                continue
            for root, dirs, files in os.walk(toolchain_dir):
                for gcc_name in ("riscv-none-elf-gcc.exe", "riscv64-unknown-elf-gcc"):
                    if gcc_name in files:
                        gcc_path = os.path.join(root, gcc_name)
                        objcopy_path = gcc_path.replace("gcc", "objcopy")
                        break
                if gcc_path:
                    break
            if gcc_path:
                break

    if not gcc_path or not os.path.exists(gcc_path):
        print("Hata: RISC-V gcc bulunamadi!")
        print("  - WSL'de: 'source ~/chipyard/env.sh' ile riscv64-unknown-elf-gcc PATH'e gelir.")
        print("  - Windows'ta: riscv-none-elf-gcc.exe (xpack) PATH'te olmali veya kurulum dizinini ekleyin.")
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
        "sw/libgcc_min.c",   # RV32I icin __divsi3/__mulsi3 vb. (libgcc yerine)
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
