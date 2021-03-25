#include <librdkafka/rdkafkacpp.h>

namespace clotho {
namespace kafka {
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
}
}
