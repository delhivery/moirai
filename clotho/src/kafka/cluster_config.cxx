#include <algorithm>
#include <chrono>
#include <clotho/common/utils.hxx>
#include <clotho/kafka/cluster_config.hxx>
#include <cppkafka/exceptions.h>
#include <cppkafka/metadata.h>
#include <cppkafka/producer.h>
#include <cppkafka/utils/buffered_producer.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <numeric>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace clotho::kafka;

ClusterConfig::ClusterConfig(std::string consumer_group, uint64_t flags)
  : m_flags(flags)
  , m_consumer_group(consumer_group)
  , m_min_topology_buffering(std::chrono::milliseconds(1000))
  , m_producer_buffering(std::chrono::milliseconds(1000))
  , m_producer_message_timeout(std::chrono::milliseconds(0))
  , m_consumer_buffering(std::chrono::milliseconds(1000))
  , m_schema_registry_timeout(std::chrono::milliseconds(10000))
  , m_cluster_state_timeout(60s)
  , m_max_pending_sink_messages(50000)
  , m_fail_fast(true)
{}

void
ClusterConfig::load_config_from_env()
{}

std::vector<std::string>
ClusterConfig::get_brokers() const
{
  return m_brokers;
}

void
ClusterConfig::set_storage_root(std::filesystem::path root_path)
{
  if (not std::filesystem::exists(root_path)) {
    std::filesystem::create_directories(root_path);

    if (not std::filesystem::exists(root_path)) {
      spdlog::critical("Failed to create storage path at {}", root_path);
      exit(0);
    }
  }
  m_root_path = root_path;
}

std::filesystem::path
ClusterConfig::get_storage_root() const
{
  return m_root_path;
}

std::filesystem::path
ClusterConfig::get_ca_cert_path() const
{
  return m_ca_cert_path;
}

bool
ClusterConfig::set_ca_cert_path(std::filesystem::path path)
{
  if (not std::filesystem::exists(path)) {
    spdlog::warn("CA cert not found at {}, ignoring ssl config", path);
    return false;
  }
  m_ca_cert_path = path;
  return true;
}

bool
ClusterConfig::set_private_key_path(std::filesystem::path client_cert_path,
                                    std::filesystem::path private_key_path,
                                    std::string passphrase)
{
  if (not std::filesystem::exists(client_cert_path) or
      not std::filesystem::exists(private_key_path)) {
    spdlog::warn("Private key: {} or Client cert {} missing, ssl client auth "
                 "config incomplete. Ignoring config",
                 private_key_path,
                 client_cert_path);
    return false;
  }
  m_client_cert_path = client_cert_path;
  m_private_key_path = private_key_path;
  m_private_key_passphrase = passphrase;
  return true;
}

std::filesystem::path
ClusterConfig::get_client_cert_path() const
{
  return m_client_cert_path;
}

std::filesystem::path
ClusterConfig::get_private_key_path() const
{
  return m_private_key_path;
}

std::string
ClusterConfig::get_private_key_passphrase() const
{
  return m_private_key_passphrase;
}

void
ClusterConfig::set_schema_registry_uri(std::vector<std::string> uris)
{
  bool valid_uri =
    std::all_of(uris.begin(), uris.end(), [](const std::string& uri) {
      return uri.starts_with("http://") or uri.starts_with("https://");
    });

  if (not valid_uri) {
    spdlog::critical("Bad schema registry uri: {}", uris);
    exit(0);
  }
  m_schema_registry_uri = uris;
}

std::vector<std::string>
ClusterConfig::get_schema_registry_uri() const
{
  return m_schema_registry_uri;
}

void
ClusterConfig::set_push_gateway_uri(std::string uri)
{
  m_push_gateway_uri = uri;
}

std::string
ClusterConfig::get_push_gateway_uri() const
{
  return m_push_gateway_uri;
}

void
ClusterConfig::set_schema_registry_timeout(std::chrono::milliseconds timeout)
{
  m_schema_registry_timeout = timeout;
}

std::chrono::milliseconds
ClusterConfig::get_schema_registry_timeout() const
{
  return m_schema_registry_timeout;
}

