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
    std::string results[500];
    if (size_t num_records = solution_queue->try_dequeue_bulk(results, 500);
        num_records > 0) {
      std::vector<nlohmann::json> dataset = {};
      std::for_each(
        results,
        results + num_records,
        [this, &dataset](const std::string& result) {
          auto package = nlohmann::json::parse(result);
          dataset.push_back(nlohmann::json{
            { "index",
              { { "_index", search_index },
                { "_id", package["_id"].template get<std::string>() } } } });
          package.erase("_id");
          dataset.push_back(package);
        });

      try {
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
                                       "/_bulk",
                                       Poco::Net::HTTPMessage::HTTP_1_1);
        request.setCredentials("Basic",
                               getEncodedCredentials(username, password));
        request.setContentType("application/json");
        std::string stringified = std::accumulate(
          dataset.begin(),
          dataset.end(),
          std::string{},
          [](const std::string& acc, const nlohmann::json& row) {
            return acc.empty() ? row.dump()
                               : std::format("{}\n{}", acc, row.dump());
          });
        // stringified += "\n";
        request.setContentLength((int)stringified.length());
        session.sendRequest(request) << stringified;
        Poco::Net::HTTPResponse response;
        std::istream& response_stream = session.receiveResponse(response);
        std::stringstream response_raw;
        Poco::StreamCopier::copyStream(response_stream, response_raw);

        if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK or
            response.getStatus() == Poco::Net::HTTPResponse::HTTP_CREATED) {
          app.logger().debug(std::format(
            "Got successful response from ES Host: {}", response_raw.str()));
        } else {
          app.logger().error(std::format("Error uploading data: <{}>: {}",
                                         response.getStatus(),
                                         response_raw.str()));
          app.logger().error(std::format("Raw data: {}", stringified));
        }
      } catch (const std::exception& exc) {
        app.logger().error(std::format("Error pushing data: {}", exc.what()));
      }
    }
  }
}
