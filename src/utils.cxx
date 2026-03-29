#include "utils.hxx"
#include <openssl/evp.h>

#include <algorithm>
#include <format>
#include <stdexcept>

auto TopicMap::insert(const std::string &role, const std::string &topic)
    -> bool {
  auto role_it = m_role_to_topic.find(role);
  if (role_it != m_role_to_topic.end()) {
    m_topic_to_role.erase(role_it->second);
  }

  auto topic_it = m_topic_to_role.find(topic);
  if (topic_it != m_topic_to_role.end()) {
    m_role_to_topic.erase(topic_it->second);
  }

  m_role_to_topic[role] = topic;
  m_topic_to_role[topic] = role;
  return true;
}

auto TopicMap::contains_role(const std::string &role) const -> bool {
  return m_role_to_topic.contains(role);
}

auto TopicMap::contains_topic(const std::string &topic) const -> bool {
  return m_topic_to_role.contains(topic);
}

auto TopicMap::topic_for(const std::string &role) const -> const std::string & {
  if (const auto iter = m_role_to_topic.find(role);
      iter != m_role_to_topic.end()) {
    return iter->second;
  }

  throw std::out_of_range(std::format("Unknown topic role {}", role));
}

auto TopicMap::role_for(const std::string &topic) const -> const std::string & {
  if (const auto iter = m_topic_to_role.find(topic);
      iter != m_topic_to_role.end()) {
    return iter->second;
  }

  throw std::out_of_range(std::format("Unknown topic {}", topic));
}

auto TopicMap::topics() const -> std::vector<std::string> {
  std::vector<std::string> result;
  result.reserve(m_role_to_topic.size());
  for (const auto &[role, topic] : m_role_to_topic) {
    (void)role;
    result.push_back(topic);
  }

  return result;
}

auto index_and_type_to_path(const std::string &index_name,
                            const std::string &type_name) -> std::string {
  return std::format("/{}/{}/", index_name, type_name);
}

auto index_and_type_to_path(const std::string &index_name,
                            const std::string &type_name,
                            const std::string &document_id) -> std::string {
  return std::format("/{}/{}/{}/", index_name, type_name, document_id);
}

auto get_encoded_credentials(const std::string &username,
                             const std::string &password) -> std::string {
  const std::string credentials = username + ":" + password;
  std::string encoded(4 * ((credentials.size() + 2) / 3), '\0');
  const auto *input =
      reinterpret_cast<const unsigned char *>(credentials.data());
  auto *output = reinterpret_cast<unsigned char *>(encoded.data());
  const int length =
      EVP_EncodeBlock(output, input, static_cast<int>(credentials.size()));
  encoded.resize(length);
  return encoded;
}

void to_lower(std::string &value) {
  std::ranges::transform(value, value.begin(), [](unsigned char chr) -> int {
    return std::tolower(chr);
  });
}
