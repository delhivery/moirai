#pragma once

#include "scan_reader.hxx"
#include "utils.hxx"
#include <chrono>
#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

class KafkaReader : public ScanReader {
public:
  struct QueueSet {
    BlockingQueue<std::string> *node;
    BlockingQueue<std::string> *edge;
    BlockingQueue<std::string> *load;
  };

private:
  struct ConsumerDeleter {
    void operator()(RdKafka::KafkaConsumer *consumer) const;
  };

  std::string m_broker_url;
  std::unique_ptr<RdKafka::KafkaConsumer, ConsumerDeleter> m_consumer;
  const size_t m_batch_size;
  const std::chrono::milliseconds m_timeout;
  static constexpr std::string_view CONSUMER_GROUP = "CLOTHO";
  std::vector<std::string> m_topics;
  TopicMap m_topic_map;
  std::unordered_map<std::string, std::string> m_properties;
  BlockingQueue<std::string> *m_node_queue;
  BlockingQueue<std::string> *m_edge_queue;

public:
  KafkaReader(const std::string &broker_url, size_t batch_size,
              std::chrono::milliseconds timeout, TopicMap topic_map,
              std::unordered_map<std::string, std::string> properties,
              QueueSet queues);

  ~KafkaReader();

  auto consume_message(std::chrono::milliseconds timeout)
      -> std::unique_ptr<RdKafka::Message>;

  auto dispatch_message(const RdKafka::Message &message,
                        const std::stop_token &stop_token) -> bool;

  void run(std::stop_token stop_token) override;
};
