#ifndef CLOTHO_EVENT_HXX
#define CLOTHO_EVENT_HXX

#include <cassert>
#include <clotho/common/record.hxx>
#include <functional>
#include <memory>

namespace clotho {
class EventCompletedMarker
{
protected:
  int64_t m_offset;
  int32_t m_error_code;
  std::function<void(int64_t, int32_t)> m_callback;

public:
  EventCompletedMarker(int64_t, std::function<void(int64_t, int32_t)>);

  EventCompletedMarker(std::function<void(int64_t, int32_t)>);

  void init(int64_t);

  virtual ~EventCompletedMarker();

  inline int64_t offset() const;

  inline int32_t error() const;

  inline void fail(int32_t);
};

template<class K, class V>
class Event
{
private:
  std::shared_ptr<const Record<K, V>> m_record;
  std::shared_ptr<EventCompletedMarker> m_completed_marker;
  const int64_t m_partition_hash;

public:
  Event(std::shared_ptr<const Record<K, V>> record,
        std::shared_ptr<EventCompletedMarker> marker = nullptr)
    : m_record(record)
    , m_completed_marker(marker)
    , m_partition_hash(-1)
  {}

  Event(std::shared_ptr<const Record<K, V>> record,
        std::shared_ptr<EventCompletedMarker> marker,
        uint32_t partition_hash)
    : m_record(record)
    , m_completed_marker(marker)
    , m_partition_hash(partition_hash)
  {}

  inline int64_t event_time() const
  {
    return m_record ? m_record->event_time() : -1;
  }

  inline int64_t offset() const
  {
    return m_completed_marker ? m_completed_marker->offset() : -1;
  }

  inline bool has_partition_hash() const { return m_partition_hash >= 0; }

  inline uint32_t partition_hash() const
  {
    assert(m_partition_hash >= 0);
    return static_cast<uint32_t>(m_partition_hash);
  }
};
};
#endif
