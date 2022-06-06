#ifndef MOIRAI_SCAN_READER
#define MOIRAI_SCAN_READER

#include "concurrentqueue.h"
#include "utils.hxx"
#include <Poco/Runnable.h>
#include <atomic>
#include <filesystem>
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <vector>

class ScanReader : public Poco::Runnable
{
protected:
  moodycamel::ConcurrentQueue<std::string>* load_queue;

public:
  std::atomic<bool> running;

  ScanReader(moodycamel::ConcurrentQueue<std::string>*);

  virtual void run() = 0;
};

#endif
