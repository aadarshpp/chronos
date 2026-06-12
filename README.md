# Metric-T

A high-performance financial time-series database built with C for storage efficiency and Python for visualization. Metric-T fetches market data, stores it in a compact binary format, and serves it through a modern web dashboard.

## Overview

Metric-T is designed around a hybrid architecture:

* **Core Engine (C)** - Fetches market data, parses JSON responses, and serializes records into an optimized binary format.
* **API Layer (FastAPI)** - Exposes binary data through HTTP endpoints.
* **Frontend Dashboard** - Interactive TradingView-style charts rendered directly in the browser.

The project demonstrates how low-level systems programming can be combined with modern web technologies to build efficient analytics tools.

---

## Features

### Efficient Binary Storage

* Fixed-width 32-byte records
* Predictable memory layout
* Fast sequential reads and writes
* Reduced storage overhead

### Financial Precision

* Prices stored as fixed-point integers (`value × 10000`)
* Avoids floating-point rounding errors
* Consistent calculations across languages

### Zero-Copy Interoperability

* Python reads the exact binary layout produced by C
* No serialization/deserialization overhead
* Shared schema between components

### Interactive Dashboard

* FastAPI-powered backend
* Dynamic chart rendering
* Browser-based visualization
* No page reloads required

---

## Binary Schema

Each record is exactly 32 bytes.

```c
#pragma pack(push, 1)
typedef struct {
    uint64_t timestamp;
    int32_t open;
    int32_t high;
    int32_t low;
    int32_t close;
    uint32_t volume;
    uint32_t crc;
} CandleStick;
#pragma pack(pop)
```

| Field     | Type     | Size         |
| --------- | -------- | ------------ |
| timestamp | uint64_t | 8 bytes      |
| open      | int32_t  | 4 bytes      |
| high      | int32_t  | 4 bytes      |
| low       | int32_t  | 4 bytes      |
| close     | int32_t  | 4 bytes      |
| volume    | uint32_t | 4 bytes      |
| crc       | uint32_t | 4 bytes      |
| **Total** |          | **32 bytes** |

---

## Project Structure

```text
Metric-T/
├── data/
│   └── .gitkeep
├── src/
│   ├── fetcher.c
│   └── engine.h
├── lib/
│   └── cJSON/
├── scripts/
├── template/
├── main.py
├── requirements.txt
├── README.md
└── LICENSE
```

---

## Running the Project

### Windows (PowerShell)

```powershell
gcc src/fetcher.c lib/cJSON-1.7.19/cJSON.c -Ilib/cJSON-1.7.19 -o fetcher -lcurl

.\fetcher

python main.py
```

Open:

```text
http://localhost:8000
```

---

### Linux

```bash
gcc src/fetcher.c lib/cJSON-1.7.19/cJSON.c -Ilib/cJSON-1.7.19 -o fetcher -lcurl

./fetcher

python main.py
```

Open:

```text
http://localhost:8000
```

---

## Data Flow

```text
Yahoo Finance
      │
      ▼
Fetcher (C)
      │
      ▼
market_data.bin
      │
      ▼
FastAPI
      │
      ▼
Dashboard
```

---

## Future Roadmap

### Scalability

* Monthly/yearly data partitioning
* Multi-symbol storage support

### Performance

* Memory-mapped file access (`mmap`)
* Faster range queries
* Indexed lookups

### Analytics

* Simple Moving Average (SMA)
* Exponential Moving Average (EMA)
* RSI and MACD indicators

### Product Features

* Dynamic ticker search
* Multi-asset support
* Live refresh mode
* Historical backtesting

---

## Technology Stack

### Core Engine

* C
* libcurl
* cJSON

### Backend

* Python
* FastAPI

### Frontend

* HTML
* JavaScript
* TradingView Lightweight Charts

---

## License

MIT License
