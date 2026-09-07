#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <vector>

#include "OrderBook.hpp"

namespace MarketData {

class ShardManager {
   public:
    explicit ShardManager(size_t initialArenaBytes = 32 * 1024 * 1024)
        : books_(200005, nullptr) {
        buffers_.emplace_back(std::make_unique<std::pmr::monotonic_buffer_resource>(initialArenaBytes));
    }

    ShardManager(ShardManager&&) noexcept = default;
    ShardManager& operator=(ShardManager&&) noexcept = default;

    OrderBook* getBook(size_t symbolId) {
        if (__builtin_expect(symbolId >= books_.size(), 0)) {
            books_.resize(symbolId + 1024, nullptr);
        }

        if (__builtin_expect(books_[symbolId] == nullptr, 0)) {
            books_[symbolId] = new OrderBook(buffers_.front().get());
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

    void mergeFrom(ShardManager& other) {
        if (other.books_.size() > books_.size()) {
            books_.resize(other.books_.size(), nullptr);
        }
        for (size_t i = 0; i < other.books_.size(); ++i) {
            if (other.books_[i] != nullptr) {
                if (books_[i] == nullptr) {
                    books_[i] = other.books_[i];
                    other.books_[i] = nullptr;
                } else {
                    books_[i]->mergeFrom(*other.books_[i]);
                }
            }
        }
        for (auto& buf : other.buffers_) {
            buffers_.emplace_back(std::move(buf));
        }
        other.buffers_.clear();
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

}
