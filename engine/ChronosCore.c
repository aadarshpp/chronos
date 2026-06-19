#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "ChronosClient.h"

#define BLOCK_SIZE 4
#define BUFFER_CAPACITY 4096
#define MAX_BLOCKS 10000

// --- Helpers ---
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

// --- The Index Structure ---
typedef struct __attribute__((packed)) {
    long file_offset;
    long baseline_ts;
    int  baseline_price;
} IndexEntry;

// --- The State Struct ---
typedef struct {
    FILE *file;
    uint32_t prev_ts;
    uint32_t prev_price;
    int32_t prev_ts_delta;
    int block_record_count;

    long current_block_baseline_ts;
    int  current_block_baseline_price;
    
    uint8_t buffer[BUFFER_CAPACITY];
    int buffer_len;
    
    IndexEntry index[MAX_BLOCKS];
    int index_count;

    pthread_mutex_t lock;
} ChronosEncoder;

// --- JNI Functions ---

JNIEXPORT jlong JNICALL Java_ChronosClient_initEngine(JNIEnv *env, jobject obj) {
    ChronosEncoder *enc = (ChronosEncoder *)malloc(sizeof(ChronosEncoder));
    enc->file = fopen("data.chronos", "wb");
    enc->prev_ts = 0;
    enc->prev_price = 0;
    enc->prev_ts_delta = 0;
    enc->block_record_count = 0;
    enc->buffer_len = 0;
    enc->index_count = 0; // Init index
    return (jlong)enc;
}

JNIEXPORT void JNICALL Java_ChronosClient_insertCompressed(JNIEnv *env, jobject obj, jlong ptr, jlong timestamp, jint fixedPrice) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    uint32_t ts = (uint32_t)timestamp;
    uint32_t price = (uint32_t)fixedPrice;
    
    uint8_t temp_varint[10];
    int varint_len;
    long current_block_offset = 0;

    if (enc->block_record_count == 0) {
        // Calculate where this block is starting in the final file
        current_block_offset = ftell(enc->file) + enc->buffer_len;
        
        varint_len = write_varint(ts, temp_varint);
        memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
        enc->buffer_len += varint_len;

        varint_len = write_varint(price, temp_varint);
        memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
        enc->buffer_len += varint_len;

        enc->prev_ts = ts;
        enc->prev_price = price;
        enc->prev_ts_delta = 0;

        // Capture the baseline for the index when the block starts
        enc->current_block_baseline_ts = ts;
        enc->current_block_baseline_price = price;
        
    } else {
        int32_t ts_delta = (int32_t)(ts - enc->prev_ts);
        int32_t price_delta = (int32_t)(price - enc->prev_price);

        if (enc->block_record_count == 1) {
            varint_len = write_varint(zigzag_encode(ts_delta), temp_varint);
            memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
            enc->buffer_len += varint_len;
            enc->prev_ts_delta = ts_delta;
        } else {
            int32_t ts_dod = ts_delta - enc->prev_ts_delta;
            varint_len = write_varint(zigzag_encode(ts_dod), temp_varint);
            memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
            enc->buffer_len += varint_len;
            enc->prev_ts_delta = ts_delta;
        }

        varint_len = write_varint(zigzag_encode(price_delta), temp_varint);
        memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
        enc->buffer_len += varint_len;

        enc->prev_ts = ts;
        enc->prev_price = price;
    }

    enc->block_record_count++;
    
    // BLOCK RESET & INDEX SAVE
    if (enc->block_record_count >= BLOCK_SIZE) {
        // Save the current block's offset and baseline to the index
        enc->index[enc->index_count].file_offset = current_block_offset;
        enc->index[enc->index_count].baseline_ts = enc->current_block_baseline_ts; // Use saved baseline
        enc->index[enc->index_count].baseline_price = enc->current_block_baseline_price; // Use saved baseline
        enc->index_count++;
        enc->block_record_count = 0;
    }

    if (enc->buffer_len > 4080) {
        fwrite(enc->buffer, 1, enc->buffer_len, enc->file);
        enc->buffer_len = 0;
    }
}

JNIEXPORT void JNICALL Java_ChronosClient_closeEngine(JNIEnv *env, jobject obj, jlong ptr) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    
    if (enc->buffer_len > 0) {
        fwrite(enc->buffer, 1, enc->buffer_len, enc->file);
        enc->buffer_len = 0;
    }
    
    // 1. Write the Index Array to disk
    fwrite(enc->index, sizeof(IndexEntry), enc->index_count, enc->file);
    
    // 2. Write the number of blocks (4 bytes)
    int32_t num_blocks = enc->index_count;
    fwrite(&num_blocks, sizeof(int32_t), 1, enc->file);
    
    // 3. Write the 8-byte Magic Number
    uint64_t magic = 0xDEADBEEFCAFEBABE;
    fwrite(&magic, sizeof(uint64_t), 1, enc->file);
    
    fclose(enc->file);
    free(enc);
}

// Temporary test helper
JNIEXPORT jstring JNICALL Java_ChronosClient_getIndexStats(JNIEnv *env, jobject obj, jlong ptr) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    char buf[256];
    sprintf(buf, "Index Entries: %d | First Offset: %ld | First TS: %ld", 
            enc->index_count, 
            enc->index[0].file_offset, 
            enc->index[0].baseline_ts);
    return (*env)->NewStringUTF(env, buf);
}

