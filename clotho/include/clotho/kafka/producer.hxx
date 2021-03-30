#ifndef CLOTHO_PRODUCER_KAFKA_PRODUCER_HXX
#define CLOTHO_PRODUCER_KAFKA_PRODUCER_HXX
#include "clotho/common/event.hxx"
#include <clotho/kafka/cluster_config.hxx>
#include <clotho/producer/producer.hxx>
#include <cppkafka/cppkafka.h>
#include <cppkafka/topic_partition_list.h>
#include <memory>

namespace clotho {
namespace kafka {
namespace detail {

/*class PartitionerCb : public RdKafka::PartitionerCb
{
public:
  int32_t partitioner_cb(const RdKafka::Topic*,
                         std::string_view,
                         int32_t,
                         void*);
};

class DeliveryReportCb : public RdKafka::DeliveryReportCb
{
private:
  RdKafka::ErrorCode m_error_code;

public:
  DeliveryReportCb();

  virtual void delivery_report_cb(RdKafka::Message&);

  inline RdKafka::ErrorCode status() const;
};

class EventCb : public RdKafka::EventCb
{
public:
  void event_cb(RdKafka::Event&);
};*/

enum MemoryManagementMode
{
  FREE = 1,
  COPY = 2,
};
}

class Producer : public ::clotho::Producer<std::string, std::string>
{
private:
  const std::string m_topic;
  std::unique_ptr<cppkafka::Producer> m_rd_producer;
  bool m_closed;
  size_t m_partition_count;
  uint64_t m_message_count;
  uint64_t m_message_bytes;
  cppkafka::Error m_error;

  std::function<void(cppkafka::TopicPartitionList&)> m_default_partitioner;

public:
  Producer(std::shared_ptr<ClusterConfig>, std::string);

  ~Producer();

  void register_metrics(Processor*) override;

  void close() override;

  int produce(uint32_t, const std::string&, const std::string&, int64_t);

  inline std::string topic() const override;

  void insert(std::shared_ptr<Event<std::string, std::string>>) override;

  inline size_t queue_size() const override;

  void poll() override;

  inline void poll(std::chrono::milliseconds);

  inline bool good() const override;

  inline size_t partition_count() const;

  inline int32_t flush(std::chrono::milliseconds);
};
}
}
#endif
