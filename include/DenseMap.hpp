#pragma once

#include <functional>
#include <memory_resource>
#include <utility>
#include <vector>

namespace MarketData {

template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>>
class DenseMap {
   public:
    using PairType = std::pair<Key, Value>;
    using PmrVector = std::vector<PairType, std::pmr::polymorphic_allocator<PairType>>;
    using MetaVector = std::vector<int8_t, std::pmr::polymorphic_allocator<int8_t>>;

    enum Status : int8_t { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };

    explicit DenseMap(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : entries_(mr), meta_(mr) {
        reserve(16);
    }

    void reserve(size_t capacity) {
        if (capacity <= capacity_) return;

        size_t newCap = 1;
        while (newCap < capacity) newCap <<= 1;

        PmrVector newEntries(newCap, entries_.get_allocator());
        MetaVector newMeta(newCap, EMPTY, meta_.get_allocator());

        size_t mask = newCap - 1;
        size_t activeCount = 0;

        size_t oldCap = capacity_;
        for (size_t i = 0; i < oldCap; ++i) {
            if (meta_[i] == OCCUPIED) {
                size_t hash = Hash{}(entries_[i].first);
                size_t idx = hash & mask;

                while (newMeta[idx] == OCCUPIED) {
                    idx = (idx + 1) & mask;
                }

                newEntries[idx] = std::move(entries_[i]);
                newMeta[idx] = OCCUPIED;
                activeCount++;
            }
        }

        entries_ = std::move(newEntries);
        meta_ = std::move(newMeta);
        capacity_ = newCap;
        mask_ = capacity_ - 1;

        size_ = activeCount;
        slots_used_ = activeCount;
    }

    Value* find(const Key& key) {
        if (size_ == 0) return nullptr;

        size_t idx = Hash{}(key)&mask_;

        while (meta_[idx] != EMPTY) {
            if (meta_[idx] == OCCUPIED && Equal{}(entries_[idx].first, key)) {
                return &entries_[idx].second;
            }
            idx = (idx + 1) & mask_;
        }
        return nullptr;
    }

    template <typename... Args>
    Value& emplace(const Key& key, Args&&... args) {
        if (slots_used_ >= capacity_ * 3 / 4) {
            reserve(capacity_ * 2);
        }

        size_t idx = Hash{}(key)&mask_;
        size_t firstDeleted = -1;

        while (meta_[idx] != EMPTY) {
            if (meta_[idx] == OCCUPIED && Equal{}(entries_[idx].first, key)) {
                return entries_[idx].second;
            }
            if (meta_[idx] == DELETED && firstDeleted == static_cast<size_t>(-1)) {
                firstDeleted = idx;
            }
            idx = (idx + 1) & mask_;
        }

        if (firstDeleted != static_cast<size_t>(-1)) {
            idx = firstDeleted;
        } else {
            slots_used_++;
        }

        entries_[idx].first = key;
        entries_[idx].second = Value(std::forward<Args>(args)...);
        meta_[idx] = OCCUPIED;
        size_++;

        return entries_[idx].second;
    }

    Value& operator[](const Key& key) { return emplace(key); }

    void erase(const Key& key) {
        if (size_ == 0) return;

        size_t idx = Hash{}(key)&mask_;

        while (meta_[idx] != EMPTY) {
            if (meta_[idx] == OCCUPIED && Equal{}(entries_[idx].first, key)) {
                meta_[idx] = DELETED;
                size_--;
                return;
            }
            idx = (idx + 1) & mask_;
        }
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    template <typename F>
    void forEach(F&& f) const {
        for (size_t i = 0; i < capacity_; ++i) {
            if (meta_[i] == OCCUPIED) {
                f(entries_[i].first, entries_[i].second);
            }
        }
    }

   private:
    PmrVector entries_;
    MetaVector meta_;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    size_t size_ = 0;
    size_t slots_used_ = 0;
};

}
