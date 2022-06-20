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
#include <execution>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

SearchWriter::SearchWriter(const Poco::URI& uri,
                           const std::string& user,
                           const std::string& pass,
                           const std::string& index,
                           queue_t* qPtr,
                           size_t batchSize)
  : Consumer(qPtr, batchSize)
  , mUser(user)
  , mPass(pass)
  , mIndex(index)
  , mSession(uri.getHost(), uri.getPort())
{
}

void
SearchWriter::push(const json_t& records, size_t nRecords)
{
  Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
                                 "/_bulk",
                                 Poco::Net::HTTPMessage::HTTP_1_1);
  request.setCredentials("Basic", getEncodedCredentials(mUser, mPass));
  request.setContentType("application/json");
  auto requestOutStream = mSession.sendRequest(request);

  auto entries =
    records | std::views::filter([](const auto& record) {
      return not record["_id"].is_null();
    }) |
    std::views::transform([this](const auto& record) {
      nlohmann::json entry = record;
      entry.erase("_id");
      return nlohmann::json::array(
        { { "index", { { "_index", mIndex }, { "_id", record["_id"] } } },
          entry });
    }) |
    std::views::join;

  for (const auto& entry : entries) {
    requestOutStream << entry.dump() << std::endl;
  }

  Poco::Net::HTTPResponse response;
  auto requestInStream = mSession.receiveResponse(response);

  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK or
      response.getStatus() == Poco::Net::HTTPResponse::HTTP_CREATED) {
    logger().debug("Successfully pushed data to ES");
  } else {
    logger().error("Failed to push data to ES: {}<{}>",
                   response.getStatus(),
                   response.getReason());
  }
}
