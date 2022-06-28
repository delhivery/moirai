#ifndef MOIRAI_PRODUCER
#define MOIRAI_PRODUCER

#include "concepts.hxx"
#include "runnable.hxx"
#include <concurrentqueue/concurrentqueue.h>
#include <nlohmann/json.hpp>
#include <vector>

template <typename T>
concept producer = requires(T t) {
  typename T::json_t;

  { t.fetch() } -> sized_range_of<typename T::value_t>;
};

template <class ProducerT> class Producer : public Runnable {
protected:
  using json_t = nlohmann::json;
  using jptr_t = json_t::json_pointer;
  using queue_t = moodycamel::ConcurrentQueue<json_t>;

  queue_t *mQueuePtr;

  size_t mBatchSize;

  Producer(queue_t *qPtr, size_t batchSize)
      : mQueuePtr(qPtr), mBatchSize(batchSize) {}

  Producer(const Producer &other)
      : mQueuePtr(other.mQueuePtr), mBatchSize(other.mBatchSize) {}

public:
  Producer();

  auto run() -> int requires producer<ProducerT> override {
    ProducerT &prod = static_cast<ProducerT &>(*this);

    stop(false);

    if (mQueuePtr != nullptr) {
      while (not stop()) {
        std::this_thread::sleep_for(POLL_INTERVAL);

        try {
          auto records = prod.fetch();
          mQueuePtr->enqueue_bulk(records.begin(), records.size());
        } catch (const std::exception &exc) {
          logger()->error(exc.what());
        }
      }
    }
    return 0;
  }
};

#endif
