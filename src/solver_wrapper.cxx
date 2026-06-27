module;

#include "blocking_queue.hxx"

module moirai.solver_wrapper;

import std;
import moirai.app;
import moirai.date_utils;
import moirai.http;
import moirai.json_utils;
import moirai.processor;
import moirai.route_schedule;
import moirai.search_document;
import moirai.solver;
import moirai.transportation;
import moirai.utils;

namespace {

constexpr long HTTP_STATUS_OK = 200;
constexpr auto IST_OFFSET = DURATION{ 330 };
constexpr std::size_t SOLVER_BATCH_SIZE = 64;
constexpr std::size_t MIN_IPDD_LENGTH = 18;
constexpr std::string_view ROUTE_EXPANSION_THREADS_ENV =
  "MOIRAI_ROUTE_EXPANSION_THREADS";
constexpr std::string_view FACILITY_DETAIL_THREADS_ENV =
  "MOIRAI_FACILITY_DETAIL_THREADS";
constexpr std::string_view PATH_CACHE_ENABLED_ENV = "MOIRAI_PATH_CACHE_ENABLED";
constexpr std::string_view PATH_CACHE_MAX_ENTRIES_ENV =
  "MOIRAI_PATH_CACHE_MAX_ENTRIES";
constexpr std::string_view PATH_CACHE_BUCKET_MINUTES_ENV =
  "MOIRAI_PATH_CACHE_BUCKET_MINUTES";
using PackageInfo = std::tuple<std::string, std::int32_t, std::string>;


struct LoadPayload {
  std::string id;
  std::string location;
  std::string destination;
  std::string origin_center;
  std::string event_time;
  std::string ipdd_destination;
  std::string cs_slid;
  std::string cs_act;
  std::string pid;
  std::optional<moirai::Json> items;
};

struct FacilityListEntry {
  std::string code;
  std::string name;
  std::string property_id;
  std::optional<std::string> id;
  SolverWrapper::FacilityProfile profile;
};

struct WrapperScratch {
  std::vector<PackageInfo> packages;
};

thread_local WrapperScratch wrapper_scratch;

auto milliseconds_since(std::chrono::steady_clock::time_point start,
                        std::chrono::steady_clock::time_point end)
    -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
    .count();
}

auto parse_bool_env(std::string_view name, bool fallback) -> bool {
  const char* value = std::getenv(std::string(name).c_str());
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }
  std::string input{value};
  std::ranges::transform(input, input.begin(), [](unsigned char chr) {
    return static_cast<char>(std::tolower(chr));
  });
  if (input == "1" || input == "true" || input == "yes" || input == "on") {
    return true;
  }
  if (input == "0" || input == "false" || input == "no" || input == "off") {
    return false;
  }
  throw std::runtime_error(std::format("Invalid {} value '{}'", name, input));
}

auto parse_size_env(std::string_view name, std::size_t fallback,
                    bool allow_zero = false) -> std::size_t {
  const char* value = std::getenv(std::string(name).c_str());
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }
  std::size_t parsed{};
  const std::string_view input{value};
  const auto [ptr, error] =
    std::from_chars(input.data(), input.data() + input.size(), parsed);
  if (error != std::errc{} || ptr != input.data() + input.size() ||
      (!allow_zero && parsed == 0)) {
    throw std::runtime_error(std::format("Invalid {} value '{}'", name, input));
  }
  return parsed;
}

auto path_cache_config_from_environment() -> PathCacheConfig {
  PathCacheConfig cache;
  cache.enabled = parse_bool_env(PATH_CACHE_ENABLED_ENV, cache.enabled);
  cache.max_entries =
    parse_size_env(PATH_CACHE_MAX_ENTRIES_ENV, cache.max_entries, true);
  cache.bucket_minutes = static_cast<std::uint32_t>(
    parse_size_env(PATH_CACHE_BUCKET_MINUTES_ENV, cache.bucket_minutes));
  if (cache.max_entries == 0) {
    cache.enabled = false;
  }
  return cache;
}

auto optional_string(const moirai::Json& object, const char* key)
    -> std::string {
  const auto value = moirai::find_string_member(object, key);
  return value.has_value() ? std::string(*value) : std::string{};
}

