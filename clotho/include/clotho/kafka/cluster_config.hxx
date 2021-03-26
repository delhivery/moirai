#ifndef CLOTHO_KAFKA_CLUSTER_CONFIG_HXX
#define CLOTHO_KAFKA_CLUSTER_CONFIG_HXX

#include <chrono>
#include <filesystem>
#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace clotho {
namespace kafka {
namespace detail {
enum Flags
{
  NONE = 0x00,
  KAFKA = 0x01,
  SCHEMA_REGISTRY = 0x02,
  PUSHGATEWAY = 0x04,
  BB_STREAMING = 0x08
};

struct TopicData
{
  uint32_t m_partition_count;
  std::vector<int32_t> m_available_partitions;

  inline bool available() const;
};
}
class ClusterMetadata;

class ClusterConfig
{
private:
  uint64_t m_flags;
  const std::string m_consumer_group;
  std::vector<std::string> m_brokers;
  std::filesystem::path m_ca_cert_path;
  std::filesystem::path m_client_cert_path;
  std::filesystem::path m_private_key_path;
  std::string m_private_key_passphrase;
  std::chrono::milliseconds m_min_topology_buffering;
  std::chrono::milliseconds m_producer_buffering;
  std::chrono::milliseconds m_producer_message_timeout;
  std::chrono::milliseconds m_consumer_buffering;
  std::chrono::milliseconds m_schema_registry_timeout;
  size_t m_max_pending_sink_messages;
  std::string m_root_path;
  std::string m_schema_registry_uri;
  std::string m_push_gateway_uri;

  bool m_fail_fast;
  mutable std::shared_ptr<ClusterMetadata> m_metadata;
  mutable std::shared_ptr<std::string> m_data_serialized;

public:
  ClusterConfig(std::string, uint64_t);

  inline bool has_feature(detail::Flags) const;

  void load_config_from_env();

  void set_brokers(std::string);

  std::vector<std::string> get_brokers() const;

  std::string get_consumer_group() const;

  void set_consumer_buffering_time(std::chrono::milliseconds);

  std::chrono::milliseconds get_consumer_buffering_time() const;

  void set_producer_buffering_time(std::chrono::milliseconds);

  std::chrono::milliseconds set_consumer_buffering_time() const;

  void set_producer_message_timeout(std::chrono::milliseconds);

  std::chrono::milliseconds get_producer_message_timeout() const;

  void set_min_topology_buffering(std::chrono::milliseconds);

  std::chrono::milliseconds get_min_topology_buffering() const;

  void set_max_pending_sink_messages(size_t);

  size_t get_max_pending_sink_messages() const;

  bool set_ca_cert_path(std::filesystem::path);

  std::filesystem::path get_ca_cert_path() const;

  bool set_private_key_path(std::filesystem::path,
                            std::filesystem::path,
                            std::string = "");

  std::filesystem::path get_client_cert_path() const;

  std::filesystem::path get_private_key_path() const;

  std::string get_private_key_passphrase() const;

  void set_schema_registry_uri(std::string);

  std::string get_schema_registry_uri() const;

  void set_push_gateway_uri(std::string);

  std::string get_push_gateway_uri() const;

  void set_schema_registry_timeout(std::chrono::milliseconds);

  std::chrono::milliseconds get_schema_registry_timeout() const;

  void set_storage_root(std::string);

  std::string get_storage_root() const;

  void set_fail_fast(bool);

  bool get_fail_fast() const;

  std::shared_ptr<ClusterMetadata> get_cluster_metadata() const;

  void set_cluster_state_timeout(std::chrono::seconds);

  std::chrono::seconds get_cluster_state_timeout() const;

  std::shared_ptr<std::string> serialized_data(bool = false);

  void validate();

  void log() const;
};

class ClusterMetadata
{
private:
  mutable std::mutex m_mutex;
  std::unique_ptr<RdKafka::Producer> m_rd_handler;
  mutable std::set<std::string> m_available_consumer_groups;
  mutable std::set<std::string> m_missing_consumer_groups;
  mutable std::unordered_map<std::string, detail::TopicData> m_topic_data;

public:
  ClusterMetadata(const ClusterConfig*);

  ~ClusterMetadata();

  void close();

  void validate();

  uint32_t get_partition_count(std::string);

  bool consumer_group_exists(std::string, std::chrono::seconds) const;

  bool wait_for_topic_partition(std::string,
                                int32_t,
                                std::chrono::seconds) const;

  bool wait_for_topic_leaders(std::string, std::chrono::seconds) const;
};
}
}
#endif
