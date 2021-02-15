#ifndef MOIRAI_FILE_READER
#define MOIRAI_FILE_READER

#include "concurrentqueue.h"
#include "utils.hxx"
#include <Poco/Runnable.h>
#include <filesystem>

class FileReader : public Poco::Runnable
{
private:
  std::atomic<bool> running;
  std::filesystem::path load_file;
  moodycamel::ConcurrentQueue<std::string>* load_queue;

public:
  FileReader(const std::string&, moodycamel::ConcurrentQueue<std::string>*);

  ~FileReader();

  std::vector<std::string> consume_batch(size_t, int);

  virtual void run();
};
#endif
