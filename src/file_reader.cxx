module;

#include "blocking_queue.hxx"

module moirai.file_reader;

import std;
import moirai.app;
import moirai.json_utils;
import moirai.scan_reader;

namespace {
constexpr auto FILE_POLL_INTERVAL = std::chrono::milliseconds{200};
}

FileReader::FileReader(const std::string &datafile,
                       BlockingQueue<std::string> *load_queue)
    : ScanReader(load_queue), m_load_file(datafile) {}

void FileReader::run(std::stop_token stop_token) {
  auto &app = moirai::Application::instance();
  std::ifstream load_file_stream(m_load_file);
  std::streampos last_position = 0;

  if (!load_file_stream.is_open() || load_file_stream.fail()) {
    app.logger().error("FR: Error opening file: {}", m_load_file.string());
    return;
  }

  while (!stop_token.stop_requested()) {
    try {
      load_file_stream.seekg(0, std::ios::end);
      auto filesize = load_file_stream.tellg();

      for (auto current = last_position; current < filesize;
           current = load_file_stream.tellg()) {
        load_file_stream.seekg(last_position, std::ios::beg);
        std::string input;
        if (!std::getline(load_file_stream, input)) {
          load_file_stream.clear();
          break;
        }
        last_position = load_file_stream.tellg();
        if (last_position == std::streampos{-1}) {
          load_file_stream.clear();
          load_file_stream.seekg(0, std::ios::end);
          last_position = load_file_stream.tellg();
        }
        const auto payload = moirai::parse_json(input);
        if (!payload.has_value()) {
          app.logger().error("Payload is not valid JSON");
          continue;
        }

        if (payload->is_object()) {
          if (!m_load_queue.wait_enqueue(input, stop_token)) {
            return;
          }
        } else {
          app.logger().error("Payload is not an object");
        }
      }
    } catch (const std::exception &exc) {
      app.logger().error("FR: Error occurred: {}", exc.what());
    }

    moirai::wait_for(stop_token, FILE_POLL_INTERVAL);
  }
}
