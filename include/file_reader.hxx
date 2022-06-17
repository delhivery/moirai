#ifndef MOIRAI_FILE_READER
#define MOIRAI_FILE_READER

#include "scan_reader.hxx"
#include <Poco/Logger.h>
#include <filesystem>

class FileReader : public ScanReader
{
private:
  Poco::Logger& mLogger = Poco::Logger::get("file-reader");
  std::filesystem::path load_file;

public:
  FileReader(const std::string&, moodycamel::ConcurrentQueue<std::string>*);

  void run() override;
};
#endif