auto extract_load_payload(const moirai::Json& data)
    -> std::expected<LoadPayload, std::string> {
  const auto bag_identifier = moirai::find_string_member(data, "id");
  const auto current_location = moirai::find_string_member(data, "location");
  const auto item_destination =
    moirai::find_string_member(data, "destination");
  const auto event_time = moirai::find_string_member(data, "time");
  if (!bag_identifier.has_value() || !current_location.has_value() ||
      !item_destination.has_value() || !event_time.has_value()) {
    return std::unexpected{"missing id, location, destination, or time"};
  }

  const auto* items_ptr = moirai::find_array_member(data, "items");
  if (items_ptr == nullptr) {
    items_ptr = moirai::find_array_member(data, "item");
  }

  return LoadPayload{
    .id = std::string(*bag_identifier),
    .location = std::string(*current_location),
    .destination = std::string(*item_destination),
    .origin_center = optional_string(data, "oc"),
    .event_time = std::string(*event_time),
    .ipdd_destination = optional_string(data, "ipdd_destination"),
    .cs_slid = optional_string(data, "cs_slid"),
    .cs_act = optional_string(data, "cs_act"),
    .pid = optional_string(data, "pid"),
    .items = items_ptr != nullptr ? std::optional<moirai::Json>(*items_ptr)
                                  : std::nullopt,
  };
}

auto parse_facility_duration(const moirai::Json& object,
                             const char* key) -> std::optional<DURATION>
{
  if (const auto parsed_integer =
        moirai::find_integer_member<int16_t>(object, key);
      parsed_integer.has_value()) {
    return DURATION{*parsed_integer};
  }

  const auto string_value = moirai::find_string_member(object, key);
  if (!string_value.has_value() || string_value->find(':') == std::string_view::npos) {
    return std::nullopt;
  }

  const auto duration = time_string_to_time(*string_value);
  const auto minutes = duration.count();
  if (minutes < std::numeric_limits<int16_t>::min() ||
      minutes > std::numeric_limits<int16_t>::max()) {
    return std::nullopt;
  }

  return DURATION{static_cast<int16_t>(minutes)};
}

auto parse_utc_cutoff(const moirai::Json& object,
                      const char* key) -> std::optional<TIME_OF_DAY>
{
  const auto value = parse_facility_duration(object, key);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return TIME_OF_DAY{
    static_cast<std::int16_t>(datemod(*value - IST_OFFSET, DURATION{24 * 60}))
  };
}

auto facility_identifier(const moirai::Json& object) -> std::optional<std::string>
{
  if (const auto id = moirai::find_string_member(object, "id");
      id.has_value() && !id->empty()) {
    return std::string(*id);
  }
  if (const auto id = moirai::find_integer_member<std::int64_t>(object, "id");
      id.has_value()) {
    return std::to_string(*id);
  }
  return std::nullopt;
}

auto parse_facility_profile(const moirai::Json& object)
    -> SolverWrapper::FacilityProfile
{
  SolverWrapper::FacilityProfile profile;
  const auto* result = moirai::find_object_member(object, "result");
  const auto* attributes =
    result == nullptr ? nullptr
                      : moirai::find_object_member(*result,
                                                   "facility_attributes");
  if (attributes == nullptr) {
    attributes = moirai::find_object_member(object, "facility_attributes");
  }
  if (attributes == nullptr) {
    attributes = result != nullptr ? result : &object;
  }

  if (const auto outbound =
        parse_facility_duration(*attributes, "OutboundProcessingTime");
      outbound.has_value()) {
    profile.outbound_processing = *outbound;
  }
  if (const auto fresh =
        parse_facility_duration(*attributes, "FreshShipmentProcessingTime");
      fresh.has_value()) {
    profile.fresh_processing = *fresh;
  }
  if (const auto mixed =
        parse_facility_duration(*attributes, "MixedBagProcessingTime");
      mixed.has_value()) {
    profile.mixed_bag_processing = *mixed;
  }
  if (const auto cutoff =
        parse_utc_cutoff(*attributes, "CenterArrivalCutoff");
      cutoff.has_value()) {
    profile.center_arrival_cutoff = *cutoff;
  }
  return profile;
}

auto profile_for(const std::shared_ptr<SolverWrapper::FacilityProfiles>& profiles,
                 std::string_view code) -> SolverWrapper::FacilityProfile
{
  if (profiles == nullptr) {
    return {};
  }
  const auto found = profiles->find(std::string(code));
  return found == profiles->end() ? SolverWrapper::FacilityProfile{}
                                  : found->second;
}

auto parse_route_expansion_threads(std::size_t route_count) -> std::size_t {
  if (route_count == 0) {
    return 0;
  }

  const char* value = std::getenv(ROUTE_EXPANSION_THREADS_ENV.data());
  if (value != nullptr && std::string_view{value}.empty()) {
    value = nullptr;
  }

  std::size_t requested{};
  if (value != nullptr) {
    const std::string_view input{value};
    const auto [ptr, error] =
      std::from_chars(input.data(), input.data() + input.size(), requested);
    if (error != std::errc{} || ptr != input.data() + input.size() ||
        requested == 0) {
      throw std::runtime_error(
        std::format("Invalid {} value '{}'",
                    ROUTE_EXPANSION_THREADS_ENV,
                    input));
    }
  } else {
    requested = std::thread::hardware_concurrency();
    if (requested == 0) {
      requested = 1;
    }
  }

  return std::clamp(requested, std::size_t{1}, route_count);
}

