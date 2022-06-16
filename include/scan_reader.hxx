#ifndef MOIRAI_SCAN_READER
#define MOIRAI_SCAN_READER

#include "concurrentqueue.h"
#include <Poco/Runnable.h>
#include <atomic>
#include <string>
// #include "utils.hxx"
// #include <filesystem>
// #include <librdkafka/rdkafkacpp.h>
// #include <vector>

class ScanReader : public Poco::Runnable
{
protected:
  moodycamel::ConcurrentQueue<std::string>* mloadQueuePtr;

public:
  std::atomic<bool> mRunning;

  ScanReader(moodycamel::ConcurrentQueue<std::string>*);

  void run() override = 0;
};

#endif
