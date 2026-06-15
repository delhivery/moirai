export module moirai.search_document;

export import std;
export import moirai.date_utils;

export struct SearchPathLocation {
  std::string code;
  std::string arrival;
  std::int64_t arrival_ts{};
  std::string route;
  std::string departure;
  std::int64_t departure_ts{};
  bool has_departure{false};
};

export struct SearchPathSection {
  std::vector<SearchPathLocation> locations;
};

export struct SearchDocument {
  std::string id;
  std::string waybill;
  std::string package_id;
  std::string cs_slid;
  std::string cs_act;
  std::string pid;
  std::string fail;
  std::string pdd;
  std::int64_t pdd_ts{};
  SearchPathSection earliest;
  SearchPathSection ultimate;

  [[nodiscard]] auto failed() const -> bool { return !fail.empty(); }
};