auto parse_facility_detail_threads(std::size_t facility_count) -> std::size_t {
  if (facility_count == 0) {
    return 0;
  }

  const char* value = std::getenv(FACILITY_DETAIL_THREADS_ENV.data());
  if (value != nullptr && std::string_view{value}.empty()) {
    value = nullptr;
  }

  std::size_t requested{};
  if (value != nullptr) {
    const std::string_view input{value};
    const auto [ptr, error] =
      std::from_chars(input.data(), input.data() + input.size(), requested);
    if (error != std::errc{} || ptr != input.data() + input.size() ||
        requested == 0) {
      throw std::runtime_error(
        std::format("Invalid {} value '{}'",
                    FACILITY_DETAIL_THREADS_ENV,
                    input));
    }
  } else {
    requested = std::thread::hardware_concurrency();
    if (requested == 0) {
      requested = 1;
    }
    requested = std::min<std::size_t>(requested, 16);
  }

  return std::clamp(requested, std::size_t{1}, facility_count);
}

auto expand_route_specs(const moirai::Json& routes)
    -> std::vector<std::expected<std::vector<RouteEdgeSpec>, std::string>> {
  const auto route_count = moirai::json_size(routes);
  std::vector<std::expected<std::vector<RouteEdgeSpec>, std::string>> expanded(
    route_count);
  const auto worker_count = parse_route_expansion_threads(route_count);
  if (worker_count == 0) {
    return expanded;
  }

  // Build a vector of element pointers for indexed access
  std::vector<moirai::Json> route_elements;
  route_elements.reserve(route_count);
  for (const auto& route : routes) {
    route_elements.push_back(route);
  }

  const auto expand_one = [&route_elements, &expanded](std::size_t index) -> void {
    try {
      expanded[index] = build_route_edge_specs(route_elements[index], IST_OFFSET);
    } catch (const std::exception& exc) {
      expanded[index] = std::unexpected{
        std::format("route {}: {}", index, exc.what())
      };
    }
  };

  if (worker_count == 1) {
    for (std::size_t index = 0; index < route_count; ++index) {
      expand_one(index);
    }
    return expanded;
  }

  std::atomic_size_t next_route{0};
  std::vector<std::jthread> workers;
  workers.reserve(worker_count);
  for (std::size_t worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([&]() {
      while (true) {
        const auto index = next_route.fetch_add(1, std::memory_order_relaxed);
        if (index >= route_count) {
          return;
        }
        expand_one(index);
      }
    });
  }

  return expanded;
}

} // namespace

SolverWrapper::SolverWrapper(
  RuntimeQueues queues,
  const std::shared_ptr<Solver>& solver,
  const std::filesystem::path& center_timings_filename,
  std::shared_ptr<PathCache> cache,
  std::shared_ptr<FacilityProfiles> facility_profiles,
  HttpGet http_get)
  : m_solver(solver)
  , m_facility_profiles(facility_profiles != nullptr
                          ? std::move(facility_profiles)
                          : std::make_shared<FacilityProfiles>())
  , m_load_queue(*queues.load)
  , m_solution_queue(*queues.solution)
  , m_http_get(std::move(http_get))
  , m_path_cache(std::move(cache))
  , m_cache_config(path_cache_config_from_environment())
{
  if (!m_path_cache && m_cache_config.enabled) {
    m_path_cache = std::make_shared<PathCache>(m_cache_config.max_entries);
  }
  init_timings(center_timings_filename);
}

SolverWrapper::SolverWrapper(
  RuntimeQueues queues,
  InitEndpoints endpoints,
  const std::filesystem::path& center_timings_filename,
  HttpGet http_get)
  : m_node_init_uri(moirai::parse_uri(endpoints.node_uri))
  , m_node_init_auth_token(std::move(endpoints.node_token))
  , m_edge_init_uri(moirai::parse_uri(endpoints.edge_uri))
  , m_edge_init_auth_token(std::move(endpoints.edge_token))
  , m_facility_profiles(std::make_shared<FacilityProfiles>())
  , m_node_queue(queues.node)
  , m_edge_queue(queues.edge)
  , m_load_queue(*queues.load)
  , m_solution_queue(*queues.solution)
  , m_http_get(std::move(http_get))
{
  auto& app = moirai::Application::instance();
  const auto total_started = std::chrono::steady_clock::now();
  auto phase_started = total_started;
  const auto finish_phase = [&phase_started]() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = milliseconds_since(phase_started, now);
    phase_started = now;
    return elapsed;
  };

  m_cache_config = path_cache_config_from_environment();
  if (m_cache_config.enabled) {
    m_path_cache = std::make_shared<PathCache>(m_cache_config.max_entries);
  }

  m_solver = std::make_shared<Solver>();
  init_timings(center_timings_filename);
  const auto timings_ms = finish_phase();
  init_nodes();
  const auto nodes_ms = finish_phase();
  init_custody();
  const auto custody_ms = finish_phase();
  init_edges();
  const auto routes_ms = finish_phase();
  m_solver->finalize_graph();
  const auto finalize_ms = finish_phase();
  const auto stats = m_solver->graph_stats();
  app.logger().information(
    "Initialized graph: queue={} nodes={} edges={} csr_out={} csr_in={} "
    "avg_out_degree={} max_out_degree={}",
    stats.queue,
    stats.nodes,
    stats.edges,
    stats.outgoing_storage,
    stats.incoming_storage,
    stats.average_out_degree,
    stats.max_out_degree);
  app.logger().information(
    "Startup timings: timings_ms={} nodes_ms={} custody_ms={} routes_ms={} "
    "finalize_ms={} total_ms={} path_cache_enabled={} path_cache_max_entries={} "
    "path_cache_bucket_minutes={}",
    timings_ms,
    nodes_ms,
    custody_ms,
    routes_ms,
    finalize_ms,
    milliseconds_since(total_started, std::chrono::steady_clock::now()),
    m_cache_config.enabled,
    m_cache_config.max_entries,
    m_cache_config.bucket_minutes);
}

