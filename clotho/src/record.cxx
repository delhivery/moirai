#include <clotho/record.hxx>

using namespace clotho;

inline int64_t
millis_since_epoch()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch())
    .count();
}
