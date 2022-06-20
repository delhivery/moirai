#ifndef MOIRAI_CONSUMER
#define MOIRAI_CONSUMER

#include "concurrentqueue.h"
#include "runnable.hxx"
#include <nlohmann/json.hpp>

class Consumer : public Runnable
{
protected:
  using json_t = nlohmann::json;
  using queue_t = moodycamel::ConcurrentQueue<json_t>;

  queue_t* mQueuePtr;

  size_t mBatchSize;

  Consumer(queue_t*, size_t);

  Consumer(const Consumer&);

  virtual void push(const json_t&, size_t) = 0;

public:
  Consumer();

  void run() override;
};
#endif
