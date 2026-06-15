#include <nlohmann/json.hpp>
#include <zlib.h>
#include "test_helpers.hxx"

import std;
import moirai.search_writer;

namespace {

using moirai_tests::ScopedLogCapture;
using moirai_tests::expect_eq;
using moirai_tests::expect_true;

struct RecordedCall {
  std::string method;
  std::string path;
  std::string body;
  std::vector<std::string> headers;
};

struct PlannedResponse {
  std::string method;
  std::string path;
  moirai::HttpResponse response;
};

struct FakeSearchState {
  std::vector<PlannedResponse> responses;
  std::vector<RecordedCall> calls;
};

auto test_config() -> SearchIndexConfig {
  SearchIndexConfig config;
  config.bulk_gzip = false;
  config.metrics_interval = std::chrono::seconds{0};
  return config;
}

auto fake_http(std::shared_ptr<FakeSearchState> state) -> SearchHttpRequest {
  return [state](std::string_view method,
                 const moirai::Uri& uri,
                 std::string_view body,
                 const std::vector<std::string>& headers)
           -> moirai::HttpResponse {
    state->calls.push_back(RecordedCall{
      .method = std::string(method),
      .path = uri.path_and_query(),
      .body = std::string(body),
      .headers = headers,
    });

    if (state->calls.size() > state->responses.size()) {
      return { .status_code = 599, .body = "unexpected call" };
    }

    const auto& planned = state->responses[state->calls.size() - 1];
    expect_eq(std::string(method), planned.method, "HTTP method");
    if (!planned.path.empty()) {
      expect_eq(uri.path_and_query(), planned.path, "HTTP path");
    }
    return planned.response;
  };
}

auto writer_with(std::shared_ptr<FakeSearchState> state,
                 BlockingQueue<SearchDocument>& queue,
                 SearchIndexConfig config = test_config()) -> SearchWriter {
  return SearchWriter(moirai::parse_uri("http://search.test"),
                      "user",
                      "pass",
                      "moirai",
                      &queue,
                      std::move(config),
                      fake_http(std::move(state)));
}

auto document(std::string id, std::string package_id = "p1") -> SearchDocument {
  SearchDocument result;
  result.id = id;
  result.waybill = id;
  result.package_id = std::move(package_id);
  result.pdd = "06/08/26 12:00:00";
  result.pdd_ts = 29686320;

  SearchPathLocation location;
  location.code = "A";
  location.arrival = "06/08/26 08:00:00";
  location.arrival_ts = 29686080;
  result.earliest.locations.push_back(std::move(location));
  return result;
}

auto has_header(const std::vector<std::string>& headers, std::string_view name)
    -> bool {
  return std::ranges::any_of(headers, [name](std::string_view header) {
    return header == name;
  });
}

auto valid_mapping() -> std::string {
  return R"({
    "moirai": {
      "mappings": {
        "properties": {
          "waybill": {"type": "keyword"},
          "package": {"type": "keyword"},
          "updated_at": {"type": "date"},
          "updated_at_ts": {"type": "long"}
        }
      }
    }
  })";
}

auto valid_settings() -> std::string {
  return R"({
    "moirai": {
      "settings": {
        "index": {
          "number_of_shards": "24",
          "number_of_replicas": "1",
          "refresh_interval": "30s"
        }
      }
    }
  })";
}

auto split_lines(std::string_view input) -> std::vector<std::string> {
  std::vector<std::string> lines;
  while (!input.empty()) {
    const auto newline = input.find('\n');
    if (newline == std::string_view::npos) {
      lines.emplace_back(input);
      break;
    }
    if (newline > 0) {
      lines.emplace_back(input.substr(0, newline));
    }
    input.remove_prefix(newline + 1);
  }
  return lines;
}

auto gunzip(std::string_view input) -> std::string {
  z_stream stream{};
  if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK) {
    std::cerr << "Failed to initialize gzip decoder\n";
    std::exit(1);
  }

  std::string output;
  std::array<char, 8192> buffer{};
  stream.next_in =
    reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
  stream.avail_in = static_cast<uInt>(input.size());

  int result = Z_OK;
  do {
    stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
    stream.avail_out = static_cast<uInt>(buffer.size());
    result = inflate(&stream, Z_NO_FLUSH);
    if (result != Z_OK && result != Z_STREAM_END) {
      inflateEnd(&stream);
      std::cerr << "Failed to decompress gzip body\n";
      std::exit(1);
    }
    output.append(buffer.data(), buffer.size() - stream.avail_out);
  } while (result != Z_STREAM_END);

  inflateEnd(&stream);
  return output;
}

