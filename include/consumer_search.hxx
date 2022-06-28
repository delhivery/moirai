#ifndef MOIRAI_SEARCH_WRITER
#define MOIRAI_SEARCH_WRITER

#include "consumer.hxx"
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <string>

class ESPathWriter : public Consumer {
private:
  const std::string mUser;
  const std::string mPass;
  const std::string mIndex;

  Poco::Net::HTTPSClientSession mSession;

  auto logger() const -> Poco::Logger & override;

  void push(const std::vector<json_t> &) const override;

public:
  ESPathWriter(const Poco::URI &, const std::string &, const std::string &,
               const std::string &, queue_t *, size_t);
};

#endif
