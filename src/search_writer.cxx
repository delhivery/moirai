module;

#include "blocking_queue.hxx"
#include <zlib.h>

module moirai.search_writer;

import std;
import moirai.app;
import moirai.http;
import moirai.json_utils;
import moirai.search_document;
import moirai.utils;

using std::stop_token;

namespace {

constexpr std::size_t SEARCH_RESERVE_BYTES_PER_RECORD = 256;
constexpr long HTTP_STATUS_OK = 200;
constexpr long HTTP_STATUS_CREATED = 201;
constexpr long HTTP_STATUS_NOT_FOUND = 404;
constexpr std::size_t GZIP_CHUNK_BYTES = 16U * 1024U;
constexpr std::string_view DISPLAY_DATE_FORMAT =
  "MM/dd/yy HH:mm:ss||strict_date_optional_time";

struct BulkDocument {
  std::string id;
  std::string body;
};

struct BulkMetrics {
  std::uint64_t attempted_records{0};
  std::uint64_t audited_records{0};
  std::uint64_t indexed_records{0};
  std::uint64_t failed_records{0};
  std::uint64_t retried_records{0};
  std::uint64_t uploaded_bytes{0};
  std::uint64_t bulk_requests{0};
  std::chrono::steady_clock::time_point last_report{
    std::chrono::steady_clock::now()
  };
};

struct BulkItemFailures {
  bool parsed{false};
  bool has_errors{false};
  std::size_t permanent_failures{0};
  std::vector<BulkDocument> retryable;
};

std::atomic<std::uint64_t> audit_file_sequence{0};

auto parse_size_env(const char* name, std::size_t fallback,
                    bool allow_zero = false) -> std::size_t {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }

  std::size_t parsed{};
  const std::string_view input{value};
  const auto* begin = input.data();
  const auto* end = begin + input.size();
  const auto [ptr, error] = std::from_chars(begin, end, parsed);
  if (error != std::errc{} || ptr != end || (!allow_zero && parsed == 0)) {
    throw std::runtime_error(std::format("Invalid {} value '{}'", name, input));
  }

  return parsed;
}

auto parse_double_env(const char* name, double fallback) -> double {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }

  double parsed{};
  const std::string input{value};
  const auto [ptr, error] =
    std::from_chars(input.data(), input.data() + input.size(), parsed);
  if (error != std::errc{} || ptr != input.data() + input.size() ||
      parsed <= 0.0) {
    throw std::runtime_error(std::format("Invalid {} value '{}'", name, input));
  }

  return parsed;
}

auto parse_string_env(const char* name, std::string fallback) -> std::string {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }

  return value;
}

auto parse_path_env(const char* name, std::filesystem::path fallback)
    -> std::filesystem::path {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }

  return std::filesystem::path{value};
}

auto parse_bool_env(const char* name, bool fallback) -> bool {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }

  std::string input{value};
  std::ranges::transform(input, input.begin(), [](unsigned char chr) {
    return static_cast<char>(std::tolower(chr));
  });
  if (input == "1" || input == "true" || input == "yes" || input == "on") {
    return true;
  }
  if (input == "0" || input == "false" || input == "no" || input == "off") {
    return false;
  }
  throw std::runtime_error(std::format("Invalid {} value '{}'", name, input));
}

auto parse_gzip_level_env(const char* name, int fallback) -> int {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }

  int parsed{};
  const std::string_view input{value};
  const auto [ptr, error] =
    std::from_chars(input.data(), input.data() + input.size(), parsed);
  if (error != std::errc{} || ptr != input.data() + input.size() ||
      parsed < 0 || parsed > 9) {
    throw std::runtime_error(std::format("Invalid {} value '{}'", name, input));
  }
  return parsed;
}

auto default_search_request() -> SearchHttpRequest {
  auto client = std::make_shared<moirai::HttpClient>();
  return [client](std::string_view method,
                  const moirai::Uri& uri,
                  std::string_view body,
                  const std::vector<std::string>& headers)
           -> moirai::HttpResponse {
    return client->request(method, uri, body, headers);
  };
}

auto utc_timestamp(std::chrono::system_clock::time_point timestamp)
    -> std::string {
  const auto seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
  return std::format("{:%Y-%m-%dT%H:%M:%SZ}", seconds);
}

auto epoch_seconds(std::chrono::system_clock::time_point timestamp)
    -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::seconds>(
           timestamp.time_since_epoch())
    .count();
}

auto audit_timestamp(std::chrono::system_clock::time_point timestamp)
    -> std::string {
  const auto seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
  return std::format("{:%Y%m%dT%H%M%SZ}", seconds);
}

void gzip_append(z_stream& stream, std::string& output, std::string_view input,
                 int flush) {
  stream.next_in =
    reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
  stream.avail_in = static_cast<uInt>(input.size());

  int result = Z_OK;
  do {
    const auto previous_size = output.size();
    output.resize(previous_size + GZIP_CHUNK_BYTES);
    stream.next_out =
      reinterpret_cast<Bytef*>(output.data() + previous_size);
    stream.avail_out = GZIP_CHUNK_BYTES;
    result = deflate(&stream, flush);
    if (result == Z_STREAM_ERROR) {
      throw std::runtime_error("Failed to gzip compress bulk payload");
    }
    output.resize(output.size() - stream.avail_out);
  } while (stream.avail_in != 0 || (flush == Z_FINISH && result != Z_STREAM_END));
}

auto is_retryable_status(long status_code) -> bool {
  return status_code == 429 || status_code == 502 || status_code == 503 ||
         status_code == 504;
}

auto is_retryable_item_status(int status_code) -> bool {
  return status_code == 429 || status_code == 502 || status_code == 503 ||
         status_code == 504;
}