auto unique_temp_dir(std::string_view name) -> std::filesystem::path {
  const auto now = std::chrono::system_clock::now();
  const auto epoch_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             now.time_since_epoch())
                             .count();
  return std::filesystem::temp_directory_path() /
         std::format("{}-{}", name, epoch_nanos);
}

auto read_audit_lines(const std::filesystem::path& dir)
    -> std::vector<std::string> {
  std::vector<std::string> lines;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
      continue;
    }
    std::ifstream input(entry.path());
    std::string line;
    while (std::getline(input, line)) {
      if (!line.empty()) {
        lines.push_back(std::move(line));
      }
    }
  }
  return lines;
}

auto count_open_audit_files(const std::filesystem::path& dir) -> std::size_t {
  return static_cast<std::size_t>(
    std::ranges::count_if(std::filesystem::directory_iterator(dir),
                          [](const auto& entry) {
                            return entry.is_regular_file() &&
                                   entry.path().extension() == ".open";
                          }));
}

void test_missing_index_is_created_with_mapping_and_settings() {
  BlockingQueue<SearchDocument> queue;
  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 404, .body = "" } },
    { "PUT", "/moirai", { .status_code = 200, .body = R"({"acknowledged":true})" } },
  };

  auto writer = writer_with(state, queue);
  writer.ensure_index_ready();

  expect_eq(state->calls.size(), std::size_t{2}, "index creation call count");
  const auto body = nlohmann::json::parse(state->calls[1].body);
  expect_eq(body["settings"]["index"]["number_of_shards"].get<std::size_t>(),
            std::size_t{24},
            "default shard count");
  expect_eq(body["settings"]["index"]["number_of_replicas"].get<std::size_t>(),
            std::size_t{1},
            "default replica count");
  expect_eq(body["settings"]["index"]["refresh_interval"].get<std::string>(),
            std::string{"30s"},
            "default refresh interval");
  expect_eq(body["mappings"]["properties"]["waybill"]["type"].get<std::string>(),
            std::string{"keyword"},
            "waybill mapping");
  expect_eq(body["mappings"]["properties"]["updated_at_ts"]["type"]
              .get<std::string>(),
            std::string{"long"},
            "updated_at_ts mapping");
}

void test_existing_index_is_validated_without_recreation() {
  BlockingQueue<SearchDocument> queue;
  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
  };

  auto writer = writer_with(state, queue);
  writer.ensure_index_ready();

  expect_eq(state->calls.size(), std::size_t{4}, "validation call count");
  expect_true(std::ranges::none_of(state->calls, [](const RecordedCall& call) {
                return call.method == "PUT";
              }),
              "existing index is not recreated");
}

void test_incompatible_mapping_is_logged_without_destructive_change() {
  BlockingQueue<SearchDocument> queue;
  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = R"({
      "moirai": {"mappings": {"properties": {"waybill": {"type": "text"}}}}
    })" } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
  };
  ScopedLogCapture logs;

  auto writer = writer_with(state, queue);
  writer.ensure_index_ready();

  expect_true(logs.contains("field waybill has type text, expected keyword"),
              "incompatible mapping logged");
  expect_true(std::ranges::none_of(state->calls, [](const RecordedCall& call) {
                return call.method == "PUT";
              }),
              "incompatible index is not recreated");
}

void test_bootstrap_failure_prevents_bulk_indexing() {
  BlockingQueue<SearchDocument> queue;
  queue.enqueue(document("wb-1"));
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 500, .body = "search down" } },
  };

  bool threw = false;
  try {
    auto writer = writer_with(state, queue);
    writer.run(std::stop_token{});
  } catch (const std::runtime_error&) {
    threw = true;
  }

  expect_true(threw, "bootstrap failure throws");
  expect_eq(state->calls.size(), std::size_t{1}, "no bulk call after failure");
}