void
SolverWrapper::init_timings(
  const std::filesystem::path& facility_timings_filename)
{
  auto& app = moirai::Application::instance();
  if (!facility_timings_filename.empty()) {
    app.logger().debug(
      "Ignoring legacy facility timings file {}; using facility API profiles",
      facility_timings_filename.string());
  }
}

auto
SolverWrapper::get_solver() const -> std::shared_ptr<Solver>
{
  return m_solver;
}

auto
SolverWrapper::get_cache() const -> std::shared_ptr<PathCache>
{
  return m_path_cache;
}

auto
SolverWrapper::get_facility_profiles() const -> std::shared_ptr<FacilityProfiles>
{
  return m_facility_profiles;
}

void
SolverWrapper::init_nodes(int16_t page)
{
  auto& app = moirai::Application::instance();
  const auto request_uri = moirai::with_query_parameters(
    m_node_init_uri,
    { { "page", std::to_string(page) }, { "status", "active" } });
  const auto response = m_http_get(
    request_uri,
    { std::format("Authorization: Bearer {}", m_node_init_auth_token) });

  if (response.status_code == HTTP_STATUS_OK) {
    const auto response_json = moirai::parse_json(response.body);
    if (!response_json.has_value() || !response_json->is_object()) {
      app.logger().error("Unable to parse facility response");
      return;
    }

    const auto* result = moirai::find_object_member(*response_json, "result");
    const auto* data =
      result == nullptr ? nullptr : moirai::find_array_member(*result, "data");
    const auto pages =
      result == nullptr
        ? std::nullopt
        : moirai::find_integer_member<int16_t>(*result, "total_page_count");
    if (result == nullptr || data == nullptr || !pages.has_value()) {
      app.logger().error("Facility response is missing result/data metadata");
      return;
    }

    if (page == 1) {
      m_solver->reserve_nodes(moirai::json_size(*data) * static_cast<std::size_t>(*pages));
    }

    std::vector<FacilityListEntry> facilities;
    facilities.reserve(moirai::json_size(*data));
    for (const auto& facility : *data) {
      const auto facility_code =
        moirai::find_string_member(facility, "facility_code");
      if (!facility_code.has_value()) {
        app.logger().error("Skipping facility without facility_code");
        continue;
      }

      const auto facility_name = moirai::find_string_member(facility, "name");
      const auto property_id =
        moirai::find_string_member(facility, "property_id");
      facilities.push_back(FacilityListEntry{
        .code = std::string(*facility_code),
        .name = facility_name.has_value() ? std::string(*facility_name)
                                          : std::string{},
        .property_id = property_id.has_value() ? std::string(*property_id)
                                               : std::string{},
        .id = facility_identifier(facility),
        .profile = parse_facility_profile(facility),
      });
    }

    std::vector<FacilityProfile> facility_profiles;
    facility_profiles.reserve(facilities.size());
    for (const auto& facility : facilities) {
      facility_profiles.push_back(facility.profile);
    }

    const auto fetch_facility_profile =
      [this, &app, &facilities, &facility_profiles](std::size_t index) -> void {
      const auto& facility = facilities[index];
      if (!facility.id.has_value()) {
        app.logger().debug("No facility id found for {}; using defaults",
                           facility.code);
        return;
      }

      auto facility_profile = facility.profile;
      try {
        const auto detail_response = m_http_get(
          moirai::append_path(m_node_init_uri, *facility.id),
          { std::format("Authorization: Bearer {}", m_node_init_auth_token) });
        if (detail_response.status_code == HTTP_STATUS_OK) {
          const auto detail_json = moirai::parse_json(detail_response.body);
          if (detail_json.has_value() && detail_json->is_object()) {
            facility_profile = parse_facility_profile(*detail_json);
          } else {
            app.logger().error("Unable to parse facility detail for {}",
                               facility.code);
          }
        } else {
          app.logger().error("Unable to fetch facility detail for {}: {}",
                             facility.code,
                             detail_response.body);
        }
      } catch (const std::exception& exc) {
        app.logger().error("Unable to fetch facility detail for {}: {}",
                           facility.code,
                           exc.what());
      }
      facility_profiles[index] = facility_profile;
    };

    const auto detail_worker_count =
      parse_facility_detail_threads(facilities.size());
    if (detail_worker_count == 1) {
      for (std::size_t index = 0; index < facilities.size(); ++index) {
        fetch_facility_profile(index);
      }
    } else if (detail_worker_count > 1) {
      std::atomic_size_t next_facility{0};
      std::vector<std::jthread> workers;
      workers.reserve(detail_worker_count);
      for (std::size_t worker = 0; worker < detail_worker_count; ++worker) {
        workers.emplace_back([&]() {
          while (true) {
            const auto index =
              next_facility.fetch_add(1, std::memory_order_relaxed);
            if (index >= facilities.size()) {
              return;
            }
            fetch_facility_profile(index);
          }
        });
      }
    }

    for (std::size_t index = 0; index < facilities.size(); ++index) {
      const auto& facility = facilities[index];
      const auto facility_profile = facility_profiles[index];
      (*m_facility_profiles)[facility.code] = facility_profile;

      auto transport_center = TransportCenter{
        facility.code,
        facility.name
      };
      transport_center
        .set_latency<MovementType::CARTING, ProcessType::INBOUND>(
          DURATION{0});
      transport_center
        .set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(
          facility_profile.outbound_processing);
      transport_center
        .set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(
          DURATION{0});
      transport_center
        .set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(
          facility_profile.outbound_processing);
      transport_center.set_fresh_processing_time(
        facility_profile.fresh_processing);
      transport_center.set_mixed_bag_processing_time(
        facility_profile.mixed_bag_processing);
      transport_center.set_cutoff(facility_profile.center_arrival_cutoff);
      (void)m_solver->add_node(std::move(transport_center));

      if (!facility.property_id.empty()) {
        m_facility_groups[facility.property_id].push_back(facility.code);
      }
    }

    if (page < *pages) {
      init_nodes(static_cast<int16_t>(page + 1));
    }
  } else {
    app.logger().error("Unable to fetch facility data: {}", response.body);
  }
}

