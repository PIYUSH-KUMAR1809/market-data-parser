#include <benchmark/benchmark.h>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "ItchParser.hpp"
#include "OrderBook.hpp"
#include "parsers/nse/NseFoParser.hpp"

static std::vector<char> g_itch_buffer;
static std::vector<char> g_nse_buffer;

static void SetupGlobalBuffers() {
    if (g_itch_buffer.empty()) {
        const char* filename = "01302019.NASDAQ_ITCH50";
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            filename = "../01302019.NASDAQ_ITCH50";
            file.open(filename, std::ios::binary | std::ios::ate);
        }

        if (file) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            size_t read_size = std::min((size_t)size, (size_t)(500 * 1024 * 1024));
            g_itch_buffer.resize(read_size);
            file.read(g_itch_buffer.data(), static_cast<std::streamsize>(read_size));
        } else {
            g_itch_buffer.resize(1024);
        }
    }

    if (g_nse_buffer.empty()) {
        const char* filename = "FO_SnapshotData26_12_2025.bin";
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            filename = "../FO_SnapshotData26_12_2025.bin";
            file.open(filename, std::ios::binary | std::ios::ate);
        }

        if (file) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            size_t read_size = std::min((size_t)size, (size_t)(500 * 1024 * 1024));
            g_nse_buffer.resize(read_size);
            file.read(g_nse_buffer.data(), static_cast<std::streamsize>(read_size));
        } else {
        }
    }
}

static void BM_ParseItchBuffer(benchmark::State& state) {
    if (g_itch_buffer.size() < 1024) SetupGlobalBuffers();
    if (g_itch_buffer.size() < 1024) {
        state.SkipWithError("ITCH file not found or too small");
        return;
    }

    int64_t total_msgs = 0;
    int64_t total_bytes = 0;

    for (auto tmp : state) {
        MarketData::ItchParser parser;
        parser.printDebug = false;

        parser.parseBuffer(g_itch_buffer.data(), g_itch_buffer.size());

        benchmark::DoNotOptimize(parser);
        total_msgs += static_cast<int64_t>(parser.messagesParsed);
        total_bytes += static_cast<int64_t>(parser.totalBytesParsed);
    }
    state.SetItemsProcessed(total_msgs);
    state.SetBytesProcessed(total_bytes);
}

static void BM_ParseNseBuffer(benchmark::State& state) {
    if (g_nse_buffer.size() < 1024) SetupGlobalBuffers();
    if (g_nse_buffer.size() < 1024) {
        state.SkipWithError("NSE file not found or too small");
        return;
    }

    int64_t total_msgs = 0;
    int64_t total_bytes = 0;

    for (auto tmp : state) {
        NseFo::NseFoParser parser;

        parser.parseBuffer(g_nse_buffer.data(), g_nse_buffer.size());

        benchmark::DoNotOptimize(parser);
        total_msgs += static_cast<int64_t>(parser.getTotalOrderCount());
        total_bytes += static_cast<int64_t>(g_nse_buffer.size());
    }
    state.SetItemsProcessed(total_msgs);
    state.SetBytesProcessed(total_bytes);
}

static void BM_OrderBook_Add(benchmark::State& state) {
    MarketData::OrderBook book;
    uint64_t id = 0;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> priceDist(1, 100000);

    const size_t N = state.range(0);
    std::vector<int64_t> prices(N);
    for (auto& p : prices) p = priceDist(rng);

    for (auto _ : state) {
        for (size_t i = 0; i < N; ++i) {
            book.addOrder(++id, 1000, true, prices[i], 100, "TEST");
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * N));
}

static void BM_OrderBook_ExactMatch(benchmark::State& state) {
    size_t N = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        MarketData::OrderBook book;
        uint64_t id = 0;
        for (size_t i = 0; i < N; ++i) {
            book.addOrder(++id, 1000, true, 10000, 100, "TEST");
        }
        state.ResumeTiming();

        for (size_t i = 1; i <= N; ++i) {
            book.executeOrder(i, 100);
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * N));
}

BENCHMARK(BM_ParseItchBuffer)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ParseNseBuffer)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_OrderBook_Add)->Range(1000, 100000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_OrderBook_ExactMatch)->Range(1000, 100000)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