void test_bulk_indexing_uses_stable_ids_timestamps_and_no_custom_routing() {
  BlockingQueue<SearchDocument> queue;
  queue.enqueue(document("wb-1", "p1"));
  queue.enqueue(document("wb-1", "p2"));
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = R"([
      {"shard":"0","prirep":"p","docs":"100","store":"1000"},
      {"shard":"1","prirep":"p","docs":"100","store":"1000"}
    ])" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
  };

  auto writer = writer_with(state, queue);
  writer.run(std::stop_token{});

  const auto& bulk = state->calls.back();
  expect_eq(bulk.method, std::string{"POST"}, "bulk method");
  const auto lines = split_lines(bulk.body);
  expect_eq(lines.size(), std::size_t{4}, "bulk line count");

  const auto first_metadata = nlohmann::json::parse(lines[0]);
  const auto second_metadata = nlohmann::json::parse(lines[2]);
  expect_eq(first_metadata["index"]["_index"].get<std::string>(),
            std::string{"moirai"},
            "first bulk index");
  expect_eq(second_metadata["index"]["_index"].get<std::string>(),
            std::string{"moirai"},
            "second bulk index");
  expect_eq(first_metadata["index"]["_id"].get<std::string>(),
            std::string{"wb-1"},
            "first stable id");
  expect_eq(second_metadata["index"]["_id"].get<std::string>(),
            std::string{"wb-1"},
            "second stable id");
  expect_true(!first_metadata["index"].contains("routing"),
              "first metadata has no routing");
  expect_true(!second_metadata["index"].contains("routing"),
              "second metadata has no routing");

  const auto first_body = nlohmann::json::parse(lines[1]);
  const auto second_body = nlohmann::json::parse(lines[3]);
  expect_true(!first_body.contains("_id"), "first body omits _id");
  expect_true(!second_body.contains("_id"), "second body omits _id");
  expect_true(first_body.contains("updated_at"), "first body timestamp text");
  expect_true(second_body.contains("updated_at"), "second body timestamp text");
  expect_true(first_body["updated_at_ts"].is_number_integer(),
              "first body epoch timestamp");
  expect_true(second_body["updated_at_ts"].is_number_integer(),
              "second body epoch timestamp");
  expect_eq(first_body["updated_at_ts"].get<std::int64_t>(),
            second_body["updated_at_ts"].get<std::int64_t>(),
            "batch timestamps are consistent");
}

void test_bulk_flushes_when_byte_limit_is_reached() {
  BlockingQueue<SearchDocument> queue;
  queue.enqueue(document("wb-large-1", std::string(200, 'a')));
  queue.enqueue(document("wb-large-2", std::string(200, 'b')));
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
  };
  auto config = test_config();
  config.bulk_max_records = 10;
  config.bulk_max_bytes = 260;

  auto writer = writer_with(state, queue, config);
  writer.run(std::stop_token{});

  const auto post_count = std::ranges::count_if(
    state->calls, [](const RecordedCall& call) { return call.method == "POST"; });
  expect_eq(post_count, std::ptrdiff_t{2}, "byte limit split post count");
}

void test_bulk_partial_failure_retries_retryable_items_only() {
  BlockingQueue<SearchDocument> queue;
  queue.enqueue(document("wb-ok", "p1"));
  queue.enqueue(document("wb-retry", "p2"));
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({
      "errors": true,
      "items": [
        {"index": {"status": 201, "_id": "wb-ok"}},
        {"index": {"status": 429, "_id": "wb-retry"}}
      ]
    })" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
  };

  auto writer = writer_with(state, queue);
  writer.run(std::stop_token{});

  std::vector<RecordedCall> posts;
  for (const auto& call : state->calls) {
    if (call.method == "POST") {
      posts.push_back(call);
    }
  }
  expect_eq(posts.size(), std::size_t{2}, "partial failure retry count");
  expect_true(posts[1].body.find("wb-retry") != std::string::npos,
              "retry body contains retryable id");
  expect_true(posts[1].body.find("wb-ok") == std::string::npos,
              "retry body excludes successful id");
}

void test_adaptive_bulk_sizing_grows_after_success() {
  BlockingQueue<SearchDocument> queue;
  for (int index = 0; index < 8; ++index) {
    queue.enqueue(document(std::format("wb-adaptive-{}", index), "p"));
  }
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
  };
  auto config = test_config();
  config.bulk_adaptive = true;
  config.bulk_min_records = 2;
  config.bulk_max_records = 4;
  config.bulk_target_latency = std::chrono::milliseconds{1000};

  auto writer = writer_with(state, queue, config);
  writer.run(std::stop_token{});

  std::vector<RecordedCall> posts;
  for (const auto& call : state->calls) {
    if (call.method == "POST") {
      posts.push_back(call);
    }
  }
  expect_eq(posts.size(), std::size_t{3}, "adaptive post count");
  expect_eq(split_lines(posts.front().body).size(), std::size_t{4},
            "adaptive starts at min records");
  expect_eq(split_lines(posts[1].body).size(), std::size_t{4},
            "adaptive finishes first dequeue window");
  expect_eq(split_lines(posts.back().body).size(), std::size_t{8},
            "adaptive grows after success");
}

