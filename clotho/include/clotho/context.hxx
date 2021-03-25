#ifndef CLOTHO_CONSUMER_CONTEXT_HXX
#define CLOTHO_CONSUMER_CONTEXT_HXX

#include <clotho/consumer/consumer.hxx>

namespace ambasta {
class ConsumerContext
{
private:
  Consumer* m_consumer;

public:
  ConsumerContext(Consumer* = nullptr);

  ~ConsumerContext();

  void set_consumer(Consumer*);

  void business_logic() const;
};
};
#endif