class AuditSink {
public:
  explicit AuditSink(const SearchIndexConfig& config)
    : m_enabled(config.audit_enabled)
    , m_dir(config.audit_dir)
    , m_rotate_records(config.audit_rotate_records)
    , m_rotate_bytes(config.audit_rotate_bytes)
    , m_rotate_interval(config.audit_rotate_interval)
  {
    if (!m_enabled) {
      return;
    }
    if (m_dir.empty()) {
      throw std::runtime_error("DWH_AUDIT_DIR is required when DWH_AUDIT_ENABLED=true");
    }
    std::filesystem::create_directories(m_dir);
  }

  AuditSink(const AuditSink&) = delete;
  auto operator=(const AuditSink&) -> AuditSink& = delete;

  ~AuditSink()
  {
    try {
      close();
    } catch (...) {
    }
  }

  void write(std::string_view body)
  {
    if (!m_enabled) {
      return;
    }

    if (!m_stream.is_open()) {
      open_file();
    }

    const auto next_bytes = body.size() + 1U;
    const auto elapsed = std::chrono::steady_clock::now() - m_opened_at;
    if (m_records > 0 &&
        (m_records >= m_rotate_records ||
         m_bytes + next_bytes > m_rotate_bytes ||
         elapsed >= m_rotate_interval)) {
      close();
      open_file();
    }

    m_stream.write(body.data(), static_cast<std::streamsize>(body.size()));
    m_stream.put('\n');
    if (!m_stream) {
      throw std::runtime_error(std::format("Unable to write DWH audit file {}",
                                           m_open_path.string()));
    }
    ++m_records;
    m_bytes += next_bytes;
  }

  void close()
  {
    if (!m_stream.is_open()) {
      return;
    }

    m_stream.flush();
    if (!m_stream) {
      throw std::runtime_error(std::format("Unable to flush DWH audit file {}",
                                           m_open_path.string()));
    }
    m_stream.close();

    if (m_records == 0) {
      std::filesystem::remove(m_open_path);
    } else {
      std::error_code error;
      std::filesystem::rename(m_open_path, m_ready_path, error);
      if (error) {
        throw std::runtime_error(
          std::format("Unable to publish DWH audit file {} -> {}: {}",
                      m_open_path.string(),
                      m_ready_path.string(),
                      error.message()));
      }
    }

    m_records = 0;
    m_bytes = 0;
    m_open_path.clear();
    m_ready_path.clear();
  }

private:
  bool m_enabled{false};
  std::filesystem::path m_dir;
  std::size_t m_rotate_records{100'000};
  std::size_t m_rotate_bytes{128U * 1024U * 1024U};
  std::chrono::seconds m_rotate_interval{300};
  std::ofstream m_stream;
  std::filesystem::path m_open_path;
  std::filesystem::path m_ready_path;
  std::size_t m_records{0};
  std::size_t m_bytes{0};
  std::chrono::steady_clock::time_point m_opened_at{};

  void open_file()
  {
    const auto now = std::chrono::system_clock::now();
    const auto epoch_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               now.time_since_epoch())
                               .count();
    const auto sequence = audit_file_sequence.fetch_add(1) + 1;
    const auto stem = std::format("moirai-audit-{}-{}-{:06}",
                                  audit_timestamp(now),
                                  epoch_nanos,
                                  sequence);
    m_open_path = m_dir / (stem + ".jsonl.open");
    m_ready_path = m_dir / (stem + ".jsonl");
    m_stream.open(m_open_path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!m_stream.is_open()) {
      throw std::runtime_error(std::format("Unable to open DWH audit file {}",
                                           m_open_path.string()));
    }
    m_records = 0;
    m_bytes = 0;
    m_opened_at = std::chrono::steady_clock::now();
  }
};

auto sample_ids(const std::vector<std::string>& ids, std::size_t sample_size)
    -> std::string {
  std::string output;
  const auto count = std::min(ids.size(), sample_size);
  for (std::size_t index = 0; index < count; ++index) {
    if (!output.empty()) {
      output += ", ";
    }
    output += ids[index];
  }
  if (ids.size() > count) {
    output += ", ...";
  }
  return output;
}

void append_json_string(std::string& output, std::string_view value) {
  output.push_back('"');
  for (const unsigned char chr : value) {
    switch (chr) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\b':
      output += "\\b";
      break;
    case '\f':
      output += "\\f";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (chr < 0x20) {
        output += std::format("\\u{:04x}", static_cast<unsigned>(chr));
      } else {
        output.push_back(static_cast<char>(chr));
      }
    }
  }
  output.push_back('"');
}

void append_field_prefix(std::string& output, std::string_view key,
                         bool& first) {
  if (!first) {
    output.push_back(',');
  }
  first = false;
  append_json_string(output, key);
  output.push_back(':');
}

void append_string_field(std::string& output, std::string_view key,
                         std::string_view value, bool& first) {
  append_field_prefix(output, key, first);
  append_json_string(output, value);
}

void append_int_field(std::string& output, std::string_view key,
                      std::int64_t value, bool& first) {
  append_field_prefix(output, key, first);
  output += std::to_string(value);
}

void append_location(std::string& output, const SearchPathLocation& location) {
  output.push_back('{');
  bool first = true;
  append_string_field(output, "code", location.code, first);
  if (!location.facility_name.empty()) {
    append_string_field(output, "facility_name", location.facility_name, first);
  }
  append_string_field(output, "arrival", location.arrival, first);
  append_int_field(output, "arrival_ts", location.arrival_ts, first);
  if (location.has_departure) {
    append_string_field(output, "route", location.route, first);
    if (!location.route_name.empty()) {
      append_string_field(output, "route_name", location.route_name, first);
    }
    append_string_field(output, "departure", location.departure, first);
    append_int_field(output, "departure_ts", location.departure_ts, first);
  }
  output.push_back('}');
}

void append_path_section(std::string& output, std::string_view key,
                         const SearchPathSection& section, bool& first) {
  if (section.locations.empty()) {
    return;
  }

  append_field_prefix(output, key, first);
  output.push_back('{');
  bool section_first = true;
  append_field_prefix(output, "locations", section_first);
  output.push_back('[');
  for (std::size_t index = 0; index < section.locations.size(); ++index) {
    if (index > 0) {
      output.push_back(',');
    }
    append_location(output, section.locations[index]);
  }
  output.push_back(']');

  append_field_prefix(output, "first", section_first);
  append_location(output, section.locations.front());
  if (section.locations.size() > 1) {
    append_field_prefix(output, "second", section_first);
    append_location(output, section.locations[1]);
  }
  output.push_back('}');
}

