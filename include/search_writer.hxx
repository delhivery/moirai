#ifndef MOIRAI_SEARCH_WRITER
#define MOIRAI_SEARCH_WRITER

#include "consumer.hxx"
#include <Poco/URI.h>
#include <string>

class SearchWriter : public Consumer
{
private:
  // Poco::Logger& mLogger = Poco::Logger::get("search-writer");
  const std::string username;
  const std::string password;
  const std::string mIndex;

  Poco::Net::HTTPSClientSession mSession;

public:
  SearchWriter(const Poco::URI&,
               const std::string&,
               const std::string&,
               const std::string&,
               queue_t*,
               size_t);

  void push(const json_t&, size_t) override;
};

#endif
