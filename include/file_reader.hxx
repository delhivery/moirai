#ifndef MOIRAI_FILE_READER
#define MOIRAI_FILE_READER

#include "concurrentqueue.h"
#include "utils.hxx"
#include <Poco/Runnable.h>
#include <filesystem>

class FileReader : public Poco::Runnable
{
private:
  std::filesystem::path load_file;
  moodycamel::ConcurrentQueue<std::string>* load_queue;

public:
  std::atomic<bool> running;

  FileReader(const std::string&, moodycamel::ConcurrentQueue<std::string>*);

  virtual void run();
};
#endif
