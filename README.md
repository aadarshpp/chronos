<p align="center">
  <img src="./assests/Chronos_banner.png" width="300">
</p>

<p align="center">
  <a href="https://en.wikipedia.org/wiki/C_(programming_language)">
    <img src="https://img.shields.io/badge/C-17-blue?logo=c">
  </a>
  <a href="https://openjdk.org/">
    <img src="https://img.shields.io/badge/Java-11-orange?logo=openjdk">
  </a>
  <a href="https://en.wikipedia.org/wiki/Java_Native_Interface">
    <img src="https://img.shields.io/badge/JNI-Native-green">
  </a>
  <a href="https://opensource.org/licenses/MIT">
    <img src="https://img.shields.io/badge/License-MIT-yellow.svg">
  </a>
</p>

---

**Chronos** is a specialized, high-performance time-series storage engine built for financial tick data. It bridges a C native core (for extreme I/O and bit-level compression) to a Java HTTP front-end via JNI.

## Key Features
*   **Block-Based Delta-of-Delta Compression:** Achieves >80% file size reduction by exploiting the continuous nature of market timestamps and price ticks.
*   **Bit-Level Optimization:** Uses ZigZag encoding and Variable-Length Integers (Varint) to dynamically shrink integer footprints down to 1 byte.
*   **O(log N) Sparse Indexing:** Appends a binary-searchable index footer to the file, allowing instant range queries without full-file scans.
*   **Thread-Safe Concurrency:** Uses `pthread_mutex_t` to safely handle concurrent HTTP inserts via a fixed Java thread pool.
*   **Zero-Dependency Java Server:** Uses built-in `com.sun.net.httpserver` to keep the focus purely on the C engine mechanics.

## How The Compression Works
1.  **Fixed-Point:** Prices (e.g., `150.25`) are converted to integers (`15025`) in Java to eliminate floating-point inaccuracies and reduce size.
2.  **Block Resets:** Every 4 records, an Absolute Baseline is written.
3.  **Timestamps:** Record 1 is Absolute. Record 2 is a Delta. Records 3+ are Delta-of-Delta (the change in the gap).
4.  **Encoding:** Deltas are passed through ZigZag (mapping negatives to positives) and encoded as Varints (using the 8th bit as a continue flag), drastically reducing disk usage.

## Quick Start

### Prerequisites
*   `gcc`
*   `jdk 11+`
*   `make`

### Build & Run
```bash
make clean && make
make server
```

### API Endpoints
*   **Insert Data:** `curl "http://localhost:8080/insert?time=<long>&price=<double>"`
*   **Query Data:** `curl "http://localhost:8080/query?start=<long>&end=<long>"`
*   **Close Engine:** `curl "http://localhost:8080/close"` *(Flushes RAM buffer and writes index footer)*

## Architecture
*   **Java Layer:** HTTP routing, thread pooling, Fixed-Point conversion, and JNI pointer management.
*   **C Layer (`ChronosCore.c`):** State machine management, RAM buffering, bitwise math, `fwrite` I/O, and Sparse Index generation.

## License
This project is open source and available under the [MIT License](LICENSE).