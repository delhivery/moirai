#ifndef CLOTHO_EVENT_QUEUE_HXX
#define CLOTHO_EVENT_QUEUE_HXX

#include <clotho/common/event.hxx>
#include <deque>
#include <memory>
namespace clotho {
template<class K, class V>
class EventQueue
{
private:
  std::deque<std::shared_ptr<Event<K, V>>> m_queue;
  int64_t m_next_event_ts;
  // mutable spinlock m_spinlock;
public:
  EventQueue();

  inline size_t size() const;

  inline int64_t next_event_ts() const;

  inline bool empty() const;

  inline void push_back(std::shared_ptr<Event<K, V>>);

  inline void push_front(std::shared_ptr<Event<K, V>>);

  inline std::shared_ptr<Event<K, V>> front();

  inline std::shared_ptr<Event<K, V>> back();

  inline void pop_front();

  inline void pop_back();

  inline std::shared_ptr<Event<K, V>> pop_front_and_get();
};
}
#endif
