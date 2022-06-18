#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20
#endif

#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 1
#endif

#include "search_writer.hxx"
#include "utils.hxx"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <execution>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

SearchWriter::SearchWriter(const Poco::URI& uri,
                           const std::string& search_user,
                           const std::string& search_pass,
                           const std::string& search_index,
                           queue_t* qPtr,
                           size_t batchSize)
  : uri(uri)
  , username(search_user)
  , password(search_pass)
  , mIndex(search_index)
  , solution_queue(solution_queue)
  , running(true)
  , Consumer(qPtr, batchSize)
{
  mSession = Poco::Net::HTTPSClientSession(uri.getHost(), uri.getPort();
}

void
SearchWriter::push(const json_t& records, size_t nRecords)
{
  auto entries =
    records | std::views::filter([](const auto& record) {
      return not record["_id"].is_null();
    }) |
    std::views::transform([](const auto& record) {
      nlohmann::json entry = record;
      entry.erase("_id");
      return nlohmann::json::array{
        { "index", { { "_index", mIndex }, { "_id", record["_id"] } } }
      };
    }) |
    std::views::join;

  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
                                 "/_bulk",
                                 Poco::Net::HTTPMessage::HTTP_1_1);
}

void
SearchWriter::run()
{
  while (running or solution_queue->size_approx() > 0) {
    std::string results[1024];
    if (size_t num_records = solution_queue->try_dequeue_bulk(results, 1024);
        num_records > 0) {

      std::string stringified =
        std::accumulate(dataset.begin(),
                        dataset.end(),
                        std::string{},
                        [](const std::string& acc, const nlohmann::json& row) {
                          return acc.empty()
                                   ? row.dump()
                                   : fmt::format("{}\n{}", acc, row.dump());
                        });
      stringified += "\n";
      mLogger.information(stringified);
      try {
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
                                       "/_bulk",
                                       Poco::Net::HTTPMessage::HTTP_1_1);
        request.setCredentials("Basic",
                               getEncodedCredentials(username, password));
        request.setContentType("application/json");

        request.setContentLength((int)stringified.length());
        session.sendRequest(request) << stringified;
        Poco::Net::HTTPResponse response;
        std::istream& response_stream = session.receiveResponse(response);
        std::stringstream response_raw;
        Poco::StreamCopier::copyStream(response_stream, response_raw);

        if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK or
            response.getStatus() == Poco::Net::HTTPResponse::HTTP_CREATED) {
          mLogger.debug(fmt::format("Got successful response from ES Host: {}",
                                    response_raw.str()));
          mLogger.debug(fmt::format("Pushed {} records", num_records));
        } else {
          mLogger.error(fmt::format("Error uploading data: <{}>: {}",
                                    response.getStatus(),
                                    response_raw.str()));
          mLogger.error(fmt::format("Raw data: {}", stringified));
        }
      } catch (const std::exception& exc) {
        mLogger.error(fmt::format(
          "Error pushing data: {}. Data: {}", exc.what(), stringified));
      }
    }
  }
}
