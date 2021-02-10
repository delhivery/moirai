#include "search_writer.hxx"
#include "utils.hxx"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Thread.h>
#include <Poco/Util/ServerApplication.h>

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
      app.logger().debug(
        std::format("Sending payload for indexing {}", results));
      try {
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
                                       indexAndTypeToPath(search_index, "doc"),
                                       Poco::Net::HTTPMessage::HTTP_1_1);
        request.setCredentials("Basic",
                               getEncodedCredentials(username, password));
        request.setContentType("application/json");
        request.setContentLength((int)results.length());
        session.sendRequest(request) << results;
        Poco::Net::HTTPResponse response;
        std::istream& response_stream = session.receiveResponse(response);
        if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
          std::stringstream response;
          Poco::StreamCopier::copyStream(response_stream, response);
          app.logger().debug(std::format(
            "Got successful response from ES Host: {}", response.str()));
        } else {
          std::stringstream response;
          Poco::StreamCopier::copyStream(response_stream, response);
          app.logger().error(
            std::format("Error uploading data: {}", response.str()));
        }
      } catch (const std::exception& err) {
        app.logger().error(std::format("Error pushing data: {}", err.what()));
      }
    }
  }
}