void
SolverWrapper::init_custody()
{
  auto& app = moirai::Application::instance();

  for (const auto& [key, value] : m_facility_groups) {
    (void)key;

    for (size_t i = 0; i < value.size(); ++i) {
      for (size_t j = 0; j < value.size(); ++j) {
        if (i == j) {
          continue;
        }

        const auto vertex_primary = m_solver->find_node(value[i]);
        const auto vertex_secondary = m_solver->find_node(value[j]);

        if (vertex_primary.has_value() and vertex_secondary.has_value()) {
          const std::string name =
            std::format("CUSTODY-{}-{}", value[i], value[j]);
          const auto edge_id = m_solver->add_edge(
              *vertex_primary, *vertex_secondary,
              TransportEdge{name, name});
          if (edge_id != INVALID_EDGE) {
            app.logger().debug("Added custody edge: {}", name);
          }
        } else {
          app.logger().error("Colocated facilities {}:{} or {}:{} missing",
                             value[i],
                             vertex_primary.has_value(),
                             value[j],
                             vertex_secondary.has_value());
        }
      }
    }
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void
SolverWrapper::init_edges()
{
  auto& app = moirai::Application::instance();
  const auto response = m_http_get(
    m_edge_init_uri,
    { std::format("Authorization: Bearer {}", m_edge_init_auth_token) });

  if (response.status_code == HTTP_STATUS_OK) {
    const auto response_json = moirai::parse_json(response.body);
    if (!response_json.has_value() || !response_json->is_object()) {
      app.logger().error("Unable to parse route response");
      return;
    }
    const auto* data = moirai::find_array_member(*response_json, "data");
    if (data == nullptr) {
      app.logger().error("Route response is missing data array");
      return;
    }

    app.logger().debug("Got {} routes", moirai::json_size(*data));
    const auto expansion_started = std::chrono::steady_clock::now();
    auto expanded_routes = expand_route_specs(*data);
    const auto expansion_ms =
      milliseconds_since(expansion_started, std::chrono::steady_clock::now());
    const auto route_edge_count = std::accumulate(
      expanded_routes.begin(),
      expanded_routes.end(),
      std::size_t{0},
      [](std::size_t total, const auto& route_specs) {
        return total + (route_specs.has_value() ? route_specs->size() : 0U);
      });
    m_solver->reserve_edges(route_edge_count);

    std::unordered_map<std::string,
                       std::optional<NodeId>,
                       TransparentStringHash,
                       TransparentStringEqual>
      node_cache;
    node_cache.reserve(route_edge_count == 0 ? 0 : route_edge_count / 2U);
    const auto resolve_node =
      [this, &node_cache](std::string_view center_code) -> std::optional<NodeId> {
      if (const auto cached = node_cache.find(center_code);
          cached != node_cache.end()) {
        return cached->second;
      }
      auto node = m_solver->find_node(center_code);
      node_cache.emplace(std::string(center_code), node);
      return node;
    };

    const auto insertion_started = std::chrono::steady_clock::now();
    std::size_t inserted_edges = 0;
    for (auto& route_specs : expanded_routes) {
      if (!route_specs.has_value()) {
        app.logger().error("Skipping route: {}", route_specs.error());
        continue;
      }

      for (auto& spec : *route_specs) {
        const auto source_vertex = resolve_node(spec.source_center_code);
        const auto target_vertex = resolve_node(spec.target_center_code);
        if (source_vertex.has_value() && target_vertex.has_value()) {
          (void)m_solver->add_edge(*source_vertex,
                                   *target_vertex,
                                   std::move(spec.edge));
          ++inserted_edges;
        } else {
          app.logger().error(
            "Edge<{}>: Source<{}>:{} or Target<{}>:{} vertex missing",
            spec.edge.code,
            spec.source_center_code,
            source_vertex.has_value(),
            spec.target_center_code,
            target_vertex.has_value());
        }
      }
    }
    app.logger().information(
      "Route timings: routes={} expanded_edges={} inserted_edges={} "
      "expansion_ms={} insertion_ms={}",
      moirai::json_size(*data),
      route_edge_count,
      inserted_edges,
      expansion_ms,
      milliseconds_since(insertion_started, std::chrono::steady_clock::now()));
  } else {
    app.logger().error("Unable to fetch route data: {}", response.body);
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto
SolverWrapper::find_paths(
  std::string bag,
  std::string bag_source,
  std::string bag_target,
  int32_t bag_start,
  DURATION source_processing_offset,
  CLOCK bag_end,
  DURATION mixed_bag_processing,
  std::vector<PackageInfo>& packages) const
  -> SearchDocument
{
  auto& app = moirai::Application::instance();
  const CLOCK zero = CLOCK{ std::chrono::minutes{ 0 } };
  const CLOCK start =
    zero + std::chrono::minutes{ bag_start } + source_processing_offset;
  CLOCK bag_pdd = bag_end;
  app.logger().debug("Bag end epoch minutes: {}",
                     bag_end.time_since_epoch().count());

  CLOCK bag_earliest_pdd = CLOCK::max();
  const auto source = m_solver->find_node(bag_source);
  const auto target = m_solver->find_node(bag_target);

  app.logger().debug("Finding path for {} with tmax epoch minutes: {}",
                     bag,
                     bag_end.time_since_epoch().count());

  SearchDocument response;
  response.id = bag;
  response.waybill = bag;
  response.package_id = bag;

  if (!source.has_value() || !target.has_value()) {
    app.logger().debug(
      "{}: Pathing failed. Source <{}>: {} or Target <{}>: {} missing",
      bag,
      bag_source,
      source.has_value(),
      bag_target,
      target.has_value());
    response.fail = std::format(
      "{}: Pathing failed. Source <{}>: {} or Target <{}>: {} missing",
      bag,
      bag_source,
      source.has_value(),
      bag_target,
      target.has_value());
    response.is_critical = true;
    return response;
  }

  const auto cache_bucket = [this](CLOCK timestamp) -> std::uint32_t {
    return m_cache_config.bucket_minutes == 0
             ? timestamp.time_since_epoch().count()
             : timestamp.time_since_epoch().count() /
                 m_cache_config.bucket_minutes;
  };
  const auto cache_key = [&](std::string_view mode,
                             NodeId source_node,
                             NodeId target_node,
                             CLOCK timestamp) -> std::string {
    return std::format("{}:{}:{}:{}",
                       mode,
                       source_node,
                       target_node,
                       cache_bucket(timestamp));
  };
  const auto cache_lookup = [this](std::string_view key)
      -> std::shared_ptr<const PathCache::Entry> {
    if (!m_path_cache) {
      return nullptr;
    }
    return m_path_cache->find(key);
  };
  const auto cache_store = [this](std::string_view key,
                                   PathCacheEntry entry) -> PathCacheEntry {
    if (m_path_cache) {
      PathCacheEntry copy = entry;
      m_path_cache->insert(std::string(key), std::move(copy));
    }
    return entry;
  };
  const auto forward_path = [&]() -> PathCacheEntry {
    const auto key = cache_key("F:S", *source, *target, start);
    if (auto cached = cache_lookup(key)) {
      return cached->value;
    }
    const auto path =
      m_solver->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        *source, *target, start);
    PathCacheEntry entry;
    entry.found = !path.empty();
    if (entry.found) {
      entry.first_distance = path.front().distance;
      entry.last_distance = path.back().distance;
      parse_path_into<PathTraversalMode::FORWARD>(path, entry.locations);
    }
    return cache_store(key, std::move(entry));
  }();

  bool critical = false;

  if (forward_path.found) {
    bag_earliest_pdd = forward_path.last_distance;

    app.logger().debug("Bag pdd epoch minutes {} earliest_pdd epoch minutes {}",
                       bag_pdd.time_since_epoch().count(),
                       bag_earliest_pdd.time_since_epoch().count());
    if (bag_pdd < bag_earliest_pdd) {
      critical = true;
    } else if (!packages.empty()) {
      CLOCK required_parent_deadline = CLOCK::max();
      for (const auto& package : packages) {
        const auto child_pdd =
          zero + std::chrono::minutes(std::get<1>(package));
        const auto child_target = m_solver->find_node(std::get<0>(package));
        if (!child_target.has_value()) {
          continue;
        }

        CLOCK child_pdd_at_parent_target = child_pdd;
        if (child_target != target) {
          const auto child_key =
            cache_key("R:S", *child_target, *target, child_pdd);
          const auto child_critical_path = [&]() -> PathCacheEntry {
            if (auto cached = cache_lookup(child_key)) {
              return cached->value;
            }
            const auto path =
              m_solver
                ->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
                  *child_target, *target, child_pdd);
            PathCacheEntry entry;
            entry.found = !path.empty();
            if (entry.found) {
              entry.first_distance = path.front().distance;
              entry.last_distance = path.back().distance;
              parse_path_into<PathTraversalMode::REVERSE>(path, entry.locations);
            }
            return cache_store(child_key, std::move(entry));
          }();

          if (!child_critical_path.found) {
            continue;
          }
          child_pdd_at_parent_target =
            child_critical_path.first_distance - mixed_bag_processing;
        }

        if (child_pdd_at_parent_target < required_parent_deadline) {
          required_parent_deadline = child_pdd_at_parent_target;
        }
      }

      if (required_parent_deadline != CLOCK::max()) {
        if (bag_pdd == CLOCK::max() || required_parent_deadline < bag_pdd) {
          bag_pdd = required_parent_deadline;
        }
        if (bag_pdd < bag_earliest_pdd) {
          critical = true;
        }
      }
    }
  } else {
    critical = true;
  }

  if (bag_pdd == CLOCK::max()) {
    bag_pdd = bag_earliest_pdd;
  }

  response.is_critical = critical;

  if (!critical) {
    const auto key = cache_key("R:S", *target, *source, bag_pdd);
    const auto ultimate_path = [&]() -> PathCacheEntry {
      if (auto cached = cache_lookup(key)) {
        return cached->value;
      }
      const auto path =
        m_solver->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
          *target, *source, bag_pdd);
      PathCacheEntry entry;
      entry.found = !path.empty();
      if (entry.found) {
        entry.first_distance = path.front().distance;
        entry.last_distance = path.back().distance;
        parse_path_into<PathTraversalMode::REVERSE>(path, entry.locations);
      }
      return cache_store(key, std::move(entry));
    }();
    if (ultimate_path.found) {
      response.ultimate.locations = ultimate_path.locations;
    }
  }

  if (!packages.empty()) {
    response.package_id = std::get<2>(packages[0]);
  }

  if (forward_path.found) {
    response.earliest.locations = forward_path.locations;
  }

  response.pdd = format_clock(bag_pdd);
  response.pdd_ts = bag_pdd.time_since_epoch().count() * 60;

  return response;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void
SolverWrapper::run(const std::stop_token& stop_token)
{
  auto& app = moirai::Application::instance();
  app.logger().debug("Initializing solver");
  app.logger().debug("Processing loads");
  app.logger().debug("0. Facility API profiles has {} entries",
                     m_facility_profiles == nullptr
                       ? std::size_t{0}
                       : m_facility_profiles->size());

  while (true) {
    try {
      std::array<std::string, SOLVER_BATCH_SIZE> payloads;
      if (const size_t num_packages =
            m_load_queue.wait_dequeue_bulk(std::span(payloads), stop_token);
          num_packages > 0) {
        std::for_each(
          payloads.begin(),
          payloads.begin() + static_cast<std::ptrdiff_t>(num_packages),
          // NOLINTNEXTLINE(readability-function-cognitive-complexity)
          [&app, &stop_token, this](const std::string& payload) -> void {
            const auto data = moirai::parse_json(payload);
            if (!data.has_value() || !data->is_object()) {
              app.logger().error("Invalid load payload");
              return;
            }

            const auto load = extract_load_payload(*data);
            if (!load.has_value()) {
              app.logger().error("Load payload is invalid: {}", load.error());
              return;
            }

            auto& packages = wrapper_scratch.packages;
            packages.clear();
            bool has_mixed_child_destinations = false;

            if (load->items.has_value()) {
              packages.reserve(moirai::json_size(*load->items));
              for (const auto& waybill : *load->items) {
                const auto waybill_ipdd =
                  moirai::find_string_member(waybill, "ipdd_destination");
                const auto waybill_cn =
                  moirai::find_string_member(waybill, "cn");
                const auto waybill_id =
                  moirai::find_string_member(waybill, "id");
                if (!waybill_ipdd.has_value() || !waybill_cn.has_value() ||
                    !waybill_id.has_value()) {
                  std::string invalid_fields;
                  if (!waybill_ipdd.has_value()) {
                    invalid_fields += "ipdd_destination";
                  }
                  if (!waybill_cn.has_value()) {
                    if (!invalid_fields.empty()) {
                      invalid_fields += ", ";
                    }
                    invalid_fields += "cn";
                  }
                  if (!waybill_id.has_value()) {
                    if (!invalid_fields.empty()) {
                      invalid_fields += ", ";
                    }
                    invalid_fields += "id";
                  }
                  app.logger().error(
                    "Load {} contains invalid waybill entry. "
                    "Invalid/missing fields: {}. Waybill payload: {}",
                    load->id,
                    invalid_fields,
                    moirai::dump(waybill));
                  continue;
                }

                try {
                  const auto cn = std::string(*waybill_cn);
                  const auto ipdd = std::string(*waybill_ipdd);
                  if (ipdd.length() <= MIN_IPDD_LENGTH) {
                    throw std::runtime_error(
                      std::format("ipdd_destination too short: '{}'", ipdd));
                  }
                  if (cn != load->destination) {
                    has_mixed_child_destinations = true;
                  }
                  const auto destination_profile =
                    profile_for(m_facility_profiles, cn);
                  const auto waybill_tmax = iso_to_date_utc_cutoff(
                    ipdd,
                    destination_profile.center_arrival_cutoff);

                  packages.emplace_back(std::move(cn),
                                        waybill_tmax.time_since_epoch().count(),
                                        std::string(*waybill_id));
                } catch (const std::exception& exc) {
                  app.logger().error(
                    "Load {} contains invalid waybill entry. "
                    "Failed to parse waybill: {}. Waybill payload: {}",
                    load->id,
                    exc.what(),
                    moirai::dump(waybill));
                }
              }
            }

            if (load->location.empty() || load->destination.empty() ||
                load->location == load->destination) {
              return;
            }

            CLOCK tmax = CLOCK::max();

            const auto destination_profile =
              profile_for(m_facility_profiles, load->destination);
            if (!load->items.has_value() &&
                load->ipdd_destination.length() > MIN_IPDD_LENGTH) {
              tmax = iso_to_date_utc_cutoff(
                load->ipdd_destination,
                destination_profile.center_arrival_cutoff);
            }

            SearchDocument solution;
            try {
              DURATION source_processing_offset{0};
              if (!load->items.has_value() &&
                  !load->origin_center.empty() &&
                  load->origin_center == load->location) {
                source_processing_offset =
                  profile_for(m_facility_profiles, load->location)
                    .fresh_processing;
              }
              const DURATION mixed_bag_processing =
                load->items.has_value() && has_mixed_child_destinations
                  ? destination_profile.mixed_bag_processing
                  : DURATION{0};
              solution = find_paths(
                load->id,
                load->location,
                load->destination,
                static_cast<int32_t>(iso_to_date(load->event_time)
                                       .time_since_epoch()
                                       .count()),
                source_processing_offset,
                tmax,
                mixed_bag_processing,
                packages);
            } catch (const std::exception& exc) {
              app.logger().error(
                "Failed to process load {}: {}", load->id, exc.what());
              return;
            }

            solution.cs_slid = load->cs_slid;
            solution.cs_act = load->cs_act;
            solution.pid = load->pid;
            if (!m_solution_queue.wait_enqueue(std::move(solution), stop_token)) {
              return;
            }
          });
      } else if (m_load_queue.closed()) {
        break;
      }
    } catch (const std::exception& exc) {
      app.logger().error("Exception occurred: {}", exc.what());
    }
  }
  if (m_path_cache) {
    const auto cache_metrics = m_path_cache->metrics();
    app.logger().information(
      "Path cache metrics: enabled=true entries={} hits={} misses={} evictions={}",
      cache_metrics.occupied,
      cache_metrics.hits,
      cache_metrics.misses,
      cache_metrics.evictions);
  } else {
    app.logger().information("Path cache metrics: enabled=false");
  }
}
