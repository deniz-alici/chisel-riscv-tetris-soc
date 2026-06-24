import sys
import os

def main():
    bin_file = "sw/program.bin"
    hex_file = "sw/program.hex"
    
    if not os.path.exists(bin_file):
        print(f"Hata: {bin_file} bulunamadı!")
        sys.exit(1)
        
    try:
        with open(bin_file, "rb") as f:
            data = f.read()
            
        # 4-byte hizalama için gerekirse sıfır ekle (Padding)
        while len(data) % 4 != 0:
            data += b"\x00"
            
        # RISC-V Little-Endian formatında 32-bit kelimeleri hex olarak yaz (B3B2B1B0)
        with open(hex_file, "w") as f:
            for i in range(0, len(data), 4):
                word_bytes = data[i:i+4]
                # Little-endian birleştirme
                word_val = (word_bytes[3] << 24) | (word_bytes[2] << 16) | (word_bytes[1] << 8) | word_bytes[0]
                f.write(f"{word_val:08x}\n")
                
        print(f"Başarıyla Verilog HEX formatına dönüştürüldü: {hex_file} ({len(data)//4} kelime)")
    except Exception as e:
        print(f"Dönüştürme hatası: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
