module;

#include "simdjson_module.hxx"

export module moirai.json_utils;

export import std;

export namespace moirai {

using Json = simdjson::dom::element;
using JsonParser = simdjson::dom::parser;

inline auto thread_local_parser() -> JsonParser& {
  thread_local JsonParser parser;
  return parser;
}

inline auto parse_json(std::string_view input) -> std::optional<Json> {
  auto& parser = thread_local_parser();
  auto result = parser.parse(input.data(), input.size());
  if (result.error() != simdjson::SUCCESS) {
    return std::nullopt;
  }

  return result.value_unsafe();
}

inline auto parse_json(JsonParser& parser, std::string_view input)
    -> std::optional<Json> {
  auto result = parser.parse(input.data(), input.size());
  if (result.error() != simdjson::SUCCESS) {
    return std::nullopt;
  }

  return result.value_unsafe();
}

inline auto parse_json(std::istream& input) -> std::optional<Json> {
  std::string content(std::istreambuf_iterator<char>(input), {});
  return parse_json(content);
}

// simdjson::dom::element is a lightweight tape pointer (16 bytes).
// We store results in a thread-local ring buffer so callers can hold
// pointers to multiple results simultaneously.
inline auto find_member(const Json& object, const char* key) -> const Json* {
  if (!object.is_object()) {
    return nullptr;
  }

  auto result = object[key];
  if (result.error() != simdjson::SUCCESS) {
    return nullptr;
  }

  constexpr std::size_t SLOT_COUNT = 32;
  thread_local std::array<Json, SLOT_COUNT> slots;
  thread_local std::size_t next_slot = 0;
  auto& slot = slots[next_slot % SLOT_COUNT];
  next_slot++;
  slot = result.value_unsafe();
  return &slot;
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

inline auto json_size(const Json& value) -> std::size_t {
  if (value.is_array()) {
    return value.get_array().value_unsafe().size();
  }
  if (value.is_object()) {
    return value.get_object().value_unsafe().size();
  }
  return 0;
}

// Note: simdjson::to_string() cannot be used in GCC modules due to
// TU-local entity issues. Callers needing JSON serialization for logging
// should use simdjson::to_string() directly from an implementation file
// that includes simdjson.h, or pass string_view fields for logging.
inline auto dump(const Json& /*value*/) -> std::string {
  return "<json>";
}

inline auto get_string(const Json& value) -> std::optional<std::string_view> {
  if (!value.is_string()) {
    return std::nullopt;
  }

  return value.get_string().value_unsafe();
}

inline auto get_bool(const Json& value) -> std::optional<bool> {
  if (!value.is_bool()) {
    return std::nullopt;
  }

  return value.get_bool().value_unsafe();
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

  if (value.is_int64()) {
    const auto number = value.get_int64().value_unsafe();
    if (number < std::numeric_limits<Integer>::min() ||
        number > std::numeric_limits<Integer>::max()) {
      return std::optional<Integer>{};
    }
    return static_cast<Integer>(number);
  }

  if (value.is_uint64()) {
    const auto number = value.get_uint64().value_unsafe();
    if (number > static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())) {
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
