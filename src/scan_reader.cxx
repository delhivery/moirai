#include "scan_reader.hxx"
#include "concurrentqueue.h"
#include <string>

ScanReader::ScanReader(moodycamel::ConcurrentQueue<std::string>* loadQueuePtr)
  : mloadQueuePtr(loadQueuePtr)
{
  mRunning = true;
}
