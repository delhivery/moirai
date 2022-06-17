#include "queue.hxx"

static constexpr thread_id_t invalidThreadId = 0;
static constexpr thread_id_t inValidThreadIdAlt = 1;

thread_id_t
thread_id()
{
  static thread_local int x;
  return reinterpret_cast<thread_id_t>(&x);
}