auto serialize_document_body(const SearchDocument& document,
                             std::string_view updated_at,
                             std::int64_t updated_at_ts) -> std::string {
  std::string output;
  output.reserve(512);
  output.push_back('{');
  bool first = true;
  append_string_field(output, "waybill", document.waybill, first);
  append_string_field(output, "package", document.package_id, first);
  append_string_field(output, "cs_slid", document.cs_slid, first);
  append_string_field(output, "cs_act", document.cs_act, first);
  append_string_field(output, "pid", document.pid, first);
  if (!document.fail.empty()) {
    append_string_field(output, "fail", document.fail, first);
  }
  append_path_section(output, "earliest", document.earliest, first);
  append_path_section(output, "ultimate", document.ultimate, first);
  append_string_field(output, "pdd", document.pdd, first);
  append_int_field(output, "pdd_ts", document.pdd_ts, first);
  append_string_field(output, "updated_at", updated_at, first);
  append_int_field(output, "updated_at_ts", updated_at_ts, first);
  output.push_back('}');
  return output;
}

auto estimate_bulk_payload_bytes(const std::vector<BulkDocument>& documents,
                                 std::string_view search_index)
    -> std::size_t {
  return std::accumulate(
    documents.begin(),
    documents.end(),
    std::size_t{0},
    [search_index](std::size_t total, const BulkDocument& document) {
      return total + document.id.size() + document.body.size() +
             search_index.size() + SEARCH_RESERVE_BYTES_PER_RECORD;
    });
}

void append_bulk_payload(std::string& payload,
                         const std::vector<BulkDocument>& documents,
                         std::string_view search_index) {
  const auto bytes = std::accumulate(
    documents.begin(),
    documents.end(),
    std::size_t{0},
    [search_index](std::size_t total, const BulkDocument& document) {
      return total + document.id.size() + document.body.size() +
             search_index.size() + SEARCH_RESERVE_BYTES_PER_RECORD;
    });
  payload.clear();
  payload.reserve(bytes);
  for (const auto& document : documents) {
    payload += R"({"index":{"_index":)";
    append_json_string(payload, search_index);
    payload += R"(,"_id":)";
    append_json_string(payload, document.id);
    payload += "}}\n";
    payload += document.body;
    payload.push_back('\n');
  }
}

