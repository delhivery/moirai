#pragma once

#include <chrono>
#include <filesystem>
#include <string>
namespace moirai {
class ClusterMetadata;
class ClusterConfig
{
private:
  uint64_t m_flags;
  std::string m_brokers;
  std::filesystem::path m_ca_certs_path;
  std::filesystem::path m_client_cert_path;
  std::filesystem::path m_private_key_path;
  std::string m_private_key_passphrase;
  std::chrono::milliseconds m_minimum_topology_buffering;
  std::chrono::milliseconds m_producer_buffering;
  std::chrono::milliseconds m_consumer_buffering;
  std::chrono::milliseconds m_schema_registry_timeout;
  std::chrono::seconds m_cluster_state_timeout;
  std::size_t m_max_pending_sink_messages;
  std::string m_root_path;
  std::string m_schema_registry_uri;
  std::string m_pushgateway_uri;

  bool m_fail_fast;
  mutable std::shared_ptr<ClusterMetadata> m_metadata;

public:
  enum flags_t : uint8_t
  {
    NONE = 0b0000,
    KAFKA = 0b0001,
    SCHEMA_REGISTRY = 0b0010,
    PUSH_GATEWAY = 0b0100,
    STREAMING = 0b1000,
  };

  ClusterConfig(std::string = "",
                uint8_t = KAFKA | SCHEMA_REGISTRY | PUSH_GATEWAY);

  inline auto has_feature(flags_t) const -> bool;

  void load_config_from_env();

  void set_brokers(std::string);
  auto get_brokers() const -> std::string;

  auto get_consumer_group() const -> std::string;

  void set_consumer_buffering_time(std::chrono::milliseconds);
  auto get_consumer_buffering_time() const -> std::chrono::milliseconds;

  void set_producer_buffering_time(std::chrono::milliseconds);
  auto get_producer_buffering_time() const -> std::chrono::milliseconds;

  void set_producer_message_timeout(std::chrono::milliseconds);
  auto get_producer_message_timeout() const -> std::chrono::milliseconds;

  void set_minimum_topology_buffering(std::chrono::milliseconds);
  auto get_minimum_topology_buffering() const -> std::chrono::milliseconds;

  void set_max_pending_sink_messages(std::size_t);
  auto get_max_pending_sink_messages() const -> std::size_t;

  auto set_ca_cert_path(std::filesystem::path) -> bool;
  auto get_ca_cert_path() const -> std::filesystem::path;

  auto set_private_key_path(std::filesystem::path,
                            std::filesystem::path,
                            std::string = "") -> bool;
  auto get_client_cert_path() const -> std::filesystem::path;
  auto get_private_key_path() const -> std::filesystem::path;
  auto get_private_key_passphrase() const -> std::string;

  void set_schema_registry_uri(std::string);
  auto get_schema_registry_uri() const -> std::string;

  void set_pushgateway_uri(std::string);
  auto get_pushgateway_uri(std::string) const -> std::string;

  void set_schema_registry_timeout(std::chrono::milliseconds);
  auto get_schema_registry_timeout() const -> std::chrono::milliseconds;

  void set_storage_root(std::string);
  auto get_storage_root() const -> std::string;

  void set_fail_fast(bool);
  auto get_fail_fast() const -> bool;

  auto get_cluster_metadata() const -> std::shared_ptr<ClusterMetadata>;

  void set_cluster_state_timeout(std::chrono::seconds);
  auto get_cluster_state_timeout() const -> std::chrono::seconds;

  void validate();

  void log() const;
};
}
