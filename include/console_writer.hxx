#ifndef MOIRAI_CONSOLE_WRITER
#define MOIRAI_CONSOLE_WRITER

#include "consumer.hxx"

class ConsoleWriter : public Consumer
{
public:
  ConsoleWriter(queue_t*, size_t);

  void push(const json_t&, size_t) override;
}
#endif
