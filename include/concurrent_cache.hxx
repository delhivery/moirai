#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Lock-free concurrent cache with epoch-based reclamation.
//
// Design:
// - Fixed-capacity open-addressing hash table with power-of-two sizing
// - Reads are lock-free: atomic shared_ptr load from the slot
// - Writes use per-slot spinlocks (1 byte each, no contention across slots)
// - Eviction: CLOCK algorithm (approximates LRU without per-access locking)
// - Shared across threads; entries are immutable once published
//
// The key insight: once a shared_ptr<Entry> is published to a slot, any thread
// that loaded it holds a reference. The slot can be overwritten freely — old
// readers still see consistent data until they release their shared_ptr.

template <typename Value>
class ConcurrentCache {
public:
  struct Entry {
    std::string key;
    Value value;
  };

  struct Metrics {
    std::uint64_t hits{};
    std::uint64_t misses{};
    std::uint64_t evictions{};
    std::size_t occupied{};
  };

  explicit ConcurrentCache(std::size_t capacity)
    : m_capacity(next_power_of_two(capacity))
    , m_mask(m_capacity - 1)
    , m_slots(m_capacity)
    , m_locks(m_capacity)
    , m_referenced(m_capacity)
    , m_clock_hand(0)
  {
  }

  ConcurrentCache(const ConcurrentCache&) = delete;
  auto operator=(const ConcurrentCache&) -> ConcurrentCache& = delete;

  auto find(std::string_view key) const -> std::shared_ptr<const Entry> {
    const auto hash = hash_key(key);
    const auto max_probe = probe_limit();

    for (std::size_t probe = 0; probe < max_probe; ++probe) {
      const auto slot = (hash + probe) & m_mask;
      auto entry = m_slots[slot].load(std::memory_order_acquire);
      if (!entry) {
        m_misses.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
      }
      if (entry->key == key) {
        m_referenced[slot].store(true, std::memory_order_relaxed);
        m_hits.fetch_add(1, std::memory_order_relaxed);
        return entry;
      }
    }

    m_misses.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }

  void insert(std::string key, Value value) {
    auto entry = std::make_shared<const Entry>(
      Entry{std::move(key), std::move(value)});
    const auto hash = hash_key(entry->key);
    const auto max_probe = probe_limit();

    // Try to find an existing slot or an empty one
    for (std::size_t probe = 0; probe < max_probe; ++probe) {
      const auto slot = (hash + probe) & m_mask;
      SlotLock guard(m_locks[slot]);

      auto existing = m_slots[slot].load(std::memory_order_acquire);
      if (!existing) {
        m_slots[slot].store(std::move(entry), std::memory_order_release);
        m_referenced[slot].store(true, std::memory_order_relaxed);
        m_occupied.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      if (existing->key == entry->key) {
        m_slots[slot].store(std::move(entry), std::memory_order_release);
        m_referenced[slot].store(true, std::memory_order_relaxed);
        return;
      }
    }

    // All probe slots occupied — evict via CLOCK and place in evicted slot
    const auto evicted_slot = clock_evict();
    SlotLock guard(m_locks[evicted_slot]);
    auto prev = m_slots[evicted_slot].load(std::memory_order_acquire);
    m_slots[evicted_slot].store(std::move(entry), std::memory_order_release);
    m_referenced[evicted_slot].store(true, std::memory_order_relaxed);
    if (!prev) {
      m_occupied.fetch_add(1, std::memory_order_relaxed);
    } else {
      m_evictions.fetch_add(1, std::memory_order_relaxed);
    }
  }

  auto metrics() const -> Metrics {
    return {
      .hits = m_hits.load(std::memory_order_relaxed),
      .misses = m_misses.load(std::memory_order_relaxed),
      .evictions = m_evictions.load(std::memory_order_relaxed),
      .occupied = m_occupied.load(std::memory_order_relaxed),
    };
  }

private:
  struct alignas(64) SpinLock {
    std::atomic<bool> locked{false};
  };

  class SlotLock {
  public:
    explicit SlotLock(SpinLock& lock) : m_lock(lock) {
      while (m_lock.locked.exchange(true, std::memory_order_acquire)) {
        while (m_lock.locked.load(std::memory_order_relaxed)) {
          // spin
        }
      }
    }
    ~SlotLock() { m_lock.locked.store(false, std::memory_order_release); }
    SlotLock(const SlotLock&) = delete;
    auto operator=(const SlotLock&) -> SlotLock& = delete;
  private:
    SpinLock& m_lock;
  };

  static auto next_power_of_two(std::size_t value) -> std::size_t {
    if (value == 0) return 1;
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return value + 1;
  }

  static auto hash_key(std::string_view key) -> std::size_t {
    return std::hash<std::string_view>{}(key);
  }

  auto probe_limit() const -> std::size_t {
    return std::min(m_capacity, std::size_t{32});
  }

  auto clock_evict() -> std::size_t {
    const auto full_sweep = m_capacity * 2;
    for (std::size_t attempt = 0; attempt < full_sweep; ++attempt) {
      const auto slot = m_clock_hand.fetch_add(1, std::memory_order_relaxed) & m_mask;
      if (m_referenced[slot].exchange(false, std::memory_order_relaxed)) {
        continue;
      }
      auto existing = m_slots[slot].load(std::memory_order_acquire);
      if (existing) {
        return slot;
      }
    }
    return m_clock_hand.fetch_add(1, std::memory_order_relaxed) & m_mask;
  }

  std::size_t m_capacity;
  std::size_t m_mask;
  std::vector<std::atomic<std::shared_ptr<const Entry>>> m_slots;
  std::vector<SpinLock> m_locks;
  mutable std::vector<std::atomic<bool>> m_referenced;
  std::atomic<std::size_t> m_clock_hand;

  mutable std::atomic<std::uint64_t> m_hits{0};
  mutable std::atomic<std::uint64_t> m_misses{0};
  std::atomic<std::uint64_t> m_evictions{0};
  std::atomic<std::size_t> m_occupied{0};
};
