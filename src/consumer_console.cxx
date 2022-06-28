#include "consumer_console.hxx"
#include <ranges>

LogPathWriter::LogPathWriter(queue_t *qPtr, size_t batchSize)
    : Consumer(qPtr, batchSize) {}

auto LogPathWriter::logger() const -> Poco::Logger & {
  return Poco::Logger::get("path-writer.log");
}

void LogPathWriter::push(const std::vector<json_t> &data) const {
  for (const auto &record : data) {
    logger().information(record.dump());
  }
}
