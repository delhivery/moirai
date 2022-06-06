#include "file_reader.hxx"
#include "date_utils.hxx"
#include <Poco/Util/ServerApplication.h>
#include <exception>
#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

FileReader::FileReader(const std::string& datafile,
                       moodycamel::ConcurrentQueue<std::string>* load_queue)
  : ScanReader(load_queue)
  , load_file(datafile)
{
}

void
FileReader::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  std::ifstream load_file_stream;
  std::ofstream clear_file_stream;
  int last_position = 0;

  try {
    load_file_stream.open(load_file);
    assert(load_file_stream.is_open());
    assert(!load_file_stream.fail());
  } catch (const std::exception& exc) {
    app.logger().error(fmt::format("FR: Error opening file: {}", exc.what()));
  }
  while (running) {
    try {
      Poco::Thread::sleep(200);
      load_file_stream.seekg(0, std::ios::end);
      auto filesize = load_file_stream.tellg();

      for (auto current = last_position; current < filesize;
           current = load_file_stream.tellg()) {
        load_file_stream.seekg(last_position, std::ios::beg);
        std::string input;
        std::getline(load_file_stream, input);
        last_position = load_file_stream.tellg();
        auto payload = nlohmann::json::parse(input);

        if (payload.is_object())
          load_queue->enqueue(payload.dump());
        else
          app.logger().error("Payload is not an object");
      }
    } catch (const std::exception& exc) {
      app.logger().error(fmt::format("FR: Error occurred: {}", exc.what()));
    }
  }
}
