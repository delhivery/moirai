#ifndef MOIRAI_FILE_READER
#define MOIRAI_FILE_READER

#include "producer.hxx"
#include <filesystem>

class FileReader : public Producer
{
private:
  // Poco::Logger& mLogger = Poco::Logger::get("file-reader");
  std::ifstream mInFile;
  size_t nPos;

  auto fetch() -> nlohmann::json::array override;

public:
  FileReader(const std::string&, queue_t*, size_t);

  void run() override;
};
#endif
