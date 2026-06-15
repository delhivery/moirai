module;

#include <nlohmann/json.hpp>

export module moirai.json_utils;

export import std;

export namespace moirai {

using Json = nlohmann::json;

inline auto parse_json(std::string_view input) -> std::optional<Json> {
  auto parsed = Json::parse(input, nullptr, false);
  if (parsed.is_discarded()) {
    return std::nullopt;
  }

  return parsed;
}

inline auto parse_json(std::istream& input) -> std::optional<Json> {
  auto parsed = Json::parse(input, nullptr, false);
  if (parsed.is_discarded()) {
    return std::nullopt;
  }

  return parsed;
}

inline auto find_member(const Json& object, const char* key) -> const Json* {
  if (!object.is_object()) {
    return nullptr;
  }

  const auto iter = object.find(key);
  if (iter == object.end()) {
    return nullptr;
  }

  return &*iter;
}

inline auto find_object_member(const Json& object, const char* key)
    -> const Json* {
  const auto* value = find_member(object, key);
  if (value == nullptr || !value->is_object()) {
    return nullptr;
  }

  return value;
}

inline auto find_array_member(const Json& object, const char* key)
    -> const Json* {
  const auto* value = find_member(object, key);
  if (value == nullptr || !value->is_array()) {
    return nullptr;
  }

  return value;
}

inline auto get_string(const Json& value) -> std::optional<std::string_view> {
  if (!value.is_string()) {
    return std::nullopt;
  }

  return value.template get_ref<const std::string&>();
}

inline auto find_string_member(const Json& object, const char* key)
    -> std::optional<std::string_view> {
  const auto* value = find_member(object, key);
  if (value == nullptr) {
    return std::nullopt;
  }

  return get_string(*value);
}

template <typename Integer>
inline auto parse_integer(std::string_view input) -> std::optional<Integer> {
  static_assert(std::is_integral_v<Integer>);

  Integer value{};
  const auto* begin = input.data();
  const auto* end = begin + input.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || ptr != end) {
    return std::optional<Integer>{};
  }

  return value;
}

template <typename Integer>
inline auto get_integer(const Json& value) -> std::optional<Integer> {
  static_assert(std::is_integral_v<Integer>);

  if (value.is_number_integer()) {
    const auto number = value.template get<std::int64_t>();
    if (number < std::numeric_limits<Integer>::min() ||
        number > std::numeric_limits<Integer>::max()) {
      return std::optional<Integer>{};
    }
    return static_cast<Integer>(number);
  }

  if (value.is_number_unsigned()) {
    const auto number = value.template get<std::uint64_t>();
    if (number > std::numeric_limits<Integer>::max()) {
      return std::optional<Integer>{};
    }
    return static_cast<Integer>(number);
  }

  const auto string_value = get_string(value);
  if (!string_value.has_value()) {
    return std::optional<Integer>{};
  }

  return parse_integer<Integer>(*string_value);
}

template <typename Integer>
inline auto find_integer_member(const Json& object, const char* key)
    -> std::optional<Integer> {
  const auto* value = find_member(object, key);
  if (value == nullptr) {
    return std::optional<Integer>{};
  }

  return get_integer<Integer>(*value);
}

} // namespace moirai
