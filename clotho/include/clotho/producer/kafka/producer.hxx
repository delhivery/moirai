#ifndef CLOTHO_PRODUCER_KAFKA_PRODUCER_HXX
#define CLOTHO_PRODUCER_KAFKA_PRODUCER_HXX
#include "clotho/common/event.hxx"
#include <clotho/kafka/cluster_config.hxx>
#include <clotho/producer/producer.hxx>
#include <memory>

namespace clotho {
namespace kafka {
namespace detail {
class PartitionerCb : public RdKafka::PartitionerCb
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
};

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
  std::unique_ptr<RdKafka::Topic> m_rd_topic;
  std::unique_ptr<RdKafka::Producer> m_rd_producer;
  bool m_closed;
  size_t m_partition_count;
  uint64_t m_message_count;
  uint64_t m_message_bytes;

  detail::HashPartitionCb m_default_partitioner;
  detail::DeliveryReportCb m_delivery_report_callback;
  detail::EventCb m_event_callback;

public:
  Producer(std::shared_ptr<ClusterConfig>, std::string);

  ~Producer();

  void register_metrics(Processor*) override;

  void close() override;

  int produce(uint32_t,
              detail::MemoryManagementMode,
              void*,
              size_t,
              void*,
              size_t,
              int64_t,
              std::shared_ptr<EventCompletedMarker>);

  inline std::string topic() const override;

  void insert(std::shared_ptr<Event<std::string, std::string>>) override;

  inline size_t queue_size() const override;

  void poll() override;

  inline void poll(int);

  inline bool good() const override;

  inline size_t partition_count() const;

  inline int32_t flush(int);
};
}
}
#endif
