module;

#include "blocking_queue_fwd.hxx"

export module moirai.file_reader;

export import std;
export import moirai.scan_reader;

export class FileReader : public ScanReader {
private:
  std::filesystem::path m_load_file;

public:
  FileReader(const std::string& datafile, BlockingQueue<std::string>* load_queue);
  ~FileReader() override;

  void run(std::stop_token stop_token) override;
};