void
ClusterConfig::set_consumer_buffering_time(std::chrono::milliseconds timeout)
{
  m_consumer_buffering = timeout;
}

std::chrono::milliseconds
ClusterConfig::get_consumer_buffering_time() const
{
  return m_consumer_buffering;
}

void
ClusterConfig::set_producer_buffering_time(std::chrono::milliseconds timeout)
{
  m_producer_buffering = timeout;
}

std::chrono::milliseconds
ClusterConfig::get_producer_buffering_time() const
{
  return m_producer_buffering;
}

void
ClusterConfig::set_producer_message_timeout(std::chrono::milliseconds timeout)
{
  m_producer_message_timeout = timeout;
}

std::chrono::milliseconds
ClusterConfig::get_producer_message_timeout() const
{
  return m_producer_message_timeout;
}

void
ClusterConfig::set_min_topology_buffering(std::chrono::milliseconds timeout)
{
  m_min_topology_buffering = timeout;
}

std::chrono::milliseconds
ClusterConfig::get_min_topology_buffering() const
{
  return m_min_topology_buffering;
}

void
ClusterConfig::set_max_pending_sink_messages(size_t sz)
{
  m_max_pending_sink_messages = sz;
}

size_t
ClusterConfig::get_max_pending_sink_messages() const
{
  return m_max_pending_sink_messages;
}

void
ClusterConfig::set_fail_fast(bool fail_fast)
{
  m_fail_fast = fail_fast;
}

bool
ClusterConfig::get_fail_fast() const
{
  return m_fail_fast;
}

std::shared_ptr<ClusterMetadata>
ClusterConfig::get_cluster_metadata() const
{
  if (m_metadata == nullptr)
    m_metadata = std::make_shared<ClusterMetadata>(this);
  return m_metadata;
}

void
ClusterConfig::set_cluster_state_timeout(std::chrono::seconds timeout)
{
  m_cluster_state_timeout = timeout;
}

std::chrono::seconds
ClusterConfig::get_cluster_state_timeout() const
{
  return m_cluster_state_timeout;
}

std::shared_ptr<std::string>
ClusterConfig::serialized_data(bool relaxed)
{
  // TODO
  if (!m_data_serialized)
    m_data_serialized =
      std::make_shared<std::string>(get_schema_registry(), relaxed);
  return m_data_serialized;
}

void
ClusterConfig::validate()
{
  if (has_feature(detail::Flags::KAFKA)) {
    if (m_brokers.size() == 0) {
      spdlog::critical("No brokers defined");
      exit(0);
    }

    bool is_ssl =
      std::any_of(m_brokers.begin(), m_brokers.end(), [](std::string& broker) {
        return broker.starts_with("ssl");
      });

    if (is_ssl and not std::filesystem::exists(m_ca_cert_path)) {
      spdlog::critical("Brokers using SSL and no cert configured");
      exit(0);
    }
  }

  if (has_feature(detail::Flags::SCHEMA_REGISTRY)) {
    bool is_https = std::any_of(m_schema_registry_uri.begin(),
                                m_schema_registry_uri.end(),
                                [](const std::string& schema_uri) {
                                  return schema_uri.starts_with("https://");
                                });

    if (is_https and not std::filesystem::exists(m_ca_cert_path)) {
      spdlog::critical("Schema registry uses SSL and no cert configured");
      exit(0);
    }
  }

  if (has_feature(detail::Flags::KAFKA))
    get_cluster_metadata()->validate();

  if (has_feature(detail::Flags::SCHEMA_REGISTRY)) {
    if (m_schema_registry_uri.size()) {
      auto schema_registry = get_schema_registry();

      if (not schema_registry->validate()) {
        spdlog::critical("Schema registry validation failed");
        exit(0);
      }
    }
  }
}

