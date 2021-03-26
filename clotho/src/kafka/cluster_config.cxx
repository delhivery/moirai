#include "clotho/common/record.hxx"
#include <algorithm>
#include <chrono>
#include <clotho/kafka/cluster_config.hxx>
#include <clotho/kafka/utils.hxx>
#include <experimental/net>
#include <librdkafka/rdkafkacpp.h>
#include <numeric>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace clotho;

kafka::ClusterMetadata::ClusterMetadata(const ClusterConfig* config)
{
  std::string error_string;
  std::unique_ptr<RdKafka::Conf> rd_config(
    RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

  try {
    auto brokers = config->get_brokers();

    set_config(rd_config.get(),
               "metadata.broker.list",
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
      set_config(rd_config.get(), "security.protocol", "ssl");
      set_config(
        rd_config.get(), "ssl.ca.location", config->get_ca_cert_path());

      if (std::filesystem::exists(config->get_client_cert_path()) and
          std::filesystem::exists(config->get_private_key_path())) {
        set_config(rd_config.get(),
                   "ssl.certificate.location",
                   config->get_client_cert_path());
        set_config(
          rd_config.get(), "ssl.key.location", config->get_private_key_path());

        if (not config->get_private_key_passphrase().empty())
          set_config(rd_config.get(),
                     "ssl.key.password",
                     config->get_private_key_passphrase());
      }
    }
  } catch (std::invalid_argument& exc) {
  }
}

std::vector<std::string>
kafka::ClusterConfig::get_brokers() const
{
  return m_brokers;
}
