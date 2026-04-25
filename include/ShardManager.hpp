#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <vector>

#include "OrderBook.hpp"

namespace MarketData {

class ShardManager {
   public:
    ShardManager() = default;

    OrderBook* getBook(size_t symbolId) {
        if (symbolId >= books_.size()) {
            size_t newSize = symbolId + 1024;
            books_.resize(newSize, nullptr);
        }

        if (books_[symbolId] == nullptr) {
            auto buffer = new std::pmr::monotonic_buffer_resource(static_cast<size_t>(1024 * 1024));
            buffers_.emplace_back(buffer);

            books_[symbolId] = new OrderBook(buffer);
        }
        return books_[symbolId];
    }

    size_t getTotalOrderCount() const {
        size_t total = 0;
        for (const auto* book : books_) {
            if (book) {
                total += book->getOrderCount();
            }
        }
        return total;
    }

    ~ShardManager() {
        for (auto* book : books_) {
            if (book) delete book;
        }
    }

   private:
    std::vector<OrderBook*> books_;
    std::vector<std::unique_ptr<std::pmr::monotonic_buffer_resource>> buffers_;
};

}  // namespace MarketData
