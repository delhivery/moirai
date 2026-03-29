#include "search_writer.hxx"
#include "app.hxx"
#include "http.hxx"
#include "json_utils.hxx"
#include "utils.hxx"
#include <array>
#include <format>
#include <utility>
#include <vector>

using std::stop_token;

namespace {

constexpr std::size_t SEARCH_BULK_BATCH_SIZE = 1024;
constexpr std::size_t SEARCH_RESERVE_BYTES_PER_RECORD = 256;
constexpr long HTTP_STATUS_OK = 200;
constexpr long HTTP_STATUS_CREATED = 201;

} // namespace

SearchWriter::SearchWriter(moirai::Uri uri,
                           std::string search_user,
                           std::string search_pass,
                           std::string search_index,
                           BlockingQueue<std::string>* solution_queue)
  : m_uri(std::move(uri))
  , m_username(std::move(search_user))
  , m_password(std::move(search_pass))
  , m_search_index(std::move(search_index))
  , m_solution_queue(*solution_queue)
{
}

void
SearchWriter::run(const stop_token& stop_token)
{
  auto& app = moirai::Application::instance();

  while (true) {
    std::array<std::string, SEARCH_BULK_BATCH_SIZE> results;
    const size_t num_records =
      m_solution_queue.wait_dequeue_bulk(std::span(results), stop_token);
    if (num_records == 0) {
      if (m_solution_queue.closed()) {
        break;
      }
      continue;
    }

    std::string stringified;
    stringified.reserve(num_records * SEARCH_RESERVE_BYTES_PER_RECORD);
    size_t pushed_records = 0;
    std::vector<std::string> indexed_ids;
    indexed_ids.reserve(num_records);
    std::for_each(results.begin(),
                  results.begin() + static_cast<std::ptrdiff_t>(num_records),
                  [this, &app, &stringified, &pushed_records, &indexed_ids](
                    const std::string& result) -> void {
                    auto package = moirai::parse_json(result);
                    if (!package.has_value() || !package->is_object()) {
                      app.logger().error("Invalid search payload");
                      return;
                    }

                    const auto id_iter = package->find("_id");
                    if (id_iter == package->end() || !id_iter->is_string()) {
                      return;
                    }

                    stringified +=
                      std::format(R"({{"index":{{"_index":"{}","_id":"{}"}}}})",
                                  m_search_index,
                                  id_iter->template get<std::string>());
                    stringified.push_back('\n');
                    indexed_ids.push_back(
                      id_iter->template get_ref<const std::string&>());
                    package->erase("_id");
                    stringified += package->dump();
                    stringified.push_back('\n');
                    ++pushed_records;
                  });

    if (pushed_records == 0) {
      continue;
    }

    try {
      const auto response = moirai::http_post(
        moirai::append_path(m_uri, "/_bulk"),
        stringified,
        { std::format("Authorization: Basic {}",
                      get_encoded_credentials(m_username, m_password)),
          "Content-Type: application/x-ndjson" });

      if (response.status_code == HTTP_STATUS_OK ||
          response.status_code == HTTP_STATUS_CREATED) {
        app.logger().debug("Got successful response from ES Host: {}",
                           response.body);
        std::string indexed_id_list;
        for (size_t index = 0; index < indexed_ids.size(); ++index) {
          if (!indexed_id_list.empty()) {
            indexed_id_list += ", ";
          }
          indexed_id_list += indexed_ids[index];
        }
        app.logger().information("Pushed {} records. Indexed ids: {}",
                                 pushed_records,
                                 indexed_id_list);
        app.logger().information("{}", stringified);
      } else {
        app.logger().error("Error uploading data: {}", response.body);
        app.logger().error("Raw data: {}", stringified);
      }
    } catch (const std::exception& exc) {
      app.logger().error(
        "Error pushing data: {}. Data: {}", exc.what(), stringified);
    }
  }
}
