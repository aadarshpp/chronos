import struct
import os

FILE_PATH = "data/market_data.bin"
RECORD_SIZE = 32  # Updated to 32 bytes

# Format: Q(8) i(4) i(4) i(4) i(4) I(4) I(4)
FORMAT = '<QiiiiII'

def main():
    file_size = os.path.getsize(FILE_PATH)
    num_records = file_size // RECORD_SIZE
    
    print(f"File Size: {file_size} bytes")
    print(f"Total Candles: {num_records}")
    
    with open(FILE_PATH, "rb") as f:
        print("\n--- First 5 Candles ---")
        for i in range(5):
            data = f.read(RECORD_SIZE)
            if not data: break
            
            ts, open_p, high_p, low_p, close_p, vol, crc = struct.unpack(FORMAT, data)
            
            # Descale prices
            print(f"[{i}] Time: {ts} | O:{open_p/10000:.2f} H:{high_p/10000:.2f} L:{low_p/10000:.2f} C:{close_p/10000:.2f} | Vol:{vol}")

if __name__ == "__main__":
    main()