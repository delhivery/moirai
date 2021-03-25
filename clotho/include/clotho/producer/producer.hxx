#ifndef CLOTHO_PRODUCER_PRODUCER_HXX
#define CLOTHO_PRODUCER_PRODUCER_HXX

#include <clotho/common/event.hxx>
#include <clotho/common/processor.hxx>
#include <memory>

namespace clotho {

template<class K, class V>
class Producer
{
public:
  Producer();

  virtual ~Producer();

  virtual bool good() const = 0;

  virtual void register_metrics(Processor*) = 0;

  virtual void close() = 0;

  virtual void insert(std::shared_ptr<clotho::Event<K, V>>) = 0;

  virtual size_t queue_size() const = 0;

  virtual void poll() = 0;

  virtual std::string topic() const = 0;
};

}

#endif
