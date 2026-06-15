module;

#include "blocking_queue.hxx"

module moirai.scan_reader;

import std;

ScanReader::ScanReader(BlockingQueue<std::string> *load_queue)
    : m_load_queue(*load_queue) {}
