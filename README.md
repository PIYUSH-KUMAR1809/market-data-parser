# High-Performance Market Data Parser

A production-grade, low-latency C++ market data engine designed for High-Frequency Trading (HFT) applications. It ingests raw exchange feeds (NASDAQ ITCH 5.0, NSE FO), standardizes them efficiently, and maintains a clean Limit Order Book (LOB) State of the World.

## Current Benchmarks

**Hardware Environment**: Apple Silicon M-Series (Tested on 10-core CPU)

| Component | Throughput | Items per Second | Description |
| :--- | :--- | :--- | :--- |
| **NSE Parser** | **~381 MB/s** | **~247k msgs/sec** | Zero-Copy Parser |
| **NASDAQ ITCH** | **~169 MB/s** | **~5.5 Million msgs/sec** | Zero-Copy Parser |
| **Order Book (Add)** | - | **~9M to 22M ops/sec** | Core Engine insertion latency (varies by book size) |
| **Order Book (Match)** | - | **~236M to 291M ops/sec** | Core Engine exact match latency |

*Achieved via direct buffer casting, custom memory resources (PMR), and branch-free endian conversion.*

### End-to-End Execution (Real-World Data)
Processing a full **11.24 GB** historical NASDAQ ITCH 5.0 file (`01302019.NASDAQ_ITCH50`):
*   **Total Messages Parsed**: 368,366,634
*   **Execution Time**: 97.48 seconds
*   **Throughput**: 3.77 Million messages/sec
*   **Mean Latency per message**: 242 ns
*   **P50 Latency**: 84 ns

Processing a full **13.11 GB** NSE FO Snapshot file (`FO_SnapshotData26_12_2025.bin`):
*   **Active Orders Parsed**: 484,860
*   **Execution Time**: 31.12 seconds
*   **Throughput**: ~421 MB/sec

**To run the microbenchmarks yourself:**
```bash
./build/benchmark_runner
```

## Features

### 1. High-Performance Architecture
*   **Sharding**: Orders are routed to dedicated per-symbol OrderBooks (`ShardManager`) for parallel-ready architecture.
*   **Zero-Copy Logic**: Heavy use of `std::string_view`, raw pointers, and memory mapping.
*   **PMR (Polymorphic Memory Resources)**: Uses `std::pmr::monotonic_buffer_resource` (Arena Allocation) to eliminate `malloc`/`new` latency during trading.
*   **DenseMap**: Custom Open-Addressing Hash Map implementation to replace `std::unordered_map`, guaranteeing O(1) lookups and maximizing CPU cache locality.

### 2. Multi-Exchange Support
*   **NASDAQ**: Full ITCH 5.0 parsing support.
*   **NSE**: NSE Futures & Options snapshot parsing support.
*   **Normalized**: All data is converted to a standard `Order` and `PriceLevel` structure used internally.

## Where to get Sample Data

### 1. Real NASDAQ ITCH Data
You can download historical, full-day NASDAQ ITCH 5.0 sample data directly from the official NASDAQ site:
* **Download Link:** `https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/`

### 2. Generate Local Test Data
If you want to quickly test the parser without downloading gigabytes of real data, you can use the included python script to generate a small, mock binary file:
```bash
python3 generate_data.py
```
This will create `data/test_data.bin` which you can pass to the parser.

## Build and Run Instructions

### Prerequisites
*   C++20 compliant compiler (Clang/GCC)
*   CMake 3.20+

### Quick Start
We provide convenient bash scripts that automatically handle building the project and running the executable against sample files.

**To build and run the NASDAQ ITCH parser:**
```bash
./run_itch.sh
```

**To build and run the NSE parser:**
```bash
./run_nse.sh
```

### Manual Execution
If you prefer to build and run manually:
```bash
# Build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)

# Run
./build/market_data_parser <path_to_data_file> [mode: itch|nse]
```