auto gzip_bulk_payload(const std::vector<BulkDocument>& documents,
                       std::string_view search_index,
                       int compression_level) -> std::string {
  z_stream stream{};
  if (deflateInit2(&stream,
                   compression_level,
                   Z_DEFLATED,
                   MAX_WBITS + 16,
                   8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    throw std::runtime_error("Failed to initialize gzip compressor");
  }

  std::string output;
  output.reserve(compressBound(
    static_cast<uLong>(estimate_bulk_payload_bytes(documents, search_index))));
  try {
    std::string metadata;
    metadata.reserve(search_index.size() + 128U);
    for (const auto& document : documents) {
      gzip_append(stream, output, R"({"index":{"_index":)", Z_NO_FLUSH);
      metadata.clear();
      append_json_string(metadata, search_index);
      metadata += R"(,"_id":)";
      append_json_string(metadata, document.id);
      metadata += "}}\n";
      gzip_append(stream, output, metadata, Z_NO_FLUSH);
      gzip_append(stream, output, document.body, Z_NO_FLUSH);
      gzip_append(stream, output, "\n", Z_NO_FLUSH);
    }
    gzip_append(stream, output, {}, Z_FINISH);
  } catch (...) {
    deflateEnd(&stream);
    throw;
  }

  deflateEnd(&stream);
  return output;
}

auto document_ids(const std::vector<BulkDocument>& documents)
    -> std::vector<std::string> {
  std::vector<std::string> ids;
  ids.reserve(documents.size());
  for (const auto& document : documents) {
    ids.push_back(document.id);
  }
  return ids;
}

auto parse_bulk_item_failures(const std::vector<BulkDocument>& documents,
                              std::string_view response_body)
    -> BulkItemFailures {
  BulkItemFailures result;
  const auto parsed = moirai::parse_json(response_body);
  if (!parsed.has_value() || !parsed->is_object()) {
    result.retryable = documents;
    return result;
  }
  result.parsed = true;

  const auto* errors = moirai::find_member(*parsed, "errors");
  if (errors == nullptr) {
    return result;
  }
  const auto errors_bool = moirai::get_bool(*errors);
  if (!errors_bool.has_value() || !*errors_bool) {
    return result;
  }
  result.has_errors = true;

  const auto* items = moirai::find_array_member(*parsed, "items");
  if (items == nullptr) {
    result.retryable = documents;
    return result;
  }

  const auto count = std::min(moirai::json_size(*items), documents.size());
  result.retryable.reserve(count);
  std::size_t index = 0;
  for (const auto& item : *items) {
    if (index >= count) {
      break;
    }
    const auto* operation = moirai::find_object_member(item, "index");
    if (operation == nullptr) {
      operation = moirai::find_object_member(item, "create");
    }
    if (operation == nullptr) {
      ++index;
      continue;
    }
    const auto status = moirai::find_integer_member<int>(*operation, "status");
    if (status.has_value() && is_retryable_item_status(*status)) {
      result.retryable.push_back(documents[index]);
    } else if (status.has_value() && *status >= 300) {
      ++result.permanent_failures;
    }
    ++index;
  }
  if (moirai::json_size(*items) < documents.size()) {
    result.retryable.insert(result.retryable.end(),
                            documents.begin() +
                              static_cast<std::ptrdiff_t>(moirai::json_size(*items)),
                            documents.end());
  }

  return result;
}

auto mapping_at(const moirai::Json& properties, std::string_view field)
    -> const moirai::Json* {
  const moirai::Json* current_properties = &properties;
  while (true) {
    const auto dot = field.find('.');
    const auto segment = field.substr(0, dot);
    const auto* member = moirai::find_member(*current_properties,
                                             std::string(segment).c_str());
    if (member == nullptr || !member->is_object()) {
      return nullptr;
    }

    if (dot == std::string_view::npos) {
      return member;
    }

    const auto* nested_properties = moirai::find_object_member(*member,
                                                                "properties");
    if (nested_properties == nullptr) {
      return nullptr;
    }

    current_properties = nested_properties;
    field.remove_prefix(dot + 1);
  }
}

auto field_type(const moirai::Json& properties, std::string_view field)
    -> std::optional<std::string> {
  const auto* mapping = mapping_at(properties, field);
  if (mapping == nullptr) {
    return std::nullopt;
  }

  return moirai::find_string_member(*mapping, "type")
    .transform([](std::string_view sv) { return std::string(sv); });
}

auto field_bool(const moirai::Json& properties, std::string_view field,
                std::string_view key) -> std::optional<bool> {
  const auto* mapping = mapping_at(properties, field);
  if (mapping == nullptr) {
    return std::nullopt;
  }

  const auto* member = moirai::find_member(*mapping, std::string(key).c_str());
  if (member == nullptr) {
    return std::nullopt;
  }

  return moirai::get_bool(*member);
}

auto field_string(const moirai::Json& properties, std::string_view field,
                  std::string_view key) -> std::optional<std::string> {
  const auto* mapping = mapping_at(properties, field);
  if (mapping == nullptr) {
    return std::nullopt;
  }

  const auto* member = moirai::find_member(*mapping, std::string(key).c_str());
  if (member == nullptr) {
    return std::nullopt;
  }
  const auto sv = moirai::get_string(*member);
  if (!sv.has_value()) {
    return std::nullopt;
  }
  return std::string(*sv);
}

auto integer_from_json(const moirai::Json& value) -> std::optional<std::int64_t> {
  if (value.is_int64()) {
    return value.get_int64().value_unsafe();
  }
  if (value.is_uint64()) {
    const auto number = value.get_uint64().value_unsafe();
    if (number > static_cast<std::uint64_t>(
                   std::numeric_limits<std::int64_t>::max())) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(number);
  }
  if (!value.is_string()) {
    return std::nullopt;
  }

  const auto text = value.get_string().value_unsafe();
  std::int64_t parsed{};
  const auto [ptr, error] =
    std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (error != std::errc{} || ptr != text.data() + text.size()) {
    return std::nullopt;
  }
  return parsed;
}

auto decimal_from_json(const moirai::Json& value) -> std::optional<double> {
  if (value.is_double()) {
    return value.get_double().value_unsafe();
  }
  if (value.is_int64()) {
    return static_cast<double>(value.get_int64().value_unsafe());
  }
  if (value.is_uint64()) {
    return static_cast<double>(value.get_uint64().value_unsafe());
  }
  if (!value.is_string()) {
    return std::nullopt;
  }

  const auto text = value.get_string().value_unsafe();
  double parsed{};
  const auto [ptr, error] =
    std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (error != std::errc{} || ptr != text.data() + text.size()) {
    return std::nullopt;
  }
  return parsed;
}

} // namespace

auto
SearchIndexConfig::from_environment() -> SearchIndexConfig
{
  SearchIndexConfig config;
  config.shards = parse_size_env("SEARCH_SHARDS", config.shards);
  config.replicas =
    parse_size_env("SEARCH_REPLICAS", config.replicas, true);
  config.refresh_interval =
    parse_string_env("SEARCH_REFRESH_INTERVAL", config.refresh_interval);
  config.shard_warning_ratio =
    parse_double_env("SEARCH_SHARD_WARN_RATIO", config.shard_warning_ratio);
  config.shard_critical_ratio =
    parse_double_env("SEARCH_SHARD_CRITICAL_RATIO", config.shard_critical_ratio);
  config.bulk_max_records =
    parse_size_env("SEARCH_BULK_MAX_RECORDS", config.bulk_max_records);
  config.bulk_max_bytes =
    parse_size_env("SEARCH_BULK_MAX_BYTES", config.bulk_max_bytes);
  config.bulk_max_retries =
    parse_size_env("SEARCH_BULK_MAX_RETRIES", config.bulk_max_retries, true);
  config.log_sample_ids =
    parse_size_env("SEARCH_LOG_SAMPLE_IDS", config.log_sample_ids, true);
  config.metrics_interval = std::chrono::seconds{
    parse_size_env("SEARCH_METRICS_INTERVAL_SECONDS",
                   static_cast<std::size_t>(config.metrics_interval.count()),
                   true)
  };
  config.bulk_gzip = parse_bool_env("SEARCH_BULK_GZIP", config.bulk_gzip);
  config.bulk_gzip_level =
    parse_gzip_level_env("SEARCH_BULK_GZIP_LEVEL", config.bulk_gzip_level);
  config.bulk_adaptive =
    parse_bool_env("SEARCH_BULK_ADAPTIVE", config.bulk_adaptive);
  config.bulk_min_records =
    parse_size_env("SEARCH_BULK_MIN_RECORDS", config.bulk_min_records);
  config.bulk_target_latency = std::chrono::milliseconds{
    parse_size_env("SEARCH_BULK_TARGET_LATENCY_MS",
                   static_cast<std::size_t>(
                     config.bulk_target_latency.count()))
  };
  config.audit_enabled =
    parse_bool_env("DWH_AUDIT_ENABLED", config.audit_enabled);
  config.audit_dir = parse_path_env("DWH_AUDIT_DIR", config.audit_dir);
  config.audit_rotate_records =
    parse_size_env("DWH_AUDIT_ROTATE_RECORDS", config.audit_rotate_records);
  config.audit_rotate_bytes =
    parse_size_env("DWH_AUDIT_ROTATE_BYTES", config.audit_rotate_bytes);
  config.audit_rotate_interval = std::chrono::seconds{
    parse_size_env("DWH_AUDIT_ROTATE_SECONDS",
                   static_cast<std::size_t>(
                     config.audit_rotate_interval.count()))
  };
  config.bulk_min_records =
    std::min(config.bulk_min_records, config.bulk_max_records);
  if (config.audit_enabled && config.audit_dir.empty()) {
    throw std::runtime_error(
      "DWH_AUDIT_DIR is required when DWH_AUDIT_ENABLED=true");
  }
  if (config.shard_critical_ratio < config.shard_warning_ratio) {
    throw std::runtime_error(
      "SEARCH_SHARD_CRITICAL_RATIO must be greater than or equal to "
      "SEARCH_SHARD_WARN_RATIO");
  }
  return config;
}

