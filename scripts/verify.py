import struct
import os
import json

# FILE_PATH = "data/market_data.bin"
CATALOG_FILE = "data/catalog.json"
RECORD_SIZE = 32  # Updated to 32 bytes

# Format: Q(8) i(4) i(4) i(4) i(4) I(4) I(4)
FORMAT = '<QiiiiII'

def load_catalog():
    with open(CATALOG_FILE, "r") as f:
        return json.load(f)

def main():
    catalog = load_catalog()

    for partition in catalog:
        print(f"\nChecking partition: {partition['file']}")

        file_path = partition["file"]

        file_size = os.path.getsize(file_path)
        num_records = file_size // RECORD_SIZE

        print(f"File Size: {file_size} bytes")
        print(f"Total Candles: {num_records}")

        with open(file_path, "rb") as f:
            print("\n--- First 5 Candles ---")

            for i in range(5):
                data = f.read(RECORD_SIZE)
                if not data:
                    break

                ts, open_p, high_p, low_p, close_p, vol, crc = struct.unpack(FORMAT, data)

                print(
                    f"[{i}] Time: {ts} | "
                    f"O:{open_p/10000:.2f} "
                    f"H:{high_p/10000:.2f} "
                    f"L:{low_p/10000:.2f} "
                    f"C:{close_p/10000:.2f} | "
                    f"Vol:{vol}"
                )

if __name__ == "__main__":
    main()