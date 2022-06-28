#ifndef MOIRAI_CONSOLE_WRITER
#define MOIRAI_CONSOLE_WRITER

#include "consumer.hxx"

class LogPathWriter : public Consumer {
private:
  auto logger() const -> Poco::Logger & override;

  void push(const std::vector<json_t> &) const override;

public:
  LogPathWriter(queue_t *, size_t);
};
#endif
