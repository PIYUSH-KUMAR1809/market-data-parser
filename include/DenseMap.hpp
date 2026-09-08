#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <new>
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
    using CtrlVector  = std::vector<uint8_t, std::pmr::polymorphic_allocator<uint8_t>>;

    explicit DenseMap(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : alloc_(mr), ctrl_(mr) {
        rehash(kMinCap);
    }

    DenseMap(const DenseMap&) = delete;
    DenseMap& operator=(const DenseMap&) = delete;

    DenseMap(DenseMap&& other) noexcept
        : alloc_(other.alloc_),
          hash_(std::move(other.hash_)),
          equal_(std::move(other.equal_)),
          slots_(other.slots_),
          ctrl_(std::move(other.ctrl_)),
          capacity_(other.capacity_),
          mask_(other.mask_),
          size_(other.size_),
          threshold_(other.threshold_) {
        other.slots_     = nullptr;
        other.capacity_  = 0;
        other.mask_      = 0;
        other.size_      = 0;
        other.threshold_ = 0;
    }

    DenseMap& operator=(DenseMap&& other) noexcept {
        if (this == &other) return *this;
        destroyTable();
        alloc_     = other.alloc_;
        hash_      = std::move(other.hash_);
        equal_     = std::move(other.equal_);
        slots_     = other.slots_;
        ctrl_      = std::move(other.ctrl_);
        capacity_  = other.capacity_;
        mask_      = other.mask_;
        size_      = other.size_;
        threshold_ = other.threshold_;
        other.slots_     = nullptr;
        other.capacity_  = 0;
        other.mask_      = 0;
        other.size_      = 0;
        other.threshold_ = 0;
        return *this;
    }

    ~DenseMap() { destroyTable(); }

    void reserve(size_t n) {
        size_t needed = minCap(n);
        if (needed > capacity_) rehash(needed);
    }

    Value* find(const Key& key) {
        if (size_ == 0) return nullptr;
        size_t  idx = hashOf(key) & mask_;
        uint8_t psl = 1;
        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0) return nullptr;
            if (c < psl) return nullptr;
            if (c == psl && equal_(slots_[idx].first, key))
                return &slots_[idx].second;
            idx = (idx + 1) & mask_;
            ++psl;
        }
    }

    template <typename... Args>
    std::pair<Value*, bool> try_emplace(const Key& key, Args&&... args) {
        if (__builtin_expect(size_ >= threshold_, 0))
            rehash(capacity_ ? capacity_ * 2 : kMinCap);

        size_t  idx = hashOf(key) & mask_;
        uint8_t psl = 1;

        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0) {
                ctrl_[idx] = psl;
                construct(idx, key, std::forward<Args>(args)...);
                ++size_;
                return {&slots_[idx].second, true};
            }
            if (c == psl && equal_(slots_[idx].first, key))
                return {&slots_[idx].second, false};
            if (c < psl)
                break;
            idx = (idx + 1) & mask_;
            ++psl;
        }

        PairType ins{key, Value(std::forward<Args>(args)...)};
        Value*   result = nullptr;
        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0) {
                ctrl_[idx] = psl;
                constructMove(idx, std::move(ins));
                ++size_;
                if (result == nullptr) result = &slots_[idx].second;
                return {result, true};
            }
            if (c < psl) {
                std::swap(ctrl_[idx], psl);
                std::swap(slots_[idx], ins);
                if (result == nullptr) result = &slots_[idx].second;
            }
            idx = (idx + 1) & mask_;
            ++psl;
        }
    }

    template <typename... Args>
    Value& emplace(const Key& key, Args&&... args) {
        return *try_emplace(key, std::forward<Args>(args)...).first;
    }

    Value& operator[](const Key& key) { return emplace(key); }

    void erase(const Key& key) {
        if (size_ == 0) return;
        size_t  idx = hashOf(key) & mask_;
        uint8_t psl = 1;

        while (true) {
            uint8_t c = ctrl_[idx];
            if (c == 0 || c < psl) return;
            if (c == psl && equal_(slots_[idx].first, key)) break;
            idx = (idx + 1) & mask_;
            ++psl;
        }

        size_t cur = idx;
        size_t nxt = (cur + 1) & mask_;
        while (true) {
            uint8_t nc = ctrl_[nxt];
            if (nc <= 1) break;
            ctrl_[cur]  = nc - 1;
            slots_[cur] = std::move(slots_[nxt]);
            cur = nxt;
            nxt = (nxt + 1) & mask_;
        }
        destroy(cur);
        ctrl_[cur] = 0;
        --size_;
    }

    size_t size()     const { return size_; }
    size_t capacity() const { return capacity_; }

    template <typename F>
    void forEach(F&& f) const {
        for (size_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i] != 0)
                f(slots_[i].first, slots_[i].second);
        }
    }

   private:
    static constexpr size_t kMinCap = 256;

    std::pmr::polymorphic_allocator<PairType> alloc_;
    Hash       hash_{};
    Equal      equal_{};
    PairType*  slots_     = nullptr;
    CtrlVector ctrl_;
    size_t     capacity_  = 0;
    size_t     mask_      = 0;
    size_t     size_      = 0;
    size_t     threshold_ = 0;

    static size_t mix(size_t h) noexcept {
        uint64_t x = static_cast<uint64_t>(h);
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return static_cast<size_t>(x);
    }

    size_t hashOf(const Key& key) const { return mix(hash_(key)); }

    static size_t minCap(size_t n) {
        size_t c = kMinCap;
        while (c * 3 / 4 < n) c <<= 1;
        return c;
    }

    template <typename... Args>
    void construct(size_t idx, const Key& key, Args&&... args) {
        ::new (static_cast<void*>(slots_ + idx))
            PairType(key, Value(std::forward<Args>(args)...));
    }

    void constructMove(size_t idx, PairType&& p) {
        ::new (static_cast<void*>(slots_ + idx)) PairType(std::move(p));
    }

    void destroy(size_t idx) { slots_[idx].~PairType(); }

    void destroyTable() {
        if (slots_ == nullptr) return;
        for (size_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i] != 0) destroy(i);
        }
        alloc_.deallocate(slots_, capacity_);
        slots_     = nullptr;
        capacity_  = 0;
        mask_      = 0;
        size_      = 0;
        threshold_ = 0;
        ctrl_.clear();
    }

    void relocate(PairType&& ins, PairType* slots, CtrlVector& ctrl, size_t mask) {
        size_t  idx = hashOf(ins.first) & mask;
        uint8_t psl = 1;
        while (true) {
            uint8_t c = ctrl[idx];
            if (c == 0) {
                ctrl[idx] = psl;
                ::new (static_cast<void*>(slots + idx)) PairType(std::move(ins));
                return;
            }
            if (c < psl) {
                std::swap(ctrl[idx], psl);
                std::swap(slots[idx], ins);
            }
            idx = (idx + 1) & mask;
            ++psl;
        }
    }

    void rehash(size_t newCap) {
        PairType*  oldSlots = slots_;
        CtrlVector oldCtrl  = std::move(ctrl_);
        size_t     oldCap   = capacity_;

        auto* mr = alloc_.resource();
        PairType*  newSlots = alloc_.allocate(newCap);
        CtrlVector newCtrl(newCap, 0, std::pmr::polymorphic_allocator<uint8_t>(mr));
        size_t     newMask  = newCap - 1;

        for (size_t i = 0; i < oldCap; ++i) {
            if (oldCtrl[i] == 0) continue;
            relocate(std::move(oldSlots[i]), newSlots, newCtrl, newMask);
            oldSlots[i].~PairType();
        }

        if (oldSlots != nullptr) {
            alloc_.deallocate(oldSlots, oldCap);
        }

        slots_     = newSlots;
        ctrl_      = std::move(newCtrl);
        capacity_  = newCap;
        mask_      = newMask;
        threshold_ = newCap * 3 / 4;
    }
};

}
