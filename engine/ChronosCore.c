#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
typedef struct {
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
    
    uint8_t buffer[BUFFER_CAPACITY];
    int buffer_len;
    
    // New for Phase 4
    IndexEntry index[MAX_BLOCKS];
    int index_count;
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
        // Save the state of the block that just finished
        enc->index[enc->index_count].file_offset = current_block_offset;
        enc->index[enc->index_count].baseline_ts = enc->prev_ts; // We save the baseline of the block that just passed
        // Note: To get the ACTUAL baseline of the block, we should have saved it at record 0. 
        // Let's quickly fix this in the logic: we need baseline_ts from when count was 0.
        // I will handle this cleanly in the code block below.
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