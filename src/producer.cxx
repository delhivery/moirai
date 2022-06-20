#include "producer.hxx"

Producer::Producer(queue_t* qPtr)
  : mQueuePtr(qPtr)
{
}

Producer::Producer(const Producer& other)
  : mQueuePtr(other.mQueuePtr)
  , mBatchSize(other.mBatchSize)
{
}

void
Producer::run()
{
  stop(false);

  if (mQueuePtr != nullptr) {
    while (not stop()) {
      Poco::Thread::sleep(POLL_INTERVAL);

      try {
        auto records = fetch();
      } catch (const auto& exc) {
        mLogger.error(exc.what());
      }

      for (const auto& record : records) {
        mQueuePtr->enqueue(record);
        mLogger.debug("Producer: enqueued record: {}", record.dump());
      }
    }
  }
}
