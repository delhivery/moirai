#include "scan_reader.hxx"
#include "concurrentqueue.h"
#include <string>

ScanReader::ScanReader(moodycamel::ConcurrentQueue<std::string>* load_queue)
  : Poco::Runnable()
  , load_queue(load_queue)
{
  running = true;
}
