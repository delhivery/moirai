#include "producer_file.hxx"

FileLoadProducer::FileLoadProducer(std::string_view dataFile,
                                   queue_t *loadQueue, size_t batchSize)
    : Producer(loadQueue, batchSize), mReadPos(0) {
  mInFile = std::ifstream(dataFile.data());
}

auto FileLoadProducer::logger() const -> Poco::Logger & {
  return Poco::Logger::get("load-producer.file");
}

auto FileLoadProducer::fetch() -> std::vector<nlohmann::json> {
  mInFile.seekg(0, std::ios::end);
  auto fileSize = mInFile.tellg();

  if (fileSize < mReadPos) {
    mReadPos = 0;
  }

  std::vector<json_t> entries;
  entries.reserve(mBatchSize);

  for (size_t readLines = 0; mReadPos < fileSize and readLines < mBatchSize;
       ++readLines, mReadPos = mInFile.tellg()) {
    mInFile.seekg(mReadPos, std::ios::beg);
    nlohmann::json entry;
    mInFile >> entry;
    entries.push_back(entry);
  }

  return entries;
}
