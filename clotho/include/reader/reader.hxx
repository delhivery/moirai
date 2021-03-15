#ifndef READER_HXX
#define READER_HXX
#include "utils/component.hxx"
#include <readerwriterqueue/readerwriterqueue.h>

namespace ambasta {
class Reader : public utils::Component
{
private:
  moodycamel::ReaderWriterQueue<std::string> m_queue;

public:
  Reader(std::shared_ptr<CLI::App>);

  // should have edge reader
  // should have optional edge stream reader
  virtual std::vector<std::string> fetch_edges() = 0;

  int main(std::stop_token);
};
}
#endif
