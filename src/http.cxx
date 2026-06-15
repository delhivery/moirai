module;

#include <curl/curl.h>

module moirai.http;

import std;

namespace {

class CurlGlobal
{
public:
  CurlGlobal()
  {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      throw std::runtime_error("Failed to initialize libcurl");
    }
  }

  ~CurlGlobal() { curl_global_cleanup(); }
};

auto
curl_global_state() -> CurlGlobal&
{
  static CurlGlobal state;
  return state;
}

auto
write_callback(char* buffer, size_t size, size_t nmemb, void* userdata)
  -> size_t
{
  auto* output = static_cast<std::string*>(userdata);
  output->append(buffer, size * nmemb);
  return size * nmemb;
}

auto
url_encode(CURL* curl, const std::string& value) -> std::string
{
  if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("URL parameter is too large to encode");
  }
  char* escaped =
    curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
  if (escaped == nullptr) {
    throw std::runtime_error("Failed to encode URL parameter");
  }
  std::string output(escaped);
  curl_free(escaped);
  return output;
}

void
cleanup_curl(void* handle)
{
  if (handle != nullptr) {
    curl_easy_cleanup(static_cast<CURL*>(handle));
  }
}

auto
perform_request(CURL* curl,
                std::string_view method,
                const moirai::Uri& uri,
                const std::vector<std::string>& headers,
                std::string_view body) -> moirai::HttpResponse
{
  (void)curl_global_state();

  if (curl == nullptr) {
    throw std::runtime_error("Invalid CURL easy handle");
  }

  moirai::HttpResponse response;
  curl_slist* header_list = nullptr;
  std::string request_body;

  curl_easy_reset(curl);
  for (const auto& header : headers) {
    header_list = curl_slist_append(header_list, header.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, uri.str().c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L);
  curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

  if (header_list != nullptr) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  }

  const std::string method_name{ method };
  if (method_name == "HEAD") {
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  } else if (method_name == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else if (method_name != "GET") {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_name.c_str());
  }

  if (method_name != "GET" && method_name != "HEAD") {
    request_body.assign(body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(
      curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.size()));
  }

  const CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    std::string message = curl_easy_strerror(result);
    curl_slist_free_all(header_list);
    throw std::runtime_error(message);
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);

  curl_slist_free_all(header_list);
  return response;
}

} // namespace

namespace moirai {
namespace {

constexpr std::uint16_t HTTPS_DEFAULT_PORT = 443;
constexpr std::uint16_t HTTP_DEFAULT_PORT = 80;

} // namespace

auto
Uri::path_and_query() const -> std::string
{
  if (query.empty()) {
    return path;
  }
  return path + "?" + query;
}

auto
Uri::str() const -> std::string
{
  return scheme + "://" + host + ":" + std::to_string(port) + path_and_query();
}

HttpClient::HttpClient()
{
  (void)curl_global_state();
  CURL* handle = curl_easy_init();
  if (handle == nullptr) {
    throw std::runtime_error("Failed to create CURL easy handle");
  }
  m_handle = std::shared_ptr<void>(handle, cleanup_curl);
}

auto
HttpClient::request(std::string_view method,
                    const Uri& uri,
                    std::string_view body,
                    const std::vector<std::string>& headers) -> HttpResponse
{
  return perform_request(
    static_cast<CURL*>(m_handle.get()), method, uri, headers, body);
}

auto
HttpClient::get(const Uri& uri, const std::vector<std::string>& headers)
  -> HttpResponse
{
  return request("GET", uri, {}, headers);
}

auto
HttpClient::post(const Uri& uri,
                 std::string_view body,
                 const std::vector<std::string>& headers) -> HttpResponse
{
  return request("POST", uri, body, headers);
}

auto
parse_uri(const std::string& input) -> Uri
{
  const auto scheme_end = input.find("://");
  if (scheme_end == std::string::npos) {
    throw std::runtime_error("URI is missing scheme");
  }

  Uri uri;
  uri.scheme = input.substr(0, scheme_end);
  std::string remainder = input.substr(scheme_end + 3);

  const auto path_start = remainder.find('/');
  std::string authority = path_start == std::string::npos
                            ? remainder
                            : remainder.substr(0, path_start);
  std::string suffix = path_start == std::string::npos
                         ? std::string{}
                         : remainder.substr(path_start);

  const auto query_start = suffix.find('?');
  uri.path =
    query_start == std::string::npos ? suffix : suffix.substr(0, query_start);
  uri.query = query_start == std::string::npos ? std::string{}
                                               : suffix.substr(query_start + 1);

  if (uri.path.empty()) {
    uri.path = "/";
  }

  const auto port_start = authority.rfind(':');
  if (port_start != std::string::npos) {
    uri.host = authority.substr(0, port_start);
    uri.port =
      static_cast<std::uint16_t>(std::stoul(authority.substr(port_start + 1)));
  } else {
    uri.host = authority;
    uri.port = uri.scheme == "https" ? HTTPS_DEFAULT_PORT : HTTP_DEFAULT_PORT;
  }

  if (uri.host.empty()) {
    throw std::runtime_error("URI is missing host");
  }

  return uri;
}

auto
with_query_parameters(
  const Uri& base,
  const std::vector<std::pair<std::string, std::string>>& parameters) -> Uri
{
  (void)curl_global_state();

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    throw std::runtime_error(
      "Failed to create CURL easy handle for query encoding");
  }

  Uri uri = base;
  std::string query = uri.query;

  for (const auto& [key, value] : parameters) {
    if (!query.empty()) {
      query += "&";
    }
    query += url_encode(curl, key) + "=" + url_encode(curl, value);
  }

  curl_easy_cleanup(curl);
  uri.query = query;
  return uri;
}

auto
append_path(const Uri& base, std::string_view suffix) -> Uri
{
  Uri uri = base;

  if (uri.path.empty()) {
    uri.path = "/";
  }

  if (uri.path.ends_with('/') && suffix.starts_with('/')) {
    uri.path += suffix.substr(1);
  } else if (!uri.path.ends_with('/') && !suffix.starts_with('/')) {
    uri.path += "/";
    uri.path += suffix;
  } else {
    uri.path += suffix;
  }

  return uri;
}

auto
http_request(std::string_view method,
             const Uri& uri,
             std::string_view body,
             const std::vector<std::string>& headers) -> HttpResponse
{
  HttpClient client;
  return client.request(method, uri, body, headers);
}

auto
http_get(const Uri& uri, const std::vector<std::string>& headers)
  -> HttpResponse
{
  return http_request("GET", uri, {}, headers);
}

auto
http_post(const Uri& uri,
          std::string_view body,
          const std::vector<std::string>& headers) -> HttpResponse
{
  return http_request("POST", uri, body, headers);
}

} // namespace moirai
