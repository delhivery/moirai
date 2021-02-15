#include "file_reader.hxx"
#include "date_utils.hxx"
#include "format.hxx"
#include <Poco/Util/ServerApplication.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <pstl/glue_execution_defs.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

FileReader::FileReader(const std::string& datafile,
                       moodycamel::ConcurrentQueue<std::string>* load_queue)
  : load_file(datafile)
  , load_queue(load_queue)
{}

void
FileReader::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  std::ifstream load_file_stream;
  std::ofstream clear_file_stream;

  while (running) {
    Poco::Thread::sleep(200);
    load_file_stream.open(load_file);
    assert(load_file_stream.is_open());
    assert(!load_file_stream.fail());

    if (load_file_stream.peek() != std::ifstream::traits_type::eof()) {
      auto payload = nlohmann::json::parse(load_file_stream);

      std::for_each(
        std::execution::par,
        payload.begin(),
        payload.end(),
        [this](auto const& record) { load_queue->enqueue(record); });
    }
    load_file_stream.close();
    clear_file_stream.open(load_file, std::ios::out | std::ios::trunc);
    clear_file_stream.close();
  }
}
