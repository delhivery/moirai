#include "utils.hxx"
#include "base64.hxx"
#include "fmt/format.h"
#include <sstream>

auto getEncodedCredentials(std::string_view username, std::string_view password)
    -> std::string {
  return encode(std::string_view{fmt::format("{}:{}", username, password)});
}
