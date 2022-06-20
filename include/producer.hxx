#ifndef MOIRAI_PRODUCER
#define MOIRAI_PRODUCER

#include "concurrentqueue.h"
#include "runnable.hxx"
#include <nlohmann/json.hpp>

class Producer : public Runnable
{
protected:
  using json_t = nlohmann::json;
  using queue_t = moodycamel::ConcurrentQueue<json_t>;

  queue_t* mQueuePtr;

  size_t mBatchSize;

  Producer(queue_t*, size_t);

  Producer(const Producer&);

  virtual auto fetch() -> nlohmann::json = 0;

public:
  Producer();

  void run() override;
};

#endif
