#include "search_writer.hxx"
#include "utils.hxx"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Thread.h>
#include <Poco/Util/ServerApplication.h>
#include <nlohmann/json.hpp>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

SearchWriter::SearchWriter(
  const Poco::URI& uri,
  const std::string& search_user,
  const std::string& search_pass,
  const std::string& search_index,
  moodycamel::ConcurrentQueue<std::string>* solution_queue)
  : uri(uri)
  , username(search_user)
  , password(search_pass)
  , search_index(search_index)
  , solution_queue(solution_queue)
{}

void
SearchWriter::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  Poco::Net::HTTPSClientSession session(uri.getHost(), uri.getPort());

  while (true) {
    Poco::Thread::sleep(200);
    std::string results;
    if (solution_queue->try_dequeue(results)) {
      nlohmann::json data = nlohmann::json::parse(results);

      app.logger().debug(
        std::format("Sending payload for indexing {}", results));
      try {
        Poco::Net::HTTPRequest request(
          Poco::Net::HTTPRequest::HTTP_POST,
          indexAndTypeToPath(
            search_index, "doc", data["_id"].template get<std::string>()),
          Poco::Net::HTTPMessage::HTTP_1_1);
        data.erase("_id");
        request.setCredentials("Basic",
                               getEncodedCredentials(username, password));
        request.setContentType("application/json");
        std::string stringified = data.dump();
        request.setContentLength((int)stringified.length());
        session.sendRequest(request) << stringified;
        Poco::Net::HTTPResponse response;
        std::istream& response_stream = session.receiveResponse(response);
        std::stringstream response_raw;
        Poco::StreamCopier::copyStream(response_stream, response_raw);

        if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
          app.logger().debug(std::format(
            "Got successful response from ES Host: {}", response_raw.str()));
        } else {
          app.logger().error(std::format("Error uploading data: <{}>: {}",
                                         response.getStatus(),
                                         response_raw.str()));
        }
      } catch (const std::exception& err) {
        app.logger().error(std::format("Error pushing data: {}", err.what()));
      }
    }
  }
}
