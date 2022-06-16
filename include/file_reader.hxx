#ifndef MOIRAI_FILE_READER
#define MOIRAI_FILE_READER

#include "scan_reader.hxx"
#include <filesystem>

class FileReader : public ScanReader
{
private:
  std::filesystem::path load_file;

public:
  FileReader(const std::string&, moodycamel::ConcurrentQueue<std::string>*);

  void run() override;
};
#endif
