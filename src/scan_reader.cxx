#include "scan_reader.hxx"
#include <string>

ScanReader::ScanReader(BlockingQueue<std::string> *load_queue)
    : m_load_queue(*load_queue) {}
