#ifndef MOIRAI_PRODUCER
#define MOIRAI_PRODUCER

#include "concurrentqueue.h"
#include "runnable.hxx"
#include <nlohmann/json.hpp>

class Producer : public Runnable
{
  using json_t = nlohmann::json;
  using queue_t = moodycamel::ConcurrentQueue<json_t>;

protected:
  queue_t* mQueuePtr;

  Producer(queue_t*);
};

#endif
