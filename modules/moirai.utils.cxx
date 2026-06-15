export module moirai.utils;

export import std;

export struct TopicMap {
  auto insert(const std::string& role, const std::string& topic) -> bool;

  [[nodiscard]] auto contains_role(const std::string& role) const -> bool;
  [[nodiscard]] auto contains_topic(const std::string& topic) const -> bool;

  [[nodiscard]] auto topic_for(const std::string& role) const
      -> const std::string&;
  [[nodiscard]] auto role_for(const std::string& topic) const
      -> const std::string&;
  [[nodiscard]] auto topics() const -> std::vector<std::string>;

private:
  std::unordered_map<std::string, std::string> m_role_to_topic;
  std::unordered_map<std::string, std::string> m_topic_to_role;
};

export auto index_and_type_to_path(const std::string& index_name,
                                   const std::string& type_name)
    -> std::string;

export auto index_and_type_to_path(const std::string& index_name,
                                   const std::string& type_name,
                                   const std::string& document_id)
    -> std::string;

export auto get_encoded_credentials(const std::string& username,
                                    const std::string& password)
    -> std::string;

export void to_lower(std::string& value);
