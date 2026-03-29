#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace moirai {

struct Uri {
  std::string scheme;
  std::string host;
  std::uint16_t port;
  std::string path;
  std::string query;

  [[nodiscard]] auto path_and_query() const -> std::string;
  [[nodiscard]] auto str() const -> std::string;
};

struct HttpResponse {
  long status_code{0};
  std::string body;
};

auto parse_uri(const std::string &input) -> Uri;

auto with_query_parameters(
    const Uri &base,
    const std::vector<std::pair<std::string, std::string>> &parameters) -> Uri;

auto append_path(const Uri &base, std::string_view suffix) -> Uri;

auto http_get(const Uri &uri, const std::vector<std::string> &headers = {})
    -> HttpResponse;

auto http_post(const Uri &uri, std::string_view body,
               const std::vector<std::string> &headers = {}) -> HttpResponse;

} // namespace moirai
