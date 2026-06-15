module;

#include "blocking_queue_fwd.hxx"

export module moirai.scan_reader;

export import std;

export class ScanReader {
protected:
  BlockingQueue<std::string>& m_load_queue;

public:
  explicit ScanReader(BlockingQueue<std::string>* load_queue);

  virtual ~ScanReader();

  virtual void run(std::stop_token stop_token) = 0;
};
