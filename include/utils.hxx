#ifndef MOIRAI_UTILS
#define MOIRAI_UTILS

#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20
#endif

#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 1
#endif

// #include <boost/bimap.hpp>
// #include <nlohmann/json.hpp>
// #include <numeric>
// #include <ranges>
#include <string>
#include <string_view>

// using StringToStringMap = boost::bimap<std::string, std::string>;

/*
auto
indexAndTypeToPath(const std::string&, const std::string&) -> std::string;

auto
indexAndTypeToPath(const std::string&, const std::string&, const std::string&)
  -> std::string;
*/

auto getEncodedCredentials(std::string_view, std::string_view) -> std::string;

void
to_lower(std::string&);

/*
template<std::ranges::range range_t>
constexpr auto
to_vector(range_t&& range)
  -> std::vector<std::decay_t<std::ranges::range_value_t<range_t>>>
{
  using value_t = std::decay_t<std::ranges::range_value_t<range_t>>;
  return std::vector<value_t>(range.begin(), range.end());
}

template<typename T>
auto
getJSONValue(const nlohmann::json& data, std::string_view key_string) -> T
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
/*
}

template<typename T>
auto
getJSONValue(const nlohmann::json& data, size_t index) -> T
{
  return data.at(index).get<T>();
}
*/

#endif
