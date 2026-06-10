#include "solver_wrapper.hxx"
#include "app.hxx"
#include "blocking_queue.hxx"
#include "date_utils.hxx"
#include "json_utils.hxx"
#include "processor.hxx"
#include "route_schedule.hxx"
#include "transportation.hxx"
#include "utils.hxx"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr long HTTP_STATUS_OK = 200;
constexpr auto IST_OFFSET = DURATION{ 330 };
constexpr std::size_t SOLVER_BATCH_SIZE = 64;
constexpr std::size_t MIN_IPDD_LENGTH = 11;
const auto DEFAULT_FACILITY_CUTOFF =
  std::chrono::duration_cast<TIME_OF_DAY>(time_string_to_time("04:00"));

using FacilityTimings =
  std::tuple<int16_t, int16_t, int16_t, int16_t, TIME_OF_DAY>;

auto parse_facility_latency_minutes(const moirai::Json& object,
                                    const char* key) -> std::optional<int16_t>
{
  if (const auto parsed_integer =
        moirai::find_integer_member<int16_t>(object, key);
      parsed_integer.has_value()) {
    return parsed_integer;
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

  return static_cast<int16_t>(minutes);
}

} // namespace

SolverWrapper::SolverWrapper(
  RuntimeQueues queues,
  const std::shared_ptr<Solver>& solver,
  const std::filesystem::path& center_timings_filename,
  HttpGet http_get)
  : m_solver(solver)
  , m_load_queue(*queues.load)
  , m_solution_queue(*queues.solution)
  , m_http_get(std::move(http_get))
{
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
  , m_node_queue(queues.node)
  , m_edge_queue(queues.edge)
  , m_load_queue(*queues.load)
  , m_solution_queue(*queues.solution)
  , m_http_get(std::move(http_get))
{
  auto& app = moirai::Application::instance();
  m_solver = std::make_shared<Solver>();
  init_timings(center_timings_filename);
  init_nodes();
  init_custody();
  init_edges();
  app.logger().debug("Initialized graph: {}", m_solver->show());
}

void
SolverWrapper::init_timings(
  const std::filesystem::path& facility_timings_filename)
{
  auto& app = moirai::Application::instance();
  std::ifstream facility_timings_stream(facility_timings_filename);

  if (!facility_timings_stream.is_open() || facility_timings_stream.fail()) {
    throw std::runtime_error(
      std::format("Failed to open facility timings file {}",
                  facility_timings_filename.string()));
  }

  const auto facility_timings_json =
    moirai::parse_json(facility_timings_stream);
  if (!facility_timings_json.has_value() ||
      !facility_timings_json->is_array()) {
    throw std::runtime_error("Facility timings payload is not a JSON array");
  }

  app.logger().debug("Found timings for {} facilities",
                     facility_timings_json->size());

  for (const auto& facility_timing_entry : *facility_timings_json) {
    const auto code = moirai::find_string_member(facility_timing_entry, "code");
    const auto ci = parse_facility_latency_minutes(facility_timing_entry, "ci");
    const auto co = parse_facility_latency_minutes(facility_timing_entry, "co");
    const auto li = parse_facility_latency_minutes(facility_timing_entry, "li");
    const auto lo = parse_facility_latency_minutes(facility_timing_entry, "lo");
    const auto cut = moirai::find_string_member(facility_timing_entry, "cut");
    if (!code.has_value() || !ci.has_value() || !co.has_value() ||
        !li.has_value() || !lo.has_value() || !cut.has_value()) {
      std::string invalid_fields;
      const auto append_field =
        [&invalid_fields](std::string_view field_name) -> void {
        if (!invalid_fields.empty()) {
          invalid_fields += ", ";
        }
        invalid_fields += field_name;
      };

      if (!code.has_value()) {
        append_field("code");
      }
      if (!ci.has_value()) {
        append_field("ci");
      }
      if (!co.has_value()) {
        append_field("co");
      }
      if (!li.has_value()) {
        append_field("li");
      }
      if (!lo.has_value()) {
        append_field("lo");
      }
      if (!cut.has_value()) {
        append_field("cut");
      }

      app.logger().error("Skipping invalid facility timings entry. "
                         "Invalid/missing fields: {}. "
                         "Payload: {}",
                         invalid_fields,
                         facility_timing_entry.dump());
      continue;
    }

    try {
      m_facility_timings_map[std::string(*code)] = FacilityTimings{
        *ci,
        *co,
        *li,
        *lo,
        std::chrono::duration_cast<TIME_OF_DAY>(time_string_to_time(*cut)),
      };
    } catch (const std::exception& exc) {
      app.logger().error("Skipping facility timings entry {}: {}. Payload: {}",
                         *code,
                         exc.what(),
                         facility_timing_entry.dump());
    }
  }

  app.logger().debug("Populated timings map for {} facilities",
                     m_facility_timings_map.size());
}

auto
SolverWrapper::get_solver() const -> std::shared_ptr<Solver>
{
  return m_solver;
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

    for (const auto& facility : *data) {
      const auto facility_code =
        moirai::find_string_member(facility, "facility_code");
      if (!facility_code.has_value()) {
        app.logger().error("Skipping facility without facility_code");
        continue;
      }

      FacilityTimings facility_timings{ 0, 0, 0, 0, DEFAULT_FACILITY_CUTOFF };

      if (m_facility_timings_map.contains(std::string(*facility_code))) {
        facility_timings = m_facility_timings_map[std::string(*facility_code)];
      } else {
        app.logger().debug("No timings found for facility: {}", *facility_code);
      }

      auto transport_center =
        std::make_shared<TransportCenter>(std::string(*facility_code));
      transport_center
        ->set_latency<MovementType::CARTING, ProcessType::INBOUND>(
          DURATION(std::get<0>(facility_timings)));
      transport_center
        ->set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(
          DURATION(std::get<1>(facility_timings)));
      transport_center
        ->set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(
          DURATION(std::get<2>(facility_timings)));
      transport_center
        ->set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(
          DURATION(std::get<3>(facility_timings)));
      transport_center->set_cutoff(std::get<4>(facility_timings));
      m_solver->add_node(transport_center);

      const auto property_id =
        moirai::find_string_member(facility, "property_id");
      if (property_id.has_value() && !property_id->empty()) {
        m_facility_groups[std::string(*property_id)].push_back(
          std::string(*facility_code));
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

        auto [vertex_primary, has_vertex_primary] =
          m_solver->add_node(value[i]);
        auto [vertex_secondary, has_vertex_secondary] =
          m_solver->add_node(value[j]);

        if (has_vertex_primary and has_vertex_secondary) {
          const std::string name =
            std::format("CUSTODY-{}-{}", value[i], value[j]);
          m_solver->add_edge(vertex_primary,
                             vertex_secondary,
                             std::make_shared<TransportEdge>(name, name));
          app.logger().debug("Added custody edge: {}", name);
        } else {
          app.logger().error("Colocated facilities {}:{} or {}:{} missing",
                             value[i],
                             has_vertex_primary,
                             value[j],
                             has_vertex_secondary);
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

    app.logger().debug("Got {} edges", data->size());

    for (const auto& route : *data) {
      try {
        for (const auto& spec : build_route_edge_specs(route, IST_OFFSET)) {
          auto [source_vertex, has_source_vertex] =
            m_solver->add_node(spec.source_center_code);
          auto [target_vertex, has_target_vertex] =
            m_solver->add_node(spec.target_center_code);
          if (has_source_vertex && has_target_vertex) {
            m_solver->add_edge(source_vertex, target_vertex, spec.edge);
          } else {
            app.logger().error(
              "Edge<{}>: Source<{}>:{} or Target<{}>:{} vertex missing",
              spec.edge->code,
              spec.source_center_code,
              has_source_vertex,
              spec.target_center_code,
              has_target_vertex);
          }
        }
      } catch (const std::exception& exc) {
        app.logger().error("Skipping route: {}", exc.what());
      }
    }
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
  CLOCK bag_end,
  std::vector<std::tuple<std::string, int32_t, std::string>>& packages) const
  -> nlohmann::json
{
  auto& app = moirai::Application::instance();
  const CLOCK zero = CLOCK{ std::chrono::minutes{ 0 } };
  const CLOCK start = zero + std::chrono::minutes{ bag_start };
  CLOCK bag_pdd = bag_end;
  app.logger().debug("Bag end epoch minutes: {}",
                     bag_end.time_since_epoch().count());

  CLOCK bag_earliest_pdd = CLOCK::max();
  const auto [source, has_source] = m_solver->add_node(bag_source);
  const auto [target, has_target] = m_solver->add_node(bag_target);

  app.logger().debug("Finding path for {} with tmax epoch minutes: {}",
                     bag,
                     bag_end.time_since_epoch().count());

  nlohmann::json response;
  response["_id"] = bag;
  response["waybill"] = bag;
  response["package"] = bag;

  if (!has_source || !has_target) {
    app.logger().debug(
      "{}: Pathing failed. Source <{}>: {} or Target <{}>: {} missing",
      bag,
      bag_source,
      has_source,
      bag_target,
      has_target);
    response["fail"] = std::format(
      "{}: Pathing failed. Source <{}>: {} or Target <{}>: {} missing",
      bag,
      bag_source,
      has_source,
      bag_target,
      has_target);
    return response;
  }

  auto solution_earliest_start_segment =
    m_solver->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start);
  std::shared_ptr<Segment> solution_ultimate_start_segment = nullptr;

  bool critical = false;

  if (solution_earliest_start_segment != nullptr) {
    auto segment = solution_earliest_start_segment;
    while (segment->next != nullptr) {
      segment = segment->next;
    }
    bag_earliest_pdd = segment->distance;

    app.logger().debug("Bag pdd epoch minutes {} earliest_pdd epoch minutes {}",
                       bag_pdd.time_since_epoch().count(),
                       bag_earliest_pdd.time_since_epoch().count());
    if (bag_pdd < bag_earliest_pdd) {
      critical = true;
    } else if (!packages.empty()) {
      const auto child_earliest = *std::ranges::min_element(
        packages, [](const auto& lhs, const auto& rhs) -> auto {
          return std::get<1>(lhs) < std::get<1>(rhs);
        });

      const auto [child_target, has_child_target] =
        m_solver->add_node(std::get<0>(child_earliest));

      if (child_target != target and has_child_target) {
        const auto child_pdd =
          zero + std::chrono::minutes(std::get<1>(child_earliest));
        auto child_critical_start_segment =
          m_solver->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
            child_target, target, child_pdd);

        if (child_critical_start_segment != nullptr) {
          const auto child_pdd_at_parent_target =
            child_critical_start_segment->distance;

          if (bag_pdd == CLOCK::max()) {
            bag_pdd = child_pdd_at_parent_target;
          }

          if (child_pdd_at_parent_target < bag_earliest_pdd) {
            critical = true;
          }
        }
      }
    }
  } else {
    critical = true;
  }

  if (bag_pdd == CLOCK::max()) {
    bag_pdd = bag_earliest_pdd;
  }

  if (!critical) {
    solution_ultimate_start_segment =
      m_solver->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        target, source, bag_pdd);
  }

  if (!packages.empty()) {
    response["package"] = std::get<2>(packages[0]);
  }

  if (const auto earliest_path =
        parse_path<PathTraversalMode::FORWARD>(solution_earliest_start_segment);
      !earliest_path.empty()) {
    response["earliest"]["locations"] = earliest_path;
    response["earliest"]["first"] = earliest_path[0];

    if (earliest_path.size() > 1) {
      response["earliest"]["second"] = earliest_path[1];
    }
  }

  if (const auto ultimate_path =
        parse_path<PathTraversalMode::REVERSE>(solution_ultimate_start_segment);
      !ultimate_path.empty()) {
    response["ultimate"]["locations"] = ultimate_path;
    response["ultimate"]["first"] = ultimate_path[0];

    if (ultimate_path.size() > 1) {
      response["ultimate"]["second"] = ultimate_path[1];
    }
  }

  response["pdd"] = format_clock(bag_pdd);
  response["pdd_ts"] = bag_pdd.time_since_epoch().count();

  return response;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void
SolverWrapper::run(const std::stop_token& stop_token)
{
  auto& app = moirai::Application::instance();
  app.logger().debug("Initializing solver");
  app.logger().debug("Processing loads");
  app.logger().debug("0. Facility timings has {} entries",
                     m_facility_timings_map.size());

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

            const auto bag_identifier = moirai::find_string_member(*data, "id");
            const auto current_location =
              moirai::find_string_member(*data, "location");
            const auto item_destination =
              moirai::find_string_member(*data, "destination");
            const auto event_time = moirai::find_string_member(*data, "time");
            const auto* items = moirai::find_array_member(*data, "items");
            if (items == nullptr) {
              items = moirai::find_array_member(*data, "item");
            }
            if (!bag_identifier.has_value() || !current_location.has_value() ||
                !item_destination.has_value() || !event_time.has_value()) {
              app.logger().error("Load payload is missing mandatory fields");
              return;
            }

            std::vector<std::tuple<std::string, int32_t, std::string>> packages;

            if (items != nullptr) {
              for (const auto& waybill : *items) {
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
                    *bag_identifier,
                    invalid_fields,
                    waybill.dump());
                  continue;
                }

                try {
                  auto waybill_tmax =
                    iso_to_date(std::string(*waybill_ipdd), true);
                  if (m_facility_timings_map.contains(
                        std::string(*waybill_cn))) {
                    waybill_tmax = iso_to_date(
                      std::string(*waybill_ipdd),
                      std::get<4>(
                        m_facility_timings_map[std::string(*waybill_cn)]));
                  }

                  packages.emplace_back(std::string(*waybill_cn),
                                        waybill_tmax.time_since_epoch().count(),
                                        std::string(*waybill_id));
                } catch (const std::exception& exc) {
                  app.logger().error(
                    "Load {} contains invalid waybill entry. "
                    "Failed to parse waybill: {}. Waybill payload: {}",
                    *bag_identifier,
                    exc.what(),
                    waybill.dump());
                }
              }
            }

            if (current_location->empty() || item_destination->empty() ||
                current_location == item_destination) {
              return;
            }

            CLOCK tmax = CLOCK::max();

            const auto ipdd =
              moirai::find_string_member(*data, "ipdd_destination");
            if (ipdd.has_value() && ipdd->length() > MIN_IPDD_LENGTH) {
              tmax = iso_to_date(std::string(*ipdd), true);
              if (m_facility_timings_map.contains(
                    std::string(*item_destination))) {
                tmax = iso_to_date(
                  std::string(*ipdd),
                  std::get<4>(
                    m_facility_timings_map[std::string(*item_destination)]));
              }
            }

            nlohmann::json solution;
            try {
              solution = find_paths(
                std::string(*bag_identifier),
                std::string(*current_location),
                std::string(*item_destination),
                static_cast<int32_t>(iso_to_date(std::string(*event_time))
                                       .time_since_epoch()
                                       .count()),
                tmax,
                packages);
            } catch (const std::exception& exc) {
              app.logger().error(
                "Failed to process load {}: {}", *bag_identifier, exc.what());
              return;
            }

            if (solution.empty()) {
              app.logger().debug("No legitimate paths for payload: {}",
                                 payload);
              return;
            }

            const auto cs_slid = moirai::find_string_member(*data, "cs_slid");
            const auto cs_act = moirai::find_string_member(*data, "cs_act");
            const auto pid = moirai::find_string_member(*data, "pid");
            solution["cs_slid"] =
              cs_slid.has_value() ? std::string(*cs_slid) : "";
            solution["cs_act"] = cs_act.has_value() ? std::string(*cs_act) : "";
            solution["pid"] = pid.has_value() ? std::string(*pid) : "";
            if (!m_solution_queue.wait_enqueue(solution.dump(), stop_token)) {
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
}
