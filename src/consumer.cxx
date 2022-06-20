#include "consumer.hxx"
#include <execution>

Consumer::Consumer()
  : mQueuePtr(nullptr)
  , mBatchSize(0)
{
}

Consumer::Consumer(const Consumer& other)
  : mQueuePtr(other.mQueuePtr)
  , mBatchSize(other.mBatchSize)
{
}

Consumer::Consumer(queue_t* qPtr, size_t batchSize = 1024)
  : mQueuePtr(qPtr)
  , mBatchSize(batchSize)
{
}

void
Consumer::run()
{
  stop(false);

  if (mQueuePtr != nullptr) {
    while (not stop()) {
      Poco::Thread::sleep(POLL_INTERVAL);
      json_t results[mBatchSize];
      auto nRecords = mQueuePtr->try_dequeue_bulk(mBatchSize, results);
      try {
        push(results, nRecords);
      } catch (const auto& exc) {
        mLogger.error(exc.what());
      }
    }
  }
}
