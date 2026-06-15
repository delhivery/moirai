#pragma once

#include "cameron314/concurrentqueue.h"
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <limits>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>
#include <utility>

template <typename T> class BlockingQueue {
public:
  explicit BlockingQueue(size_t capacity = std::numeric_limits<size_t>::max())
      : m_capacity(capacity),
        m_bounded(capacity != std::numeric_limits<size_t>::max()) {}

  auto enqueue(T value) -> bool { return wait_enqueue(std::move(value)); }

  auto wait_enqueue(T value, const std::stop_token &stop_token = {}) -> bool {
    {
      std::unique_lock lock(m_state_mutex);
      if (m_bounded) {
        m_not_full.wait(lock, stop_token, [this]() -> bool {
          return m_closed || (m_size + m_pending_pushes) < m_capacity;
        });
      }

      if (m_closed || stop_token.stop_requested()) {
        return false;
      }

      ++m_pending_pushes;
    }

    if (!m_queue.enqueue(std::move(value))) {
      std::scoped_lock lock(m_state_mutex);
      --m_pending_pushes;
      if (m_bounded) {
        m_not_full.notify_one();
      }
      return false;
    }

    {
      std::scoped_lock lock(m_state_mutex);
      --m_pending_pushes;
      if (m_closed) {
        T discarded;
        while (!m_queue.try_dequeue(discarded)) {
          std::this_thread::yield();
        }
        if (m_bounded) {
          m_not_full.notify_one();
        }
        return false;
      }
      ++m_size;
    }

    m_not_empty.notify_one();
    return true;
  }

  [[nodiscard]] auto size_approx() const -> size_t {
    std::scoped_lock lock(m_state_mutex);
    return m_size;
  }

  [[nodiscard]] auto empty() const -> bool {
    std::scoped_lock lock(m_state_mutex);
    return m_size == 0;
  }

  [[nodiscard]] auto closed() const -> bool {
    std::scoped_lock lock(m_state_mutex);
    return m_closed;
  }

  auto try_dequeue_bulk(T *items, size_t count) -> size_t {
    const auto reserved = reserve_items(count);
    if (reserved == 0) {
      return 0;
    }

    drain_reserved(items, reserved);
    return reserved;
  }

  auto wait_dequeue_bulk(std::span<T> items, const std::stop_token &stop_token)
      -> size_t {
    if (items.empty()) {
      return 0;
    }

    size_t reserved = 0;
    {
      std::unique_lock lock(m_state_mutex);
      m_not_empty.wait(lock, stop_token,
                       [this]() -> bool { return m_closed || m_size > 0; });
      if (m_size == 0) {
        return 0;
      }

      reserved = std::min(m_size, items.size());
      m_size -= reserved;
    }

    if (m_bounded) {
      m_not_full.notify_all();
    }

    drain_reserved(items.data(), reserved);
    return reserved;
  }

  void close() {
    {
      std::scoped_lock lock(m_state_mutex);
      m_closed = true;
    }

    m_not_empty.notify_all();
    m_not_full.notify_all();
  }

  void notify_all() {
    m_not_empty.notify_all();
    m_not_full.notify_all();
  }

private:
  auto reserve_items(size_t count) -> size_t {
    std::scoped_lock lock(m_state_mutex);
    const auto reserved = std::min(m_size, count);
    if (reserved > 0) {
      m_size -= reserved;
      if (m_bounded) {
        m_not_full.notify_all();
      }
    }

    return reserved;
  }

  void drain_reserved(T *items, size_t reserved) {
    size_t dequeued = 0;
    while (dequeued < reserved) {
      dequeued +=
          m_queue.try_dequeue_bulk(items + dequeued, reserved - dequeued);
      if (dequeued < reserved) {
        std::this_thread::yield();
      }
    }
  }

  moodycamel::ConcurrentQueue<T> m_queue;
  mutable std::mutex m_state_mutex;
  std::condition_variable_any m_not_empty;
  std::condition_variable_any m_not_full;
  size_t m_size{0};
  size_t m_pending_pushes{0};
  size_t m_capacity;
  bool m_bounded;
  bool m_closed{false};
};
