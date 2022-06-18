#ifndef MOIRAI_SCAN_READER
#define MOIRAI_SCAN_READER

#include "concurrentqueue.h"
#include "runnable.hxx"
#include <string>
// #include "utils.hxx"
// #include <filesystem>
// #include <librdkafka/rdkafkacpp.h>
// #include <vector>

class ScanReader : public Runnable
{
protected:
  moodycamel::ConcurrentQueue<std::string>* mQueuePtr;

public:
  ScanReader(moodycamel::ConcurrentQueue<std::string>*);
};

#endif
