# CHRONOS
### High-Performance Time-Series Storage Engine

[![C](https://img.shields.io/badge/C-17-blue?logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Java](https://img.shields.io/badge/Java-11-orange?logo=openjdk)](https://openjdk.org/)
[![JNI](https://img.shields.io/badge/JNI-Native-green)](https://en.wikipedia.org/wiki/Java_Native_Interface)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Chronos** is a specialized storage engine designed for high-frequency financial tick data. It combines the raw speed of C for I/O and compression with the accessibility of Java for networking.

## Key Features
*   **Hybrid Architecture:** C core for performance, Java front-end for accessibility, connected via JNI.
*   **Extreme Compression:** Utilizes Block-Aware Delta-of-Delta encoding (timestamps) and Fixed-Point Deltas (prices) to minimize disk usage.
*   **Sparse Indexing:** O(log N) query time using absolute baseline resets to enable fast data retrieval without full file scans.

## Architecture
The system is split into two layers:
1.  **The Core (C):** Handles binary file I/O, data compression logic, and memory management.
2.  **The Interface (Java):** Provides a simple HTTP Server (`com.sun.net.httpserver`) and manages the JNI bridge.

## Prerequisites
*   **GCC:** For compiling the native C library.
*   **JDK 11+:** For running the Java server.
*   **Make:** For build automation.

## License
This project is open source and available under the [MIT License](LICENSE).
