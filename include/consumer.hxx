#ifndef MOIRAI_CONSUMER
#define MOIRAI_CONSUMER

#include "concepts.hxx"
#include "runnable.hxx"
#include <concurrentqueue/concurrentqueue.h>
#include <nlohmann/json.hpp>

class Consumer : public Runnable {
protected:
  using json_t = nlohmann::json;
  using container_t = std::vector<json_t>;
  using queue_t = moodycamel::ConcurrentQueue<json_t>;

  queue_t *mQueuePtr;

  size_t mBatchSize;

  Consumer(queue_t *, size_t);

  Consumer(const Consumer &);

  virtual void push(const container_t &) const = 0;

public:
  Consumer();

  auto run() -> int override;
};

#endif
