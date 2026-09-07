#pragma once

#include <cstring>
#include <functional>
#include <memory_resource>
#include <utility>
#include <vector>

namespace MarketData {

template <typename Key,
          typename Value,
          typename Hash  = std::hash<Key>,
          typename Equal = std::equal_to<Key>>
class DenseMap {
   public:
    using PairType    = std::pair<Key, Value>;
    using EntryVector = std::vector<PairType, std::pmr::polymorphic_allocator<PairType>>;
    using CtrlVector  = std::vector<uint8_t, std::pmr::polymorphic_allocator<uint8_t>>;

    explicit DenseMap(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : entries_(mr), ctrl_(mr) {
        rehash(16);
    }

    void reserve(size_t n) {
        size_t needed = minCap(n);
        if (needed > capacity_) rehash(needed);
    }

    Value* find(const Key& key) {
        if (size_ == 0) return nullptr;
        size_t  idx = Hash{}(key) & mask_;
        uint8_t psl = 1;
        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0) return nullptr;
            if (c < psl) return nullptr;
            if (c == psl && Equal{}(entries_[idx].first, key))
                return &entries_[idx].second;
            idx = (idx + 1) & mask_;
            ++psl;
        }
    }

    template <typename... Args>
    Value& emplace(const Key& key, Args&&... args) {
        if (__builtin_expect(size_ >= threshold_, 0))
            rehash(capacity_ ? capacity_ * 2 : 16);

        size_t   idx = Hash{}(key) & mask_;
        uint8_t  psl = 1;
        PairType ins{key, Value(std::forward<Args>(args)...)};

        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0) {
                ctrl_[idx]    = psl;
                entries_[idx] = std::move(ins);
                ++size_;
                return entries_[idx].second;
            }
            if (c == psl && Equal{}(entries_[idx].first, ins.first))
                return entries_[idx].second;
            if (c < psl) {
                std::swap(ctrl_[idx], psl);
                std::swap(entries_[idx], ins);
            }
            idx = (idx + 1) & mask_;
            ++psl;
        }
    }

    Value& operator[](const Key& key) { return emplace(key); }

    void erase(const Key& key) {
        if (size_ == 0) return;
        size_t  idx = Hash{}(key) & mask_;
        uint8_t psl = 1;

        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0 || c < psl) return;
            if (c == psl && Equal{}(entries_[idx].first, key)) break;
            idx = (idx + 1) & mask_;
            ++psl;
        }

        size_t cur  = idx;
        size_t nxt  = (cur + 1) & mask_;
        while (true) {
            uint8_t nc = ctrl_[nxt];
            if (nc <= 1) break;
            ctrl_[cur]    = nc - 1;
            entries_[cur] = std::move(entries_[nxt]);
            cur = nxt;
            nxt = (nxt + 1) & mask_;
        }
        ctrl_[cur] = 0;
        --size_;
    }

    size_t size()     const { return size_; }
    size_t capacity() const { return capacity_; }

    template <typename F>
    void forEach(F&& f) const {
        for (size_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i] != 0)
                f(entries_[i].first, entries_[i].second);
        }
    }

   private:
    EntryVector entries_;
    CtrlVector  ctrl_;
    size_t      capacity_  = 0;
    size_t      mask_      = 0;
    size_t      size_      = 0;
    size_t      threshold_ = 0;

    static size_t minCap(size_t n) {
        size_t c = 16;
        while (c * 7 / 8 < n) c <<= 1;
        return c;
    }

    void rehash(size_t newCap) {
        EntryVector newEntries(newCap, entries_.get_allocator());
        CtrlVector  newCtrl(newCap, 0, ctrl_.get_allocator());
        size_t      newMask = newCap - 1;

        for (size_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i] == 0) continue;
            size_t   idx = Hash{}(entries_[i].first) & newMask;
            uint8_t  psl = 1;
            PairType ins = std::move(entries_[i]);
            while (true) {
                uint8_t c = newCtrl[idx];
                if (c == 0) {
                    newCtrl[idx]    = psl;
                    newEntries[idx] = std::move(ins);
                    break;
                }
                if (c < psl) {
                    std::swap(newCtrl[idx], psl);
                    std::swap(newEntries[idx], ins);
                }
                idx = (idx + 1) & newMask;
                ++psl;
            }
        }

        entries_   = std::move(newEntries);
        ctrl_      = std::move(newCtrl);
        capacity_  = newCap;
        mask_      = newCap - 1;
        threshold_ = newCap * 7 / 8;
    }
};

}
