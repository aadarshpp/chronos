#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ChronosClient.h"

#define BLOCK_SIZE 4
#define BUFFER_CAPACITY 4096

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

// --- The State Struct ---
typedef struct {
    FILE *file;
    uint32_t prev_ts;
    uint32_t prev_price;
    int32_t prev_ts_delta;
    int block_record_count;
    
    // The RAM buffer
    uint8_t buffer[BUFFER_CAPACITY];
    int buffer_len;
} ChronosEncoder;

// --- JNI Functions ---

JNIEXPORT jlong JNICALL Java_ChronosClient_initEngine(JNIEnv *env, jobject obj) {
    ChronosEncoder *enc = (ChronosEncoder *)malloc(sizeof(ChronosEncoder));
    enc->file = fopen("data.chronos", "wb"); // "wb" overwrites/creates fresh
    enc->prev_ts = 0;
    enc->prev_price = 0;
    enc->prev_ts_delta = 0;
    enc->block_record_count = 0;
    enc->buffer_len = 0;
    return (jlong)enc; // Return memory address as a Java long
}

JNIEXPORT void JNICALL Java_ChronosClient_insertCompressed(JNIEnv *env, jobject obj, jlong ptr, jlong timestamp, jint fixedPrice) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    
    uint32_t ts = (uint32_t)timestamp;
    uint32_t price = (uint32_t)fixedPrice;
    
    uint8_t temp_varint[10]; // Max size for one varint
    int varint_len;

    if (enc->block_record_count == 0) {
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
    if (enc->block_record_count >= BLOCK_SIZE) {
        enc->block_record_count = 0;
    }

    // FLUSH LOGIC: If RAM buffer is getting full, write to disk
    if (enc->buffer_len > 4080) {
        fwrite(enc->buffer, 1, enc->buffer_len, enc->file);
        enc->buffer_len = 0;
    }
}

JNIEXPORT void JNICALL Java_ChronosClient_closeEngine(JNIEnv *env, jobject obj, jlong ptr) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    
    // Flush anything left in the RAM buffer
    if (enc->buffer_len > 0) {
        fwrite(enc->buffer, 1, enc->buffer_len, enc->file);
    }
    
    fclose(enc->file);
    free(enc); // Prevent memory leak
}