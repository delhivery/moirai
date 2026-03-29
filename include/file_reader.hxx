#pragma once

#include "scan_reader.hxx"
#include <filesystem>

class FileReader : public ScanReader {
private:
  std::filesystem::path m_load_file;

public:
  FileReader(const std::string &datafile,
             BlockingQueue<std::string> *load_queue);

  void run(std::stop_token /*unused*/) override;
};
