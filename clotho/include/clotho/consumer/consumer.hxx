#ifndef CLOTHO_CONSUMER_CONSUMER_HXX
#define CLOTHO_CONSUMER_CONSUMER_HXX
#include <readerwriterqueue/readerwriterqueue.h>
#include <string>
#include <vector>

namespace ambasta {
class Consumer
{
protected:
  typedef typename std::shared_ptr<moodycamel::ReaderWriterQueue<std::string>>
    SharedQueue;
  SharedQueue m_queue;

  const uint16_t m_batch_size;
  const uint16_t m_timeout;

  const static uint16_t tickrate;

  std::atomic<bool> m_stop;

public:
  Consumer(SharedQueue, const uint16_t = 300);

  virtual std::vector<std::string> read() = 0;

  void run();
};
}

#endif
