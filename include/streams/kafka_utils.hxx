#pragma once
#include "cluster_config.hxx"
#include <librdkafka/rdkafkacpp.h>

namespace moirai {
void
set_config(RdKafka::Conf*, std::string, std::string);
void
set_config(RdKafka::Conf*, std::string, RdKafka::Conf*);
void
set_config(RdKafka::Conf*, std::string, RdKafka::DeliveryReportCb*);
void
set_config(RdKafka::Conf*, std::string, RdKafka::PartitionerCb*);
void
set_config(RdKafka::Conf*, std::string, RdKafka::EventCb*);

void
set_broker_config(RdKafka::Conf*, const ClusterConfig*);
}
