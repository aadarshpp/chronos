# Metric-T

An efficient C-based binary storage engine designed for financial time-series (OHLCV) data, delivering compact storage, low-latency access, and Python visualization integration. 

## Architecture
Metric-T uses a hybrid architecture to maximize efficiency:
*   **Core Engine (C):** Handles raw network requests (libcurl), JSON parsing, binary serialization, and disk I/O.
*   **Visualization Layer (Python):** Reads the binary output to render interactive analytics.

## Key Features
*   **Fixed-Width Binary Storage:** 32-byte packed structs for 100% storage density.
*   **Zero-Copy Interoperability:** Python reads the exact memory layout written by C.
*   **Financial Precision:** Fixed-point arithmetic eliminates IEEE 754 floating-point drift.

## The Schema
Each `CandleStick` record is exactly 32 bytes:
```c
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
```
