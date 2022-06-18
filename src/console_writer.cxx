#include "console_writer.hxx"
#include <execution>

ConsoleWriter::ConsoleWriter(queue_t* qPtr, size_t batchSize)
  : Consumer(qPtr, batchSize)
{
  mLogger = Poco::Logger::get("console-writer");
}

void
ConsoleWriter::push(const json_t& data, size_t nRecords)
{
  std::for_each(
    std::execution::par, data, data + nRecords, [this](const auto& record) {
      mLogger.information(record.dump());
    });
}