void
ClusterConfig::log() const
{
  if (has_feature(detail::Flags::SCHEMA_REGISTRY))
    spdlog::info("Kafka broker(s): {}", m_brokers);

  spdlog::info("Consumer group: {}", m_consumer_group);

  if (std::filesystem::exists(m_ca_cert_path))
    spdlog::info("CA cert: {}", m_ca_cert_path);

  if (std::filesystem::exists(m_client_cert_path))
    spdlog::info("Client cert: {}", m_client_cert_path);

  if (std::filesystem::exists(m_private_key_path))
    spdlog::info("Private key: {}", m_private_key_path);

  if (m_private_key_passphrase.size() > 0)
    spdlog::info("Private key passphrase: {}", m_private_key_passphrase);

  if (std::filesystem::exists(m_root_path))
    spdlog::info("Storage root: {}", m_root_path);

  spdlog::info("Kafka consumer buffering time: {}ms",
               m_consumer_buffering.count());
  spdlog::info("Kafka producer buffering time: {}ms",
               m_producer_buffering.count());

  if (m_producer_message_timeout.count() == 0)
    spdlog::info("Kafka producer message timeout: disabled");
  else
    spdlog::info("Kafka producer message timeout: {}ms",
                 m_producer_message_timeout.count());

  if (has_feature(detail::Flags::SCHEMA_REGISTRY)) {
    if (m_schema_registry_uri.size() > 0) {
      spdlog::info("Schema registry: {}", m_schema_registry_uri);
      spdlog::info("Schema registry timeout: {}ms",
                   m_schema_registry_timeout.count());
    }
  }

  spdlog::info("Kafka cluter state timeout: {}s",
               m_cluster_state_timeout.count());
}

ClusterMetadata::ClusterMetadata(const ClusterConfig* config)
{
  std::string error_string;
  cppkafka::Configuration rd_config{};

  try {
    auto brokers = config->get_brokers();

    rd_config.set("metadata.broker.list",
                  std::accumulate(brokers.begin(),
                                  brokers.end(),
                                  std::string(),
                                  [](const std::string& acc,
                                     const std::string& broker) -> std::string {
                                    return acc + (acc.length() > 0 ? "," : "") +
                                           broker;
                                  }));

    bool secure = std::any_of(
      brokers.cbegin(), brokers.cend(), [](const std::string& broker) {
        return broker.starts_with("ssl://");
      });

    if (brokers.size() > 0 and secure) {
      rd_config.set("security.protocol", "ssl");
      rd_config.set("ssl.ca.location", config->get_ca_cert_path());

      if (std::filesystem::exists(config->get_client_cert_path()) and
          std::filesystem::exists(config->get_private_key_path())) {
        rd_config.set("ssl.certificate.location",
                      config->get_client_cert_path());
        rd_config.set("ssl.key.location", config->get_private_key_path());

        if (not config->get_private_key_passphrase().empty())
          rd_config.set("ssl.key.password",
                        config->get_private_key_passphrase());
      }
    }
  } catch (cppkafka::ConfigException& exc) {
    spdlog::critical("Bad config: {}", exc.what());
    exit(0);
  }

  try {
    m_rd_producer = std::make_unique<cppkafka::Producer>(rd_config);
  } catch (cppkafka::Exception& exc) {
    spdlog::critical("Failed to create producer: {}", exc.what());
    exit(0);
  }

  bool done = false;
  bool one_reply = false;

  int64_t expires =
    millis_since_epoch() + config->get_cluster_state_timeout().count() * 1000;

  while (not done) {
    if (expires < millis_since_epoch()) {
      if (not one_reply) {
        spdlog::critical("Cannot get broker metadata");
        exit(0);
      }
      return;
    }

    try {
      cppkafka::Metadata rd_metadata = m_rd_producer->get_metadata();
      done = true;
      one_reply = true;

      for (auto&& rd_topic_md : rd_metadata.get_topics()) {
        detail::TopicData topic_data;
        for (auto&& rd_topic_parition_md : rd_topic_md.get_partitions()) {
          if (rd_topic_parition_md.get_leader() >= 0) {
            topic_data.m_available_partitions.push_back(
              rd_topic_parition_md.get_id());
          } else {
            done = false;
          }
        }
        m_topic_data.insert(std::pair<std::string, detail::TopicData>(
          rd_topic_md.get_name(), topic_data));
      }
    } catch (const cppkafka::Exception& exc) {
      done = false;
      spdlog::error("Error fetching metadata: {}", exc.what());
    }

    if (not done) {
      spdlog::info("Waiting for broker metadata to be available - sleeping 1s");
      std::this_thread::sleep_for(1s);
    }
  }
}

