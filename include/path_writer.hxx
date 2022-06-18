#ifndef MOIRAI_PATH_WRITER
#define MOIRAI_PATH_WRITER

#include "concurrentqueue.h"
#include <nlohmann/json.hpp>

class PathWriter
{
  using json = nlohmann::json;
  using queue = moodycamel::ConcurrentQueue<json>;

protected:
  queue* mQueuePtr;

public:
  PathWriter(moodycamel::ConcurrentQueue<nlohmann::json>*);
};
#endif
