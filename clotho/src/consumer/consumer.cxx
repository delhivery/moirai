#include <chrono>
#include <clotho/consumer/consumer.hxx>
#include <thread>

using namespace ambasta;

Consumer::Consumer(SharedQueue queue, const uint16_t timeout)
  : m_queue(queue)
  , m_batch_size(m_queue->max_capacity())
  , m_timeout(timeout)
  , m_stop(false)
{}

void
Consumer::run()
{
  while (!m_stop) {
    for (auto const& record : read()) {
      while (!m_queue->try_enqueue(record)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(tickrate));
      }
    }
  }
}

const uint16_t Consumer::tickrate = 200;
