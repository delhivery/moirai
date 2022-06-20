#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20
#endif

#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 1
#endif

#include "file_reader.hxx"
#include <fmt/format.h>
// #include "date_utils.hxx"
// #include <exception>
// #include <string>
// #include <sys/inotify.h>
// #include <sys/types.h>
// #include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

FileReader::FileReader(const std::string& dataFile,
                       queue_t* loadQueue,
                       size_t batchSize)
  : Producer(loadQueue, batchSize)
  , nPos(0)
{
  mInFile = std::ifstream(dataFile);
  mLogger = Poco::Logger::get("file-reader");
}

auto
FileReader::fetch() -> nlohmann::json::array
{
  mInFile.seekg(0, std::ios::end);
  auto fileSize = mInFile.tellg();

  if (fileSize < nPos) {
    nPos = 0;
  }

  nlohmann::json::array entries;

  for (size_t lineN = 0; nPos < fileSize and lineN < mBatchSize;
       ++lineN, nPos = mInFile.tellg()) {
    mInFile.seekg(nPos, std::ios::beg);
    nlohmann::json entry;
    infile >> entry;
    entries.push_back(entry);
  }

  return entries;
}
