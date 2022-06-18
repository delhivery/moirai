#include "consumer.hxx"
#include <execution>

Consumer::Consumer()
  : mQueuePtr(nullptr)
  , mBatchSize(0)
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

  if (mQueuePtr != nullptr) {
    while (not mStop) {
      Poco::Thread::sleep(POLL_INTERVAL);
      json_t results[mBatchSize];
      auto nRecords = mQueuePtr->try_dequeue_bulk(mBatchSize, results);
      push(results, nRecords);
    }
  }
}