void test_bulk_gzip_adds_content_encoding_and_compresses_body() {
  BlockingQueue<SearchDocument> queue;
  queue.enqueue(document("wb-gzip", "p1"));
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
  };
  auto config = test_config();
  config.bulk_gzip = true;
  config.bulk_gzip_level = 1;

  auto writer = writer_with(state, queue, config);
  writer.run(std::stop_token{});

  const auto& bulk = state->calls.back();
  expect_true(has_header(bulk.headers, "Content-Encoding: gzip"),
              "gzip content encoding header");
  expect_true(bulk.body.find("wb-gzip") == std::string::npos,
              "gzip body is compressed");
  const auto decompressed = gunzip(bulk.body);
  const auto lines = split_lines(decompressed);
  expect_eq(lines.size(), std::size_t{2}, "gzip decompressed line count");
  const auto metadata = nlohmann::json::parse(lines[0]);
  expect_eq(metadata["index"]["_id"].get<std::string>(),
            std::string{"wb-gzip"},
            "gzip body preserves document id");
}

void test_audit_sink_writes_closed_append_records() {
  BlockingQueue<SearchDocument> queue;
  queue.enqueue(document("wb-audit-1", "p1"));
  queue.enqueue(document("wb-audit-2", "p2"));
  queue.enqueue(document("wb-audit-3", "p3"));
  queue.close();

  auto state = std::make_shared<FakeSearchState>();
  state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = "[]" } },
    { "POST", "/_bulk", { .status_code = 200, .body = R"({"errors":false})" } },
  };
  auto config = test_config();
  const auto audit_dir = unique_temp_dir("moirai-audit-test");
  config.audit_enabled = true;
  config.audit_dir = audit_dir;
  config.audit_rotate_records = 2;
  config.audit_rotate_bytes = 1024U * 1024U;
  config.audit_rotate_interval = std::chrono::seconds{3600};

  auto writer = writer_with(state, queue, config);
  writer.run(std::stop_token{});

  expect_eq(count_open_audit_files(audit_dir),
            std::size_t{0},
            "audit files are closed");
  const auto lines = read_audit_lines(audit_dir);
  expect_eq(lines.size(), std::size_t{3}, "audit line count");

  std::set<std::string> waybills;
  for (const auto& line : lines) {
    const auto parsed = nlohmann::json::parse(line);
    waybills.insert(parsed["waybill"].get<std::string>());
    expect_true(parsed.contains("updated_at_ts"),
                "audit body includes update timestamp");
    expect_true(!parsed.contains("_id"), "audit body omits OpenSearch id");
  }
  expect_true(waybills.contains("wb-audit-1"), "first audit waybill");
  expect_true(waybills.contains("wb-audit-2"), "second audit waybill");
  expect_true(waybills.contains("wb-audit-3"), "third audit waybill");

  std::filesystem::remove_all(audit_dir);
}

void test_shard_skew_logs_warning_and_critical_thresholds() {
  BlockingQueue<SearchDocument> warning_queue;
  auto warning_state = std::make_shared<FakeSearchState>();
  warning_state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = R"([
      {"shard":"0","prirep":"p","docs":"100","store":"1000"},
      {"shard":"1","prirep":"p","docs":"160","store":"1600"}
    ])" } },
  };
  ScopedLogCapture warning_logs;
  auto warning_writer = writer_with(warning_state, warning_queue);
  warning_writer.ensure_index_ready();
  expect_true(warning_logs.contains("Search shard skew warning"),
              "warning skew logged");

  BlockingQueue<SearchDocument> critical_queue;
  auto critical_state = std::make_shared<FakeSearchState>();
  critical_state->responses = {
    { "HEAD", "/moirai", { .status_code = 200, .body = "" } },
    { "GET", "/moirai/_mapping", { .status_code = 200, .body = valid_mapping() } },
    { "GET", "/moirai/_settings", { .status_code = 200, .body = valid_settings() } },
    { "GET", "", { .status_code = 200, .body = R"([
      {"shard":"0","prirep":"p","docs":"100","store":"1000"},
      {"shard":"1","prirep":"p","docs":"250","store":"2500"}
    ])" } },
  };
  ScopedLogCapture critical_logs;
  auto critical_writer = writer_with(critical_state, critical_queue);
  critical_writer.ensure_index_ready();
  expect_true(critical_logs.contains("Search shard skew critical"),
              "critical skew logged");
}

} // namespace

auto main() -> int {
  test_missing_index_is_created_with_mapping_and_settings();
  test_existing_index_is_validated_without_recreation();
  test_incompatible_mapping_is_logged_without_destructive_change();
  test_bootstrap_failure_prevents_bulk_indexing();
  test_bulk_indexing_uses_stable_ids_timestamps_and_no_custom_routing();
  test_bulk_flushes_when_byte_limit_is_reached();
  test_bulk_partial_failure_retries_retryable_items_only();
  test_adaptive_bulk_sizing_grows_after_success();
  test_bulk_gzip_adds_content_encoding_and_compresses_body();
  test_audit_sink_writes_closed_append_records();
  test_shard_skew_logs_warning_and_critical_thresholds();
  return 0;
}
