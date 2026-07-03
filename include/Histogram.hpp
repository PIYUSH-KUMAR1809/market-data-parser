#pragma once

#include <vector>
#include "spdlog/spdlog.h"


namespace MarketData {

class Histogram {
   public:
    static constexpr size_t MAX_LATENCY_NS = 100000;
    static constexpr size_t BUCKET_SIZE = 1;

    std::vector<uint64_t> buckets;
    uint64_t overflow = 0;
    uint64_t count = 0;
    uint64_t sum = 0;
    uint64_t min_ = ~0UL;
    uint64_t max_ = 0;

    Histogram() : buckets(MAX_LATENCY_NS + 1, 0) {}

    void record(uint64_t val) {
        count++;
        sum += val;
        if (val < min_) min_ = val;
        if (val > max_) max_ = val;

        if (val < MAX_LATENCY_NS) {
            buckets[val]++;
        } else {
            overflow++;
        }
    }

    void printStats() const {
        if (count == 0) return;

        double mean = static_cast<double>(sum) / static_cast<double>(count);

        spdlog::info("Latency Statistics (ns):");
        spdlog::info("  Count: {}", count);
        spdlog::info("  Min:   {}", min_);
        spdlog::info("  Mean:  {:.2f}", mean);
        spdlog::info("  Max:   {}", max_);
        spdlog::info("  P50:   {}", getPercentile(50.0));
        spdlog::info("  P99:   {}", getPercentile(99.0));
        spdlog::info("  P99.9: {}", getPercentile(99.9));

        if (overflow > 0) {
            spdlog::warn("  Overflow (>100us): {}", overflow);
        }
    }

   private:
    uint64_t getPercentile(double p) const {
        if (count == 0) return 0;
        uint64_t threshold = static_cast<uint64_t>(static_cast<double>(count) * (p / 100.0));
        uint64_t current = 0;

        for (size_t i = 0; i < buckets.size(); ++i) {
            current += buckets[i];
            if (current >= threshold) {
                return i;
            }
        }
        return MAX_LATENCY_NS;
    }
};

}