SearchWriter::SearchWriter(moirai::Uri uri,
                           std::string search_user,
                           std::string search_pass,
                           std::string search_index,
                           BlockingQueue<SearchDocument>* solution_queue)
  : SearchWriter(std::move(uri),
                 std::move(search_user),
                 std::move(search_pass),
                 std::move(search_index),
                 solution_queue,
                 SearchIndexConfig::from_environment(),
                 default_search_request())
{
  m_shared_index_guard = true;
}

SearchWriter::SearchWriter(moirai::Uri uri,
                           std::string search_user,
                           std::string search_pass,
                           std::string search_index,
                           BlockingQueue<SearchDocument>* solution_queue,
                           SearchIndexConfig index_config,
                           SearchHttpRequest http_request)
  : m_uri(std::move(uri))
  , m_username(std::move(search_user))
  , m_password(std::move(search_pass))
  , m_search_index(std::move(search_index))
  , m_index_config(std::move(index_config))
  , m_solution_queue(*solution_queue)
  , m_http_request(std::move(http_request))
{
}

auto
SearchWriter::authorization_headers() const -> std::vector<std::string>
{
  return { std::format("Authorization: Basic {}",
                       get_encoded_credentials(m_username, m_password)) };
}

auto
SearchWriter::json_headers() const -> std::vector<std::string>
{
  auto headers = authorization_headers();
  headers.emplace_back("Content-Type: application/json");
  return headers;
}

auto
SearchWriter::bulk_headers(bool compressed) const -> std::vector<std::string>
{
  auto headers = authorization_headers();
  headers.emplace_back("Content-Type: application/x-ndjson");
  if (compressed) {
    headers.emplace_back("Content-Encoding: gzip");
  }
  return headers;
}

auto
SearchWriter::create_index_body() const -> std::string
{
  const auto keyword = [](std::size_t ignore_above) -> std::string {
    return std::format(R"({{"type":"keyword","ignore_above":{}}})", ignore_above);
  };
  const auto long_type = R"({"type":"long"})";
  const auto display_date = std::format(
    R"({{"type":"date","format":"{}"}})", DISPLAY_DATE_FORMAT);
  const auto location = std::format(
    R"({{"type":"object","dynamic":false,"properties":{{)"
    R"("code":{},"facility_name":{},"arrival":{},"arrival_ts":{},"route":{},"route_name":{},"departure":{},"departure_ts":{})"
    R"(}}}})",
    keyword(128), keyword(256), display_date, long_type, keyword(512), keyword(512), display_date, long_type);
  const auto path_section = std::format(
    R"({{"type":"object","dynamic":false,"properties":{{)"
    R"("locations":{{"type":"object","enabled":false}},)"
    R"("first":{},"second":{})"
    R"(}}}})",
    location, location);

  return std::format(
    R"({{"settings":{{"index":{{"number_of_shards":{},"number_of_replicas":{},"refresh_interval":"{}"}}}},)"
    R"("mappings":{{"dynamic":false,"properties":{{)"
    R"("waybill":{},"package":{},"cs_slid":{},"cs_act":{},"pid":{},"fail":{},)"
    R"("pdd":{},"pdd_ts":{},"updated_at":{{"type":"date"}},"updated_at_ts":{},)"
    R"("earliest":{},"ultimate":{},"critical":{})"
    R"(}}}}}})",
    m_index_config.shards,
    m_index_config.replicas,
    m_index_config.refresh_interval,
    keyword(128), keyword(128), keyword(128), keyword(128), keyword(128),
    keyword(8191),
    display_date, long_type, long_type,
    path_section, path_section, path_section);
}

void
SearchWriter::create_index()
{
  auto& app = moirai::Application::instance();
  const auto response = m_http_request("PUT",
                                       moirai::append_path(m_uri,
                                                           "/" + m_search_index),
                                       create_index_body(),
                                       json_headers());
  if (response.status_code != HTTP_STATUS_OK &&
      response.status_code != HTTP_STATUS_CREATED) {
    throw std::runtime_error(
      std::format("Unable to create search index {}: HTTP {} {}",
                  m_search_index,
                  response.status_code,
                  response.body));
  }

  app.logger().information(
    "Created search index {} with {} shards, {} replicas, refresh interval {}",
    m_search_index,
    m_index_config.shards,
    m_index_config.replicas,
    m_index_config.refresh_interval);
}

