#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "Building project..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)

echo "----------------------------------------"
echo "Running NASDAQ ITCH parser..."
echo "----------------------------------------"
./build/market_data_parser 01302019.NASDAQ_ITCH50 itch
