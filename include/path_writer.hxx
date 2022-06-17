#ifndef MOIRAI_PATH_WRITER
#define MOIRAI_PATH_WRITER

#include "concurrentqueue.h"
#include <nlohmann/json.hpp>

class PathWriter
{
protected:
  moodycamel::ConcurrentQueue<nlohmann::json>* mQueuePtr;

public:
  PathWriter(moodycamel::ConcurrentQueue<nlohmann::json>*);
};
#endif
