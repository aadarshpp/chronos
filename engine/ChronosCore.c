#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include "ChronosClient.h"

#define BLOCK_SIZE 4
#define BUFFER_CAPACITY 4096
#define MAX_BLOCKS 50000

// --- Helpers ---
uint64_t zigzag_encode(int64_t n) {
    return (uint64_t)((n << 1) ^ (n >> 63));
}

int write_varint(uint64_t value, uint8_t *buffer) {
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

int64_t zigzag_decode(uint64_t n) {
    return (int64_t)((n >> 1) ^ -(n & 1));
}

int read_varint(FILE *file, uint64_t *value) {
    *value = 0;
    int shift = 0;
    uint8_t byte;
    do {
        if (fread(&byte, 1, 1, file) != 1) return -1;
        *value |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return shift / 7;
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
    uint64_t prev_ts;
    uint32_t prev_price;
    int64_t prev_ts_delta;
    int block_record_count;
    
    uint8_t buffer[BUFFER_CAPACITY];
    int buffer_len;
    
    long current_block_offset;      // ADDED: To track partial blocks
    long current_block_baseline_ts;
    int  current_block_baseline_price;
    
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
    enc->index_count = 0;
    enc->current_block_offset = 0;
    pthread_mutex_init(&enc->lock, NULL);
    return (jlong)enc;
}

JNIEXPORT void JNICALL Java_ChronosClient_insertCompressed(JNIEnv *env, jobject obj, jlong ptr, jlong timestamp, jint fixedPrice) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    
    pthread_mutex_lock(&enc->lock);

    uint64_t ts = (uint64_t)timestamp;
    uint64_t price = (uint64_t)fixedPrice;
    
    uint8_t temp_varint[10];
    int varint_len;

    if (enc->block_record_count == 0) {
        // CHANGED: Save to struct instead of local variable
        enc->current_block_offset = ftell(enc->file) + enc->buffer_len;
        
        varint_len = write_varint(ts, temp_varint);
        memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
        enc->buffer_len += varint_len;

        varint_len = write_varint(price, temp_varint);
        memcpy(enc->buffer + enc->buffer_len, temp_varint, varint_len);
        enc->buffer_len += varint_len;

        enc->prev_ts = ts;
        enc->prev_price = price;
        enc->prev_ts_delta = 0;
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
    if (enc->block_record_count >= BLOCK_SIZE) {
        enc->index[enc->index_count].file_offset = enc->current_block_offset;
        enc->index[enc->index_count].baseline_ts = enc->current_block_baseline_ts;
        enc->index[enc->index_count].baseline_price = enc->current_block_baseline_price;
        enc->index_count++;
        enc->block_record_count = 0;
    }

    if (enc->buffer_len > 4080) {
        fwrite(enc->buffer, 1, enc->buffer_len, enc->file);
        enc->buffer_len = 0;
    }

    pthread_mutex_unlock(&enc->lock);
}

JNIEXPORT void JNICALL Java_ChronosClient_closeEngine(JNIEnv *env, jobject obj, jlong ptr) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    
    pthread_mutex_lock(&enc->lock);
    
    if (enc->buffer_len > 0) {
        fwrite(enc->buffer, 1, enc->buffer_len, enc->file);
        enc->buffer_len = 0;
    }
    
    // ADDED: FLUSH PARTIAL BLOCK TO INDEX
    if (enc->block_record_count > 0) {
        enc->index[enc->index_count].file_offset = enc->current_block_offset;
        enc->index[enc->index_count].baseline_ts = enc->current_block_baseline_ts;
        enc->index[enc->index_count].baseline_price = enc->current_block_baseline_price;
        enc->index_count++;
    }

    fwrite(enc->index, sizeof(IndexEntry), enc->index_count, enc->file);
    
    int32_t num_blocks = enc->index_count;
    fwrite(&num_blocks, sizeof(int32_t), 1, enc->file);
    
    uint64_t magic = 0xDEADBEEFCAFEBABE;
    fwrite(&magic, sizeof(uint64_t), 1, enc->file);
    
    fclose(enc->file);
    
    pthread_mutex_unlock(&enc->lock);
}

JNIEXPORT jstring JNICALL Java_ChronosClient_getIndexStats(JNIEnv *env, jobject obj, jlong ptr) {
    ChronosEncoder *enc = (ChronosEncoder *)ptr;
    char buf[256];
    sprintf(buf, "Index Entries: %d | First Offset: %ld | First TS: %ld", 
            enc->index_count, 
            enc->index[0].file_offset, 
            enc->index[0].baseline_ts);
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jstring JNICALL Java_ChronosClient_queryData(JNIEnv *env, jobject obj, jlong startTs, jlong endTs) {
    FILE *file = fopen("data.chronos", "rb");
    if (!file) return (*env)->NewStringUTF(env, "[]");

    char *response = (char *)malloc(1048576); 
    if (!response) {
        fclose(file);
        return (*env)->NewStringUTF(env, "{\"error\":\"Out of memory\"}");
    }
    
    int resp_len = 0;
    int found_any = 0;
    resp_len += snprintf(response + resp_len, 1048576 - resp_len, "[");

    fseek(file, -8, SEEK_END);
    uint64_t magic;
    fread(&magic, sizeof(uint64_t), 1, file);
    if (magic != 0xDEADBEEFCAFEBABE) {
        free(response);
        fclose(file);
        return (*env)->NewStringUTF(env, "{\"error\":\"Invalid file footer\"}");
    }

    fseek(file, -12, SEEK_END);
    int32_t block_count;
    fread(&block_count, sizeof(int32_t), 1, file);

    if (block_count == 0) {
        free(response);
        fclose(file);
        return (*env)->NewStringUTF(env, "[]");
    }

    long index_size = block_count * sizeof(IndexEntry);
    fseek(file, -12 - index_size, SEEK_END);
    
    IndexEntry *index = (IndexEntry *)malloc(index_size);
    fread(index, sizeof(IndexEntry), block_count, file);

    int target_block = 0;
    for (int i = 0; i < block_count; i++) {
        if (index[i].baseline_ts <= startTs) {
            target_block = i;
        } else {
            break;
        }
    }

    long safe_end = -12 - index_size;
    fseek(file, safe_end, SEEK_END);
    long data_end_limit = ftell(file);
    
    fseek(file, index[target_block].file_offset, SEEK_SET);
    
    uint64_t prev_ts = index[target_block].baseline_ts;
    uint64_t prev_price = index[target_block].baseline_price;
    int64_t prev_ts_delta = 0;
    int block_count_local = 0;

    while (ftell(file) < data_end_limit) {
        uint64_t ts_encoded, price_encoded;
        uint64_t current_ts;
        uint64_t current_price;

        if (block_count_local == 0) {
            if (read_varint(file, &ts_encoded) < 0) break;
            if (read_varint(file, &price_encoded) < 0) break;
            
            current_ts = ts_encoded;
            current_price = price_encoded;
            
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

        block_count_local++;
        if (block_count_local >= BLOCK_SIZE) {
            block_count_local = 0; 
        }

        if (current_ts >= startTs && current_ts <= endTs) {
            double real_price = current_price / 100.0;
            resp_len += snprintf(response + resp_len, 1048576 - resp_len, 
                                  "%s{\"ts\":%" PRIu64 ",\"price\":%.2f}",
         found_any ? "," : "", current_ts, real_price);

            found_any = 1;
        }
        
        if (current_ts > endTs) break;
    }

    resp_len += snprintf(response + resp_len, 1048576 - resp_len, "]");
    free(index);
    fclose(file);

    jstring result = (*env)->NewStringUTF(env, response);
    free(response);
    return result;
}