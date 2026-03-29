#pragma once

#include "blocking_queue.hxx"
#include <stop_token>
#include <string>

class ScanReader {
protected:
  BlockingQueue<std::string> &m_load_queue;

public:
  explicit ScanReader(BlockingQueue<std::string> *load_queue);

  virtual void run(std::stop_token stop_token) = 0;
};
