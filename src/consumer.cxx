#include "consumer.hxx"
#include <exception>
#include <execution>
#include <thread>

Consumer::Consumer() : mQueuePtr(nullptr), mBatchSize(0) {}

Consumer::Consumer(const Consumer &other)
    : mQueuePtr(other.mQueuePtr), mBatchSize(other.mBatchSize) {}

Consumer::Consumer(queue_t *qPtr, size_t batchSize = 1024)
    : mQueuePtr(qPtr), mBatchSize(batchSize) {}

auto Consumer::run() -> int {
  stop(false);

  if (mQueuePtr != nullptr) {
    while (not stop()) {
      std::this_thread::sleep_for(POLL_INTERVAL);
      container_t results;
      results.reserve(mBatchSize);
      mQueuePtr->try_dequeue_bulk(results.begin(), mBatchSize);
      try {
        push(results);
      } catch (const std::exception &exc) {
        logger()->error(exc.what());
      }
    }
  }

  return 0;
}
