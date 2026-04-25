#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "Building project..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)

echo "----------------------------------------"
echo "Running NSE Snapshot parser..."
echo "----------------------------------------"
./build/market_data_parser FO_SnapshotData26_12_2025.bin nse