void
SearchWriter::validate_index_definition()
{
  auto& app = moirai::Application::instance();
  const auto mapping_response =
    m_http_request("GET",
                   moirai::append_path(m_uri, "/" + m_search_index + "/_mapping"),
                   {},
                   authorization_headers());
  if (mapping_response.status_code != HTTP_STATUS_OK) {
    app.logger().error("Unable to validate search index mapping {}: HTTP {} {}",
                       m_search_index,
                       mapping_response.status_code,
                       mapping_response.body);
    return;
  }

  const auto parsed_mapping = moirai::parse_json(mapping_response.body);
  if (!parsed_mapping.has_value() || !parsed_mapping->is_object()) {
    app.logger().error("Unable to parse search index mapping for {}",
                       m_search_index);
    return;
  }

  const moirai::Json* properties = nullptr;
  for (const auto& kv : parsed_mapping->get_object().value_unsafe()) {
    const auto& index_definition = kv.value;
    if (!index_definition.is_object()) {
      continue;
    }
    const auto* mappings = moirai::find_object_member(index_definition,
                                                       "mappings");
    if (mappings == nullptr) {
      continue;
    }
    const auto* props = moirai::find_object_member(*mappings, "properties");
    if (props == nullptr) {
      continue;
    }
    properties = props;
    break;
  }

  if (properties == nullptr) {
    app.logger().error("Search index {} mapping has no properties",
                       m_search_index);
    return;
  }

  const auto validate_field_type =
    [&](std::string_view field, std::string_view expected_type) {
    const auto actual_type = field_type(*properties, field);
    if (!actual_type.has_value() || *actual_type != expected_type) {
      app.logger().error("Search index {} field {} has type {}, expected {}",
                         m_search_index,
                         field,
                         actual_type.value_or("<missing>"),
                         expected_type);
    }
  };
  const auto validate_field_format =
    [&](std::string_view field, std::string_view expected_format) {
      const auto actual_format = field_string(*properties, field, "format");
      if (!actual_format.has_value() || *actual_format != expected_format) {
        app.logger().error(
          "Search index {} field {} has format {}, expected {}",
          m_search_index,
          field,
          actual_format.value_or("<missing>"),
          expected_format);
      }
    };

  const std::array top_level_fields{
    std::pair{ "waybill", "keyword" },
    std::pair{ "package", "keyword" },
    std::pair{ "cs_slid", "keyword" },
    std::pair{ "cs_act", "keyword" },
    std::pair{ "pid", "keyword" },
    std::pair{ "fail", "keyword" },
    std::pair{ "pdd", "date" },
    std::pair{ "pdd_ts", "long" },
    std::pair{ "updated_at", "date" },
    std::pair{ "updated_at_ts", "long" },
  };
  for (const auto& [field, expected_type] : top_level_fields) {
    validate_field_type(field, expected_type);
  }
  validate_field_format("pdd", DISPLAY_DATE_FORMAT);

  constexpr std::array path_sections{ "earliest", "ultimate", "critical" };
  constexpr std::array path_positions{ "first", "second" };
  constexpr std::array path_location_fields{
    std::pair{ "code", "keyword" },
    std::pair{ "facility_name", "keyword" },
    std::pair{ "arrival", "date" },
    std::pair{ "arrival_ts", "long" },
    std::pair{ "route", "keyword" },
    std::pair{ "route_name", "keyword" },
    std::pair{ "departure", "date" },
    std::pair{ "departure_ts", "long" },
  };
  for (std::string_view section : path_sections) {
    validate_field_type(section, "object");
    const auto locations_field = std::format("{}.locations", section);
    validate_field_type(locations_field, "object");
    const auto locations_enabled =
      field_bool(*properties, locations_field, "enabled");
    if (!locations_enabled.has_value() || *locations_enabled) {
      app.logger().error(
        "Search index {} field {} has enabled {}, expected false",
        m_search_index,
        locations_field,
        locations_enabled.has_value()
          ? std::format("{}", *locations_enabled)
          : std::string{"<missing>"});
    }

    for (std::string_view position : path_positions) {
      const auto position_field = std::format("{}.{}", section, position);
      validate_field_type(position_field, "object");
      for (const auto& [field, expected_type] : path_location_fields) {
        validate_field_type(std::format("{}.{}", position_field, field),
                            expected_type);
      }
      validate_field_format(std::format("{}.arrival", position_field),
                            DISPLAY_DATE_FORMAT);
      validate_field_format(std::format("{}.departure", position_field),
                            DISPLAY_DATE_FORMAT);
    }
  }

  const auto settings_response =
    m_http_request("GET",
                   moirai::append_path(m_uri, "/" + m_search_index + "/_settings"),
                   {},
                   authorization_headers());
  if (settings_response.status_code != HTTP_STATUS_OK) {
    app.logger().error("Unable to validate search index settings {}: HTTP {} {}",
                       m_search_index,
                       settings_response.status_code,
                       settings_response.body);
    return;
  }

  const auto parsed_settings = moirai::parse_json(settings_response.body);
  if (!parsed_settings.has_value() || !parsed_settings->is_object()) {
    app.logger().error("Unable to parse search index settings for {}",
                       m_search_index);
    return;
  }

  for (const auto& kv : parsed_settings->get_object().value_unsafe()) {
    const auto& index_definition = kv.value;
    const auto* settings = moirai::find_object_member(index_definition,
                                                      "settings");
    if (settings == nullptr) {
      continue;
    }
    const auto* index_settings = moirai::find_object_member(*settings, "index");
    if (index_settings == nullptr) {
      continue;
    }

    const auto* shards = moirai::find_member(*index_settings,
                                             "number_of_shards");
    if (shards != nullptr) {
      const auto actual_shards = integer_from_json(*shards);
      if (!actual_shards.has_value() ||
          *actual_shards != static_cast<std::int64_t>(m_index_config.shards)) {
        app.logger().error("Search index {} has {} shards, configured {}",
                           m_search_index,
                           actual_shards.has_value()
                             ? std::to_string(*actual_shards)
                             : std::string{"<unknown>"},
                           m_index_config.shards);
      }
    }

    const auto* replicas = moirai::find_member(*index_settings,
                                               "number_of_replicas");
    if (replicas != nullptr) {
      const auto actual_replicas = integer_from_json(*replicas);
      if (!actual_replicas.has_value() ||
          *actual_replicas !=
            static_cast<std::int64_t>(m_index_config.replicas)) {
        app.logger().error("Search index {} has {} replicas, configured {}",
                           m_search_index,
                           actual_replicas.has_value()
                             ? std::to_string(*actual_replicas)
                             : std::string{"<unknown>"},
                           m_index_config.replicas);
      }
    }
    break;
  }
}

