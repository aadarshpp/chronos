#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>

#pragma pack(push, 1)

typedef struct {
    uint64_t timestamp; // 8 bytes
    int32_t open;       // 4 bytes (Scaled x10000)
    int32_t high;       // 4 bytes
    int32_t low;        // 4 bytes
    int32_t close;      // 4 bytes
    uint32_t volume;    // 4 bytes
    uint32_t crc;       // 4 bytes
} CandleStick;

#pragma pack(pop)

#define PRICE_SCALE 10000
#define DATA_FILE "data/market_data.bin"

#endif