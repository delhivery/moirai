
#include <cppkafka/configuration.h>
#include <cppkafka/message.h>
#include <cppkafka/utils/buffered_producer.h>

namespace clotho {
namespace kafka {

void
set_config(cppkafka::Configuration*, std::string, std::string);

void
set_config(cppkafka::Configuration*, std::string, cppkafka::Configuration*);
}
