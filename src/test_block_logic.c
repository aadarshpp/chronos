#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BLOCK_SIZE 4

// --- Helpers from 3A and 3B ---
uint32_t zigzag_encode(int32_t n) {
    return (uint32_t)((n << 1) ^ (n >> 31));
}

int write_varint(uint32_t value, uint8_t *buffer) {
    int bytes_written = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value > 0) byte |= 0x80;
        buffer[bytes_written] = byte;
        bytes_written++;
    } while (value > 0);
    return bytes_written;
}

// --- The Encoder ---
void encode_record(uint32_t ts, uint32_t price, FILE *file) {
    static int block_record_count = 0;
    static uint32_t prev_ts = 0;
    static uint32_t prev_price = 0;
    static int32_t prev_ts_delta = 0;

    uint8_t buffer[10]; // Max varint size is 5 bytes, we have 2 values max per record
    int buf_len = 0;

    if (block_record_count == 0) {
        // RECORD 1: Absolute Baselines
        buf_len += write_varint(ts, buffer + buf_len);
        buf_len += write_varint(price, buffer + buf_len);
        prev_ts = ts;
        prev_price = price;
        prev_ts_delta = 0; // Reset delta state
        
    } else {
        // Calculate Deltas
        int32_t ts_delta = (int32_t)(ts - prev_ts);
        int32_t price_delta = (int32_t)(price - prev_price);

        if (block_record_count == 1) {
            // RECORD 2: First-Order Deltas
            buf_len += write_varint(zigzag_encode(ts_delta), buffer + buf_len);
            buf_len += write_varint(zigzag_encode(price_delta), buffer + buf_len);
            prev_ts_delta = ts_delta;
            
        } else {
            // RECORD 3+: Delta-of-Delta for TS, Delta for Price
            int32_t ts_dod = ts_delta - prev_ts_delta;
            buf_len += write_varint(zigzag_encode(ts_dod), buffer + buf_len);
            buf_len += write_varint(zigzag_encode(price_delta), buffer + buf_len);
            prev_ts_delta = ts_delta;
        }

        prev_ts = ts;
        prev_price = price;
    }

    // Write the compressed bytes to disk
    fwrite(buffer, 1, buf_len, file);

    // Increment and check for block reset
    block_record_count++;
    if (block_record_count >= BLOCK_SIZE) {
        block_record_count = 0; // Reset for the next block
    }
}

int main() {
    // Prices are already multiplied by 100 (Fixed Point) by Java in the real app
    // Timestamps: 1000, 1001, 1002, 1004 (Gap goes +1, +1, +2 -> DoD: +1, 0, +1)
    // Prices:     15025, 15030, 15028, 15035 (Deltas: +5, -2, +7)
    
    uint32_t test_ts[] = {1000, 1001, 1002, 1004, 2000}; // Note: 2000 triggers a new block
    uint32_t test_pr[] = {15025, 15030, 15028, 15035, 15500};
    int num_records = 5;

    FILE *file = fopen("test_block.bin", "wb");
    if (!file) return 1;

    for (int i = 0; i < num_records; i++) {
        encode_record(test_ts[i], test_pr[i], file);
    }

    fclose(file);
    printf("Encoded %d records into test_block.bin\n", num_records);
    return 0;
}