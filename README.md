# Metric-T
[![FastAPI](https://img.shields.io/badge/FastAPI-0.100+-green?logo=fastapi)](https://fastapi.tiangolo.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Overview

Metric-T is designed around a hybrid architecture to maximize throughput, minimize storage overhead, and ensure scalability:

*   **Core Engine (C)** - Handles low-level networking (libcurl), JSON parsing (cJSON), and binary serialization into monthly partitions.
*   **API Layer (Python)** - Serves the binary data via a high-performance REST API (FastAPI) using **Memory Mapping (`mmap`)** to avoid loading entire files into RAM.
*   **Frontend Dashboard** - Renders interactive financial charts using TradingView Lightweight Charts.

This project demonstrates how low-level systems programming (C) can be seamlessly integrated with modern web technologies (Python/JS) to build efficient, scalable analytics tools.

---

## Features

### Efficient Binary Storage
*   **Fixed-Width Schema:** Enforced 32-byte records using `#pragma pack`.
*   **Performance:** Eliminates parsing overhead found in text-based formats (CSV/JSON).
*   **Density:** 100% storage efficiency with no padding.

### Time-Series Partitioning (Scalability)
*   **Dynamic Chunking:** Data is automatically split into monthly partitions (e.g., `data/TSLA_2023_10.bin`) based on timestamp.
*   **Catalog Index:** A JSON catalog (`catalog.json`) tracks partition metadata (start/end times), enabling rapid discovery.
*   **Scalable Design:** Querying 5 years of data only scans the relevant monthly files, not the entire dataset.

### High-Performance Querying
*   **Memory-Mapped I/O (`mmap`):** The Python API uses OS-level virtual memory to read binary files.
*   **Zero-Copy Access:** Data is read directly from disk pages to CPU registers without intermediate user-space buffering, allowing large datasets to be queried on hardware with limited RAM.

### Financial Precision
*   **Fixed-Point Arithmetic:** Prices stored as integers (scaled by 10,000) to avoid IEEE 754 floating-point drift.
*   **Auditability:** CRC32 checksums embedded in every record for data integrity verification.

### Direct Binary Compatibility
*   **Universal Protocol:** Python reads the exact memory layout produced by C.
*   **Interoperability:** Shared schema definition (`engine.h` / Python `struct`) ensures zero ambiguity between layers.

### Interactive Dashboard
*   **High-Fidelity Viz:** TradingView Lightweight Charts for professional rendering.
*   **Async API:** FastAPI backend prevents UI blocking during data retrieval.
*   **Dynamic Fetching:** Users can input any ticker symbol (e.g., AAPL, TSLA) to fetch and visualize live data.

---

## Binary Schema

Each record is exactly 32 bytes, ensuring O(1) mathematical access calculation.

```c
#pragma pack(push, 1)
typedef struct {
    uint64_t timestamp; // 8 bytes: Unix Epoch
    int32_t open;       // 4 bytes: Scaled Price
    int32_t high;       // 4 bytes: Scaled Price
    int32_t low;        // 4 bytes: Scaled Price
    int32_t close;      // 4 bytes: Scaled Price
    uint32_t volume;    // 4 bytes: Trade Volume
    uint32_t crc;       // 4 bytes: Checksum
} CandleStick;
#pragma pack(pop)
```

| Field     | Type      | Size         | Description                   |
| --------- | --------- | ------------ | ----------------------------- |
| timestamp | `uint64_t` | 8 bytes      | Unix Epoch (Seconds)          |
| open      | `int32_t`  | 4 bytes      | Open Price (x10000)           |
| high      | `int32_t`  | 4 bytes      | High Price (x10000)           |
| low       | `int32_t`  | 4 bytes      | Low Price (x10000)            |
| close     | `int32_t`  | 4 bytes      | Close Price (x10000)          |
| volume    | `uint32_t` | 4 bytes      | Total Volume                  |
| crc       | `uint32_t` | 4 bytes      | Data Integrity Checksum       |
| **Total** |           | **32 bytes** |                               |

---

## Project Structure

```text
Metric-T/
├── data/
│   ├── catalog.json          # Partition metadata index
│   ├── AAPL_2023_10.bin      # Partitioned binary files
│   └── TSLA_2023_10.bin
├── src/
│   ├── fetcher.c             # C Network Engine (Partition-aware)
│   └── engine.h              # Binary Schema Definition
├── lib/
│   └── cJSON-1.7.19/          # JSON Parser Dependency
├── scripts/
│   └── verify.py              # Python Interop Verification
├── index.html                 # Frontend Dashboard
├── main.py                    # FastAPI Server (mmap enabled)
├── requirements.txt           # Python Dependencies
└── README.md
```

---

## Getting Started

### Prerequisites
*   **C Compiler:** GCC (Linux/MinGW) or Clang.
*   **Libraries:** `libcurl` (Network), `cJSON` (Parsing).
*   **Python:** 3.8+

### Installation

1.  **Install Python Dependencies:**
    ```bash
    pip install -r requirements.txt
    ```

2.  **Compile the C Engine:**
    *   **Windows (PowerShell):**
        ```powershell
        gcc src/fetcher.c lib/cJSON-1.7.19/cJSON.c -Ilib/cJSON-1.7.19 -o fetcher -lcurl
        ```
    *   **Linux:**
        ```bash
        gcc src/fetcher.c lib/cJSON-1.7.19/cJSON.c -Ilib/cJSON-1.7.19 -o fetcher -lcurl
        ```

3.  **Generate Data (Partitioned):**
    Fetches real-time data and splits it into monthly partitions.
    ```bash
    # Fetch AAPL
    ./fetcher AAPL
    
    # Fetch TSLA
    ./fetcher TSLA
    ```

4.  **Launch Dashboard:**
    ```bash
    python main.py
    ```
    Open [http://localhost:8000](http://localhost:8000) in your browser.

---

## System Architecture

```text
[Yahoo Finance API]
          │
          ▼ (JSON)
    ┌─────────────┐
    │ Fetcher (C) │
    └─────────────┘
          │
          ├────────────────────────────┐
          │                            │
          ▼ (Binary Write)             ▼ (JSON Update)
    [data/TSLA_2023_10.bin]    [catalog.json]
    [data/TSLA_2023_11.bin]
          │
          ▼ (mmap / Read)
    ┌─────────────┐
    │  FastAPI    │ --(JSON API)--> [index.html]
    └─────────────┘
```

---

## Technology Stack

*   **Core:** C, libcurl, cJSON
*   **Backend:** Python, FastAPI, Uvicorn, mmap
*   **Frontend:** HTML5, JavaScript, TradingView Lightweight Charts

## License

MIT License