#ifndef MOIRAI_SEARCH_WRITER
#define MOIRAI_SEARCH_WRITER

#include "concurrentqueue.h"
#include <Poco/Runnable.h>
#include <Poco/URI.h>
#include <string>

class SearchWriter : public Poco::Runnable
{
private:
  Poco::URI uri;
  const std::string username;
  const std::string password;
  const std::string search_index;
  moodycamel::ConcurrentQueue<std::string>* solution_queue;

public:
  SearchWriter(const Poco::URI&,
               const std::string&,
               const std::string&,
               const std::string&,
               moodycamel::ConcurrentQueue<std::string>*);

  virtual void run();
};

#endif
