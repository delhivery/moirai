#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// Sharded concurrent cache with CLOCK eviction.
//
// Design:
// - N shards (power-of-two), each with its own hash map + shared_mutex
// - Reads take shared_lock (multiple concurrent readers per shard)
// - Writes take unique_lock (exclusive per shard, but shards are independent)
// - CLOCK eviction per shard when shard exceeds capacity/N entries
// - Entries stored as shared_ptr for zero-copy reads
//
// With 64 shards and 8 solver threads, probability of two threads hitting
// the same shard simultaneously is ~12%. Of those, read-read doesn't block.
// Write-write and write-read on the same shard block briefly — but writes
// (cache misses) are the minority once the cache warms up.

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

  explicit ConcurrentCache(std::size_t capacity,
                           std::size_t shard_count = 64)
    : m_shard_count(next_power_of_two(shard_count))
    , m_shard_mask(m_shard_count - 1)
    , m_shard_capacity(capacity / m_shard_count + 1)
    , m_shards(m_shard_count)
  {
    for (auto& shard : m_shards) {
      shard.entries.reserve(m_shard_capacity);
    }
  }

  ConcurrentCache(const ConcurrentCache&) = delete;
  auto operator=(const ConcurrentCache&) -> ConcurrentCache& = delete;

  auto find(std::string_view key) const -> std::shared_ptr<const Entry> {
    auto& shard = shard_for(key);
    std::shared_lock lock(shard.mutex);
    auto iter = shard.entries.find(key);
    if (iter == shard.entries.end()) {
      m_misses.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    iter->second.referenced.store(true, std::memory_order_relaxed);
    m_hits.fetch_add(1, std::memory_order_relaxed);
    return iter->second.entry;
  }

  void insert(std::string key, Value value) {
    auto entry = std::make_shared<const Entry>(
      Entry{key, std::move(value)});
    auto& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);

    auto [iter, inserted] = shard.entries.try_emplace(
      std::move(key), SlotData(std::move(entry)));
    if (!inserted) {
      iter->second.entry = std::move(entry);
    } else if (shard.entries.size() > m_shard_capacity) {
      evict_one(shard);
    }
    iter->second.referenced.store(true, std::memory_order_relaxed);
  }

  auto metrics() const -> Metrics {
    std::size_t total_occupied = 0;
    for (const auto& shard : m_shards) {
      std::shared_lock lock(shard.mutex);
      total_occupied += shard.entries.size();
    }
    return Metrics{
      .hits = m_hits.load(std::memory_order_relaxed),
      .misses = m_misses.load(std::memory_order_relaxed),
      .evictions = m_evictions.load(std::memory_order_relaxed),
      .occupied = total_occupied,
    };
  }

private:
  struct SlotData {
    std::shared_ptr<const Entry> entry;
    mutable std::atomic<bool> referenced{true};

    SlotData() = default;
    explicit SlotData(std::shared_ptr<const Entry> e)
      : entry(std::move(e)), referenced(true) {}
    SlotData(SlotData&& other) noexcept
      : entry(std::move(other.entry))
      , referenced(other.referenced.load(std::memory_order_relaxed)) {}
    auto operator=(SlotData&& other) noexcept -> SlotData& {
      entry = std::move(other.entry);
      referenced.store(other.referenced.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
      return *this;
    }
    SlotData(const SlotData&) = delete;
    auto operator=(const SlotData&) -> SlotData& = delete;
  };

  struct TransparentHash {
    using is_transparent = void;
    auto operator()(std::string_view sv) const noexcept -> std::size_t {
      return std::hash<std::string_view>{}(sv);
    }
    auto operator()(const std::string& s) const noexcept -> std::size_t {
      return std::hash<std::string_view>{}(s);
    }
  };

  struct TransparentEqual {
    using is_transparent = void;
    auto operator()(std::string_view a, std::string_view b) const noexcept -> bool {
      return a == b;
    }
  };

  struct Shard {
    mutable std::shared_mutex mutex;
    std::unordered_map<std::string, SlotData, TransparentHash, TransparentEqual> entries;
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

  auto shard_for(std::string_view key) const -> Shard& {
    const auto hash = std::hash<std::string_view>{}(key);
    return m_shards[hash & m_shard_mask];
  }

  void evict_one(Shard& shard) {
    // CLOCK sweep: find an unreferenced entry and erase it
    for (std::size_t pass = 0; pass < 2; ++pass) {
      for (auto iter = shard.entries.begin(); iter != shard.entries.end(); ++iter) {
        if (iter->second.referenced.exchange(false, std::memory_order_relaxed)) {
          continue;
        }
        shard.entries.erase(iter);
        m_evictions.fetch_add(1, std::memory_order_relaxed);
        return;
      }
    }
    // All referenced after 2 passes — evict the first entry
    if (!shard.entries.empty()) {
      shard.entries.erase(shard.entries.begin());
      m_evictions.fetch_add(1, std::memory_order_relaxed);
    }
  }

  std::size_t m_shard_count;
  std::size_t m_shard_mask;
  std::size_t m_shard_capacity;
  mutable std::vector<Shard> m_shards;

  mutable std::atomic<std::uint64_t> m_hits{0};
  mutable std::atomic<std::uint64_t> m_misses{0};
  std::atomic<std::uint64_t> m_evictions{0};
};
