#ifndef MOIRAI_UTILS
#define MOIRAI_UTILS

#include <boost/bimap.hpp>
#include <nlohmann/json.hpp>
#include <numeric>
#include <ranges>
#include <string>
#include <string_view>

typedef boost::bimap<std::string, std::string> StringToStringMap;

std::string
indexAndTypeToPath(const std::string&, const std::string&);

std::string
indexAndTypeToPath(const std::string&, const std::string&, const std::string&);

std::string
getEncodedCredentials(const std::string&, const std::string&);

void
to_lower(std::string&);

template<typename T>
T
getJSONValue(const nlohmann::json& data, std::string_view key_string)
{
  constexpr std::string_view delim{ "." };
  auto value = data;

  for (const auto token : std::views::split(key_string, delim)) {
    std::string key(token.begin(), token.end());
    value = value[key];
  }

  return value.template get<T>();
  /*
    const auto value = std::accumulate(
      keys.begin(), keys.end(), data, [](const auto data, const auto iter_key) {
        std::string_view key{ iter_key.begin(), iter_key.end() };
        return data[key];
      });
    return value.get<T>();
  */
}

template<typename T>
T
getJSONValue(const nlohmann::json& data, size_t index)
{
  return data.at(index).get<T>();
}

#endif