void
SearchWriter::check_shard_balance()
{
  auto& app = moirai::Application::instance();
  const auto uri = moirai::with_query_parameters(
    moirai::append_path(m_uri, "/_cat/shards/" + m_search_index),
    { { "format", "json" }, { "bytes", "b" }, { "h", "shard,prirep,docs,store" } });
  const auto response = m_http_request("GET", uri, {}, authorization_headers());
  if (response.status_code != HTTP_STATUS_OK) {
    app.logger().error("Unable to inspect search shard balance {}: HTTP {} {}",
                       m_search_index,
                       response.status_code,
                       response.body);
    return;
  }

  const auto parsed = moirai::parse_json(response.body);
  if (!parsed.has_value() || !parsed->is_array()) {
    app.logger().error("Unable to parse search shard balance for {}",
                       m_search_index);
    return;
  }

  std::int64_t min_docs = std::numeric_limits<std::int64_t>::max();
  std::int64_t max_docs = 0;
  std::int64_t min_store = std::numeric_limits<std::int64_t>::max();
  std::int64_t max_store = 0;
  std::size_t primary_shards = 0;

  for (const auto& shard : *parsed) {
    if (!shard.is_object()) {
      continue;
    }
    const auto prirep = moirai::find_string_member(shard, "prirep");
    if (prirep.has_value() && *prirep != "p") {
      continue;
    }

    const auto* docs_value = moirai::find_member(shard, "docs");
    const auto* store_value = moirai::find_member(shard, "store");
    if (docs_value == nullptr || store_value == nullptr) {
      continue;
    }

    const auto docs = integer_from_json(*docs_value);
    const auto store = decimal_from_json(*store_value);
    if (!docs.has_value() || !store.has_value()) {
      continue;
    }

    min_docs = std::min(min_docs, *docs);
    max_docs = std::max(max_docs, *docs);
    const auto rounded_store = static_cast<std::int64_t>(*store);
    min_store = std::min(min_store, rounded_store);
    max_store = std::max(max_store, rounded_store);
    ++primary_shards;
  }

  if (primary_shards < 2 || (max_docs == 0 && max_store == 0)) {
    return;
  }

  const auto docs_ratio =
    min_docs == 0 ? std::numeric_limits<double>::infinity()
                  : static_cast<double>(max_docs) / static_cast<double>(min_docs);
  const auto store_ratio =
    min_store == 0 ? std::numeric_limits<double>::infinity()
                   : static_cast<double>(max_store) /
                       static_cast<double>(min_store);
  const auto skew_ratio = std::max(docs_ratio, store_ratio);

  if (skew_ratio >= m_index_config.shard_critical_ratio) {
    app.logger().error(
      "Search shard skew critical for {}: ratio {:.2f}, docs {}-{}, store {}-{}",
      m_search_index,
      skew_ratio,
      min_docs,
      max_docs,
      min_store,
      max_store);
  } else if (skew_ratio >= m_index_config.shard_warning_ratio) {
    app.logger().information(
      "Search shard skew warning for {}: ratio {:.2f}, docs {}-{}, store {}-{}",
      m_search_index,
      skew_ratio,
      min_docs,
      max_docs,
      min_store,
      max_store);
  }
}

void
SearchWriter::ensure_index_ready()
{
  if (m_index_ready) {
    return;
  }

  static std::mutex ready_lock;
  static std::unordered_set<std::string> ready_indexes;
  std::unique_lock guard(ready_lock, std::defer_lock);
  std::string ready_key;
  if (m_shared_index_guard) {
    ready_key = m_uri.str() + "/" + m_search_index;
    guard.lock();
    if (ready_indexes.contains(ready_key)) {
      m_index_ready = true;
      return;
    }
  }

  const auto index_uri = moirai::append_path(m_uri, "/" + m_search_index);
  const auto response =
    m_http_request("HEAD", index_uri, {}, authorization_headers());
  if (response.status_code == HTTP_STATUS_NOT_FOUND) {
    create_index();
  } else if (response.status_code == HTTP_STATUS_OK) {
    validate_index_definition();
    check_shard_balance();
  } else {
    throw std::runtime_error(
      std::format("Unable to inspect search index {}: HTTP {} {}",
                  m_search_index,
                  response.status_code,
                  response.body));
  }

  if (m_shared_index_guard) {
    ready_indexes.insert(ready_key);
  }
  m_index_ready = true;
}

