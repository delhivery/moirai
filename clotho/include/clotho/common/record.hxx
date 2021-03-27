#ifndef CLOTHO_RECORD_HXX
#define CLOTHO_RECORD_HXX

#include <chrono>
#include <clotho/common/utils.hxx>
#include <memory>

namespace clotho {
template<class K, class V>
class Record
{
private:
  const K m_key;
  const std::shared_ptr<const V> m_value;
  const int64_t m_event_ts;

public:
  Record(const K& k, const V& v, int64_t ts = millis_since_epoch())
    : m_key(k)
    , m_value(std::make_shared<V>(v))
    , m_event_ts(ts)
  {}

  Record(const K& k,
         std::shared_ptr<const V> v,
         int64_t ts = millis_since_epoch())
    : m_key(k)
    , m_value(v)
    , m_event_ts(ts)
  {}

  Record(const K& k, std::nullptr_t nullp, int64_t ts = millis_since_epoch())
    : m_key(k)
    , m_value(nullptr)
    , m_event_ts(ts)
  {}

  Record(const Record& other)
    : m_key(other.m_key)
    , m_value(other.m_value)
    , m_event_ts(other.m_event_ts)
  {}

  inline bool operator==(const Record<K, V>& other) const
  {
    if (m_event_ts != other.m_event_ts)
      return false;

    if (m_key != other.m_key)
      return false;

    if (m_value.get() == nullptr or other.m_value.get() == nullptr)
      return m_value.get() == nullptr and other.m_value.get() == nullptr;

    return *m_value.get() == *other.m_value.get();
  }

  inline const K& key() const { return m_key; }

  inline const V* value() const { return m_value.get(); }

  inline std::shared_ptr<const V> shared_value() const { return m_value; }

  inline int64_t event_time() const { return m_event_ts; }
};

template<class V>
class Record<void, V>
{
private:
  const std::shared_ptr<const V> m_value;
  const int64_t m_event_ts;

public:
  Record(const V& v, int64_t ts = millis_since_epoch())
    : m_value(std::make_shared<V>(v))
    , m_event_ts(ts)
  {}

  Record(std::shared_ptr<const V> v, int64_t ts = millis_since_epoch())
    : m_value(v)
    , m_event_ts(ts)
  {}

  Record(const Record& other)
    : m_value(other.m_value)
    , m_event_ts(other.m_event_ts)
  {}

  inline bool operator==(const Record<void, V>& other) const
  {
    if (m_event_ts != other.m_event_ts)
      return false;

    if (m_value.get() == nullptr) {
      if (other.m_value.get() == nullptr)
        return true;
      return false;
    }

    return *m_value.get() == *other.m_value.get();
  }

  inline const V* value() const { return m_value.get(); }

  inline std::shared_ptr<const V> shared_value() const { return m_value; }

  inline int64_t event_time() const { return m_event_ts; }
};

template<class K>
class Record<K, void>
{
private:
  const K m_key;
  const int64_t m_event_ts;

public:
  Record(const K& k, const int64_t ts = millis_since_epoch())
    : m_key(k)
    , m_event_ts(ts)
  {}

  Record(const Record& other)
    : m_key(other.m_key)
    , m_event_ts(other.m_event_ts)
  {}

  inline bool operator==(const Record<K, void>& other) const
  {
    return m_event_ts == other.m_event_ts and m_key == other.m_key;
  }

  inline const K& key() const { return m_key; }

  inline int64_t event_time() const { return m_event_ts; }
};
};
#endif
