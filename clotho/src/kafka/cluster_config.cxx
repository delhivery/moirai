#include "clotho/common/record.hxx"
#include <chrono>
#include <clotho/kafka/cluster_config.hxx>
#include <clotho/kafka/utils.hxx>
#include <librdkafka/rdkafkacpp.h>
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

    if (rd_config->set("metadata.broker.list",
                       config->get_brokers(),
                       error_string) != RdKafka::Conf::CONF_OK) {
      throw std::invalid_argument(error_string);
    }

    if (brokers.size() > 0 and brokers[0].scheme == "ssl") {
      if (rd_config->set("security.protocol", "ssl", error_string) !=
          RdKafka::Conf::CONF_OK)
        throw std::invalid_argument(error_string);

      if (rd_config->set("ssl.ca.location",
                         config->get_ca_cert_path(),
                         error_string) != RdKafka::Conf::CONF_OK)
        throw std::invalid_argument(error_string);

      if (!config->get_client_cert_path().empty() and
          !config->get_private_key_path().empty()) {
        // TODO
        // set ssl.certificate.location = get_client_cert_path
        // set ssl.key.location = get_private_key_path
        if (config->get_private_key_passphrase().size())
          // check if we could set passphrase here
          rd_config->set("ssl.key.password",
                         config->get_private_key_passphrase(),
                         error_string);
      }
    }
  } catch (std::invalid_argument& exc) {
  }
}
