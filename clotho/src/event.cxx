#include <clotho/event.hxx>

using namespace clotho;

EventCompletedMarker::EventCompletedMarker(
  int64_t offset,
  std::function<void(int64_t, int32_t)> callback)
  : m_offset(offset)
  , m_error_code(0)
  , m_callback(callback)
{}

EventCompletedMarker::EventCompletedMarker(
  std::function<void(int64_t, int32_t)> callback)
  : m_offset(-1)
  , m_error_code(0)
  , m_callback(callback)
{}

void
EventCompletedMarker::init(int64_t offset)
{
  m_offset = offset;
}

EventCompletedMarker::~EventCompletedMarker()
{
  if (m_callback)
    m_callback(m_offset, m_error_code);
}

inline int64_t
EventCompletedMarker::offset() const
{
  return m_offset;
}

inline int32_t
EventCompletedMarker::error() const
{
  return m_error_code;
}

inline void
EventCompletedMarker::fail(int32_t error_code)
{
  if (error_code)
    m_error_code = error_code;
}