ClusterMetadata::~ClusterMetadata()
{
  close();
}

void
ClusterMetadata::validate()
{}

bool
ClusterMetadata::consumer_group_exists(std::string consumer_group,
                                       std::chrono::seconds timeout) const
{
  std::lock_guard<std::mutex> guard(m_mutex);

  if (m_available_consumer_groups.find(consumer_group) !=
      m_available_consumer_groups.end())
    return true;

  if (m_missing_consumer_groups.find(consumer_group) !=
      m_missing_consumer_groups.end())
    return false;

  auto expires = millis_since_epoch() + 1000 * timeout.count();

  while (true) {
    if (expires < millis_since_epoch()) {
      m_missing_consumer_groups.insert(consumer_group);
      return false;
    }

    try {
      m_rd_producer->get_consumer_group(consumer_group);
      m_available_consumer_groups.insert(consumer_group);
      return true;
    } catch (const cppkafka::ElementNotFound& exc) {
      return false;
    } catch (const cppkafka::HandleException& exc) {
      spdlog::error("Retrying group list in 1s: {}", exc.what());
      std::this_thread::sleep_for(1s);
    }
  }
}

bool
ClusterMetadata::wait_for_topic_leaders(std::string topic,
                                        std::chrono::seconds timeout) const
{
  std::lock_guard<std::mutex> guard{ m_mutex };
  auto item = m_topic_data.find(topic);

  if (item != m_topic_data.end())
    return item->second.available();

  auto expires = millis_since_epoch() + 1000 * timeout.count();
  auto rd_topic = m_rd_producer->get_topic(topic);

  int64_t nr_available = 0;

  while (true) {
    if (expires < millis_since_epoch())
      return false;
    auto rd_topic_md = m_rd_producer->get_metadata(rd_topic);

    for (auto&& rd_topic_parition_md : rd_topic_md.get_partitions()) {
      if (rd_topic_parition_md.get_leader() >= 0) {
        nr_available++;
      }
    }

    if (nr_available == rd_topic_md.get_partitions().size() and
        rd_topic_md.get_partitions().size() > 0)
      break;
    spdlog::error("Waiting for all partitions leader to be available for topic "
                  "{}, sleeping for 1s",
                  topic);
    std::this_thread::sleep_for(1s);
  }
  return true;
}

bool
ClusterMetadata::wait_for_topic_partition(std::string topic,
                                          uint32_t partition,
                                          std::chrono::seconds timeout) const
{
  std::lock_guard<std::mutex> guard{ m_mutex };

  auto item = m_topic_data.find(topic);

  if (item != m_topic_data.end())
    return item->second.available();

  auto expires = millis_since_epoch() + 1000 * timeout.count();
  auto rd_topic = m_rd_producer->get_topic(topic);

  while (true) {
    if (expires < millis_since_epoch())
      return false;

    auto rd_topic_md = m_rd_producer->get_metadata(rd_topic);

    for (auto&& rd_topic_parition_md : rd_topic_md.get_partitions()) {
      if (rd_topic_parition_md.get_leader() >= 0 and
          rd_topic_parition_md.get_id() == partition) {
        return true;
      }
    }

    spdlog::error("Waiting for partitions leader to be available for topic {} "
                  "and partition {}, sleeping for 1s",
                  topic,
                  partition);
    std::this_thread::sleep_for(1s);
  }
  return true;
}

uint32_t
ClusterMetadata::get_partition_count(std::string topic)
{
  std::lock_guard<std::mutex> guard{ m_mutex };

  auto item = m_topic_data.find(topic);
  if (item != m_topic_data.end())
    return item->second.m_partition_count;

  while (true) {
    try {
      auto rd_topic = m_rd_producer->get_topic(topic);
      auto rd_topic_md = m_rd_producer->get_metadata(rd_topic);

      if (rd_topic_md.get_partitions().size() > 0)
        return rd_topic_md.get_partitions().size();
    } catch (const cppkafka::Exception& exc) {
      spdlog::info("Error getting partitions for topic: {}, sleeping for 1s",
                   topic);
    }
    std::this_thread::sleep_for(1s);
  }
}
