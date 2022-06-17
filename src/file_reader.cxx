#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20
#endif

#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 1
#endif

#include "file_reader.hxx"
#include <Poco/Util/ServerApplication.h>
#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
// #include "date_utils.hxx"
// #include <exception>
// #include <string>
// #include <sys/inotify.h>
// #include <sys/types.h>
// #include <unistd.h>

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
  mLogger.debug("FR: run invoked");
  std::ifstream loadStream;
  std::ofstream clearStream;
  int lastPosition = 0;
  const uint64_t sleepFor = 200;

  try {
    loadStream.open(load_file);
    assert(loadStream.is_open());
    assert(!loadStream.fail());
  } catch (const std::exception& exc) {
    mLogger.error(fmt::format("FR: Error opening file: {}", exc.what()));
  }

  mLogger.debug("FR: Starting file reader");
  while (mRunning) {
    try {
      Poco::Thread::sleep(sleepFor);
      loadStream.seekg(0, std::ios::end);
      auto filesize = loadStream.tellg();

      for (auto current = lastPosition; current < filesize;
           current = loadStream.tellg()) {
        loadStream.seekg(lastPosition, std::ios::beg);
        std::string input;
        std::getline(loadStream, input);
        lastPosition = loadStream.tellg();
        auto payload = nlohmann::json::parse(input);

        if (payload.is_object()) {
          mloadQueuePtr->enqueue(payload.dump());
          mLogger.debug("Enqueued: {}", payload.dump());
        } else {
          mLogger.error("Payload is not an object");
        }
      }
      mLogger.debug("Read file");
    } catch (const std::exception& exc) {
      mLogger.error(fmt::format("FR: Error occurred: {}", exc.what()));
    }
  }
}
