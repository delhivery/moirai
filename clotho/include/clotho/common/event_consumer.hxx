#ifndef CLOTHO_PRODUCER_EVENT_CONSUMER_HXX
#define CLOTHO_PRODUCER_EVENT_CONSUMER_HXX
#include <clotho/common/event.hxx>
#include <clotho/common/event_queue.hxx>
#include <clotho/common/record.hxx>

namespace clotho {
template<class K, class V>
class EventConsumer
{
protected:
  EventQueue<K, V> m_queue;

public:
  typedef K key_type;
  typedef V value_type;

  EventConsumer();

  inline std::string key_type_name() const;

  inline std::string value_type_name() const;

  inline size_t queue_size() const;

  inline int64_t next_event_ts() const;

  inline void push_back(std::shared_ptr<const Record<K, V>>);

  inline void push_back(std::shared_ptr<Event<K, V>>);

  inline void push_back(const K&, const V&, int64_t = millis_since_epoch());

  inline void push_back(uint32_t, std::shared_ptr<const Record<K, V>>);

  inline void push_back(uint32_t, std::shared_ptr<Event<K, V>>);

  inline void push_back(uint32_t,
                        const K&,
                        const V&,
                        int64_t = millis_since_epoch());
};

template<class V>
class EventConsumer<void, V>
{
protected:
  EventQueue<void, V> m_queue;

public:
  typedef void key_type;
  typedef V value_type;

  EventConsumer();

  inline std::string key_type_name() const;

  inline std::string value_type_name() const;

  inline size_t queue_size() const;

  inline int64_t next_event_ts() const;

  inline void push_back(std::shared_ptr<const Record<void, V>>);

  inline void push_back(std::shared_ptr<Event<void, V>>);

  inline void push_back(const V&, int64_t = millis_since_epoch());

  inline void push_back(uint32_t, std::shared_ptr<const Record<void, V>>);

  inline void push_back(uint32_t, std::shared_ptr<Event<void, V>>);

  inline void push_back(uint32_t, const V&, int64_t = millis_since_epoch());
};

template<class K>
class EventConsumer<K, void>
{
protected:
  EventQueue<K, void> m_queue;

public:
  typedef K key_type;
  typedef void value_type;

  EventConsumer();

  inline std::string key_type_name() const;

  inline std::string value_type_name() const;

  inline size_t queue_size() const;

  inline int64_t next_event_ts() const;

  inline void push_back(std::shared_ptr<const Record<K, void>>);

  inline void push_back(std::shared_ptr<Event<K, void>>);

  inline void push_back(const K&, int64_t = millis_since_epoch());

  inline void push_back(uint32_t, std::shared_ptr<const Record<K, void>>);

  inline void push_back(uint32_t, std::shared_ptr<Event<K, void>>);

  inline void push_back(uint32_t, const K&, int64_t = millis_since_epoch());
};

template<class K, class V>
void
insert(EventConsumer<K, V>&, const Record<K, V>&);

template<class K, class V>
void
insert(EventConsumer<K, V>&,
       const K&,
       const V&,
       int64_t = millis_since_epoch());

template<class K, class V>
void
insert(EventConsumer<K, V>&,
       const K&,
       std::shared_ptr<const V>,
       int64_t = millis_since_epoch());

template<class K, class V>
void
insert(EventConsumer<K, V>&,
       const K&,
       std::shared_ptr<V>,
       int64_t = millis_since_epoch());

template<class K, class V>
void
insert(EventConsumer<K, V>&, const K&, int64_t = millis_since_epoch());

template<class K>
void
insert(EventConsumer<K, void>&, const K&, int64_t = millis_since_epoch());

template<class V>
void
insert(EventConsumer<void, V>&, const V&, int64_t = millis_since_epoch());

template<class V>
void
insert(EventConsumer<void, V>&,
       std::shared_ptr<const V>,
       int64_t = millis_since_epoch());
}
#endif
