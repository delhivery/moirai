#include "scan_reader.hxx"

ScanReader::ScanReader(moodycamel::ConcurrentQueue<std::string>* loadQueuePtr)
  : mloadQueuePtr(loadQueuePtr)
{
  mRunning = true;
}
