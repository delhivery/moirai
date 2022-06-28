#ifndef MOIRAI_FILE_READER
#define MOIRAI_FILE_READER

#include "producer.hxx"
#include <fstream>
#include <string_view>

class FileLoadProducer : public Producer {
private:
  std::ifstream mInFile;
  size_t mReadPos;

  auto fetch() -> std::vector<json_t> override;

  auto logger() const -> Poco::Logger & override;

public:
  FileLoadProducer(std::string_view, queue_t *, size_t);
};
#endif