void
SearchWriter::run(const stop_token& stop_token)
{
  auto& app = moirai::Application::instance();
  ensure_index_ready();
  BulkMetrics metrics;
  AuditSink audit_sink{m_index_config};

  auto report_metrics = [&]() -> void {
    if (m_index_config.metrics_interval.count() == 0) {
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - metrics.last_report < m_index_config.metrics_interval) {
      return;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           now - metrics.last_report)
                           .count();
    if (elapsed <= 0) {
      return;
    }
    app.logger().information(
      "Search writer metrics: audited={} indexed={} failed={} retried={} bulk_requests={} "
      "uploaded_bytes={} records_per_sec={} bytes_per_sec={} queue_depth={}",
      metrics.audited_records,
      metrics.indexed_records,
      metrics.failed_records,
      metrics.retried_records,
      metrics.bulk_requests,
      metrics.uploaded_bytes,
      metrics.indexed_records / static_cast<std::uint64_t>(elapsed),
      metrics.uploaded_bytes / static_cast<std::uint64_t>(elapsed),
      m_solution_queue.size_approx());
    metrics.last_report = now;
    metrics.audited_records = 0;
    metrics.indexed_records = 0;
    metrics.failed_records = 0;
    metrics.retried_records = 0;
    metrics.uploaded_bytes = 0;
    metrics.bulk_requests = 0;
  };

  std::size_t target_bulk_records =
    m_index_config.bulk_adaptive ? m_index_config.bulk_min_records
                                 : m_index_config.bulk_max_records;
  const auto reduce_bulk_target = [&]() -> void {
    if (!m_index_config.bulk_adaptive) {
      return;
    }
    target_bulk_records =
      std::max(m_index_config.bulk_min_records,
               std::max<std::size_t>(1, target_bulk_records / 2U));
  };
  const auto increase_bulk_target = [&]() -> void {
    if (!m_index_config.bulk_adaptive ||
        target_bulk_records >= m_index_config.bulk_max_records) {
      return;
    }
    target_bulk_records =
      std::min(m_index_config.bulk_max_records,
               target_bulk_records + m_index_config.bulk_min_records);
  };

  auto upload_batch = [&](std::vector<BulkDocument> documents) -> void {
    if (documents.empty()) {
      return;
    }

    const auto ids = document_ids(documents);
    metrics.attempted_records += documents.size();
    std::size_t attempt = 0;
    std::string payload;
    while (!documents.empty()) {
      const auto compressed = m_index_config.bulk_gzip;
      const auto request_body = [&]() -> std::string {
        if (compressed) {
          return gzip_bulk_payload(documents,
                                   m_search_index,
                                   m_index_config.bulk_gzip_level);
        }
        append_bulk_payload(payload, documents, m_search_index);
        return payload;
      }();
      const auto started = std::chrono::steady_clock::now();

      try {
        const auto response = m_http_request(
          "POST",
          moirai::append_path(m_uri, "/_bulk"),
          request_body,
          bulk_headers(compressed));
        const auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count();
        ++metrics.bulk_requests;
        metrics.uploaded_bytes += request_body.size();

        if (response.status_code == HTTP_STATUS_OK ||
            response.status_code == HTTP_STATUS_CREATED) {
          const auto failures =
            parse_bulk_item_failures(documents, response.body);
          if (!failures.has_errors) {
            metrics.indexed_records += documents.size();
            if (elapsed_ms <= m_index_config.bulk_target_latency.count()) {
              increase_bulk_target();
            } else {
              reduce_bulk_target();
            }
            app.logger().information(
              "Pushed {} records ({} bytes, {} ms, target_batch={}). Sample ids: {}",
              documents.size(),
              request_body.size(),
              elapsed_ms,
              target_bulk_records,
              sample_ids(ids, m_index_config.log_sample_ids));
            return;
          }

          const auto succeeded =
            documents.size() - failures.permanent_failures -
            failures.retryable.size();
          metrics.indexed_records += succeeded;
          metrics.failed_records += failures.permanent_failures;
          if (!failures.retryable.empty() &&
              attempt < m_index_config.bulk_max_retries) {
            reduce_bulk_target();
            metrics.retried_records += failures.retryable.size();
            app.logger().error(
              "Bulk response had {} retryable and {} permanent failures; "
              "retrying attempt {}",
              failures.retryable.size(),
              failures.permanent_failures,
              attempt + 1);
            documents = failures.retryable;
          } else {
            reduce_bulk_target();
            metrics.failed_records += failures.retryable.size();
            app.logger().error(
              "Bulk response had {} permanent failures and {} exhausted "
              "retryable failures",
              failures.permanent_failures,
              failures.retryable.size());
            return;
          }
        } else if (is_retryable_status(response.status_code) &&
                   attempt < m_index_config.bulk_max_retries) {
          reduce_bulk_target();
          metrics.retried_records += documents.size();
          app.logger().error("Retryable bulk HTTP {} for {} records; attempt {}",
                             response.status_code,
                             documents.size(),
                             attempt + 1);
        } else {
          reduce_bulk_target();
          metrics.failed_records += documents.size();
          app.logger().error("Error uploading {} records: HTTP {} {}",
                             documents.size(),
                             response.status_code,
                             response.body);
          return;
        }
      } catch (const std::exception& exc) {
        if (attempt < m_index_config.bulk_max_retries) {
          reduce_bulk_target();
          metrics.retried_records += documents.size();
          app.logger().error("Retryable bulk exception for {} records: {}",
                             documents.size(),
                             exc.what());
        } else {
          reduce_bulk_target();
          metrics.failed_records += documents.size();
          app.logger().error("Error pushing {} records: {}",
                             documents.size(),
                             exc.what());
          return;
        }
      }

      ++attempt;
      const auto delay = std::chrono::milliseconds{
        static_cast<int>(100 * (1U << std::min<std::size_t>(attempt, 5)))
      };
      std::this_thread::sleep_for(delay);
    }
  };

  std::vector<SearchDocument> results(
    std::max<std::size_t>(1, m_index_config.bulk_max_records));
  while (true) {
    const size_t num_records =
      m_solution_queue.wait_dequeue_bulk(std::span(results), stop_token);
    if (num_records == 0) {
      if (m_solution_queue.closed()) {
        break;
      }
      continue;
    }

    std::vector<BulkDocument> documents;
    documents.reserve(num_records);
    std::size_t document_bytes = 0;
    const auto indexed_at = std::chrono::system_clock::now();
    const auto indexed_at_text = utc_timestamp(indexed_at);
    const auto indexed_at_seconds = epoch_seconds(indexed_at);
    for (std::size_t index = 0; index < num_records; ++index) {
      if (results[index].id.empty()) {
        app.logger().error("Invalid search payload without id");
        continue;
      }

      BulkDocument document{
        .id = std::move(results[index].id),
        .body = serialize_document_body(results[index],
                                        indexed_at_text,
                                        indexed_at_seconds),
      };
      audit_sink.write(document.body);
      ++metrics.audited_records;
      const auto estimated_bytes =
        document.id.size() + document.body.size() + m_search_index.size() + 64U;
      if (!documents.empty() &&
          (documents.size() >= target_bulk_records ||
           document_bytes + estimated_bytes > m_index_config.bulk_max_bytes)) {
        upload_batch(std::move(documents));
        documents.clear();
        document_bytes = 0;
      }
      document_bytes += estimated_bytes;
      documents.push_back(std::move(document));
    }

    upload_batch(std::move(documents));
    report_metrics();
  }
  audit_sink.close();
  report_metrics();
}