// --- INVERSE MATH HELPERS ---

int32_t zigzag_decode(uint32_t n) {
    return (int32_t)((n >> 1) ^ -(n & 1));
}

// Reads a varint from a file and advances the file pointer
int read_varint(FILE *file, uint32_t *value) {
    *value = 0;
    int shift = 0;
    uint8_t byte;
    
    do {
        if (fread(&byte, 1, 1, file) != 1) return -1; // EOF or error
        *value |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    return shift / 7; // return number of bytes read
}

// --- THE QUERY FUNCTION ---

JNIEXPORT jstring JNICALL Java_ChronosClient_queryData(JNIEnv *env, jobject obj, jlong startTs, jlong endTs) {
    FILE *file = fopen("data.chronos", "rb");
    if (!file) return (*env)->NewStringUTF(env, "[]");

    // 1. Seek to Magic Number (last 8 bytes)
    fseek(file, -8, SEEK_END);
    uint64_t magic;
    fread(&magic, sizeof(uint64_t), 1, file);
    if (magic != 0xDEADBEEFCAFEBABE) {
        fclose(file);
        return (*env)->NewStringUTF(env, "{\"error\":\"Invalid file footer\"}");
    }

    // 2. Read Block Count (previous 4 bytes)
    fseek(file, -12, SEEK_END);
    int32_t block_count;
    fread(&block_count, sizeof(int32_t), 1, file);

    if (block_count == 0) {
        fclose(file);
        return (*env)->NewStringUTF(env, "[]");
    }

    // 3. Read Index Array
    long index_size = block_count * sizeof(IndexEntry);
    fseek(file, -12 - index_size, SEEK_END);
    
    IndexEntry *index = (IndexEntry *)malloc(index_size);
    fread(index, sizeof(IndexEntry), block_count, file);

    // 4. Binary Search to find the starting block
    int target_block = 0;
    for (int i = 0; i < block_count; i++) {
        if (index[i].baseline_ts <= startTs) {
            target_block = i;
        } else {
            break;
        }
    }

    // 5. Jump to Data and Initialize Decoder State
    fseek(file, index[target_block].file_offset, SEEK_SET);
    
    uint32_t prev_ts = index[target_block].baseline_ts;
    uint32_t prev_price = index[target_block].baseline_price;
    int32_t prev_ts_delta = 0;
    
    // We must decode from the start of the block to rebuild the state, 
    // even if the first few records are before startTs.
    int block_count_local = 0;

    char response[8192] = "["; // Start JSON array
    int resp_len = 1;
    int found_any = 0;  

    // Calculate the safe endpoint (where the index footer begins)
    long safe_end = -12 - index_size;
    fseek(file, safe_end, SEEK_END);
    long data_end_limit = ftell(file);
    
    // Jump back to data start
    fseek(file, index[target_block].file_offset, SEEK_SET);

    // 6. The Decode Loop
    while (ftell(file) < data_end_limit) {
        uint32_t ts_encoded, price_encoded;
        uint32_t current_ts;
        uint32_t current_price;

        if (block_count_local == 0) {
            // ABSOLUTE BASELINE: Read the raw values from the file
            if (read_varint(file, &ts_encoded) < 0) break;
            if (read_varint(file, &price_encoded) < 0) break;
            
            current_ts = ts_encoded;
            current_price = price_encoded;
            
            // Crucial: Reset all state for this new block
            prev_ts = current_ts;
            prev_price = current_price;
            prev_ts_delta = 0;
            
        } else {
            if (read_varint(file, &ts_encoded) < 0) break;
            if (read_varint(file, &price_encoded) < 0) break;

            int32_t ts_delta;
            if (block_count_local == 1) {
                ts_delta = zigzag_decode(ts_encoded);
                prev_ts_delta = ts_delta;
            } else {
                int32_t ts_dod = zigzag_decode(ts_encoded);
                ts_delta = prev_ts_delta + ts_dod;
                prev_ts_delta = ts_delta;
            }
            
            int32_t price_delta = zigzag_decode(price_encoded);
            
            current_ts = prev_ts + ts_delta;
            current_price = prev_price + price_delta;
            
            prev_ts = current_ts;
            prev_price = current_price;
        }

        // Advance block counter and RESET if we hit the block size limit
        block_count_local++;
        if (block_count_local >= BLOCK_SIZE) {
            block_count_local = 0; 
        }

        // 7. Filter and Format JSON
        if (current_ts >= startTs && current_ts <= endTs) {
            double real_price = current_price / 100.0;
            resp_len += snprintf(response + resp_len, sizeof(response) - resp_len, 
                                 "%s{\"ts\":%u,\"price\":%.2f}", 
                                 found_any ? "," : "", current_ts, real_price);
            found_any = 1;
        }
        
        // Stop if we've passed the requested end time (optimization)
        if (current_ts > endTs) break;
    }

    strcat(response, "]");
    free(index);
    fclose(file);

    return (*env)->NewStringUTF(env, response);
}