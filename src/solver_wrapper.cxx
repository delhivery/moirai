#include "solver_wrapper.hxx"
#include "date_utils.hxx"
#include "format.hxx"
#include "processor.hxx"
#include "transportation.hxx"
#include "utils.hxx"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Util/ServerApplication.h>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* load_queue,
  moodycamel::ConcurrentQueue<std::string>* solution_queue,
  const std::shared_ptr<Solver> solver)
  : load_queue(load_queue)
  , solution_queue(solution_queue)
  , solver(solver)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  running = true;
  // app.logger().information(solver.show_all());
}

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* node_queue,
  moodycamel::ConcurrentQueue<std::string>* edge_queue,
  moodycamel::ConcurrentQueue<std::string>* load_queue,
  moodycamel::ConcurrentQueue<std::string>* solution_queue,
  const std::string& node_uri,
  const std::string& node_token,
  const std::string& edge_uri,
  const std::string& edge_token,
  const std::filesystem::path& center_timings_filename)
  : node_queue(node_queue)
  , edge_queue(edge_queue)
  , load_queue(load_queue)
  , solution_queue(solution_queue)
  , node_init_uri(node_uri)
  , node_init_auth_token(node_token)
  , edge_init_uri(edge_uri)
  , edge_init_auth_token(edge_token)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  solver = std::make_shared<Solver>();
  init_timings(center_timings_filename);
  init_nodes();
  init_custody();
  init_edges();
  app.logger().debug(moirai::format("Initialized graph: {}", solver->show()));
  running = true;
  // app.logger().information(solver.show_all());
}

void
SolverWrapper::init_timings(
  const std::filesystem::path& facility_timings_filename)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  std::ifstream facility_timings_stream;
  facility_timings_stream.open(facility_timings_filename);

  assert(facility_timings_stream.is_open());
  assert(!facility_timings_stream.fail());

  auto facility_timings_json = nlohmann::json::parse(facility_timings_stream);

  std::ranges::for_each(facility_timings_json,
                        [this](auto const& facility_timing_entry) {});

  std::for_each(
    facility_timings_json.begin(),
    facility_timings_json.end(),
    [this](auto const& facility_timing_entry) {
      facility_timings_map[facility_timing_entry["code"]
                             .template get<std::string>()] =
        std::make_tuple(
          std::stoi(facility_timing_entry["ci"].template get<std::string>()),
          std::stoi(facility_timing_entry["co"].template get<std::string>()),
          std::stoi(facility_timing_entry["li"].template get<std::string>()),
          std::stoi(facility_timing_entry["lo"].template get<std::string>()));
    });
}

const std::shared_ptr<Solver>
SolverWrapper::get_solver() const
{
  return solver;
}

void
SolverWrapper::init_nodes(int16_t page)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  Poco::URI temp_uri(node_init_uri);
  temp_uri.addQueryParameter("page", std::to_string(page));
  temp_uri.addQueryParameter("status", "active");
  std::string path(temp_uri.getPathAndQuery());

  if (path.empty())
    path = "/";

  Poco::Net::HTTPSClientSession session(temp_uri.getHost(), temp_uri.getPort());
  Poco::Net::HTTPRequest request(
    Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
  request.setCredentials("Bearer", node_init_auth_token);
  session.sendRequest(request);
  Poco::Net::HTTPResponse response;
  std::istream& response_stream = session.receiveResponse(response);

  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
    auto response_json = nlohmann::json::parse(response_stream);
    auto data = response_json["result"]["data"];

    std::for_each(data.begin(), data.end(), [&app, this](auto const& facility) {
      std::string facility_code =
        facility["facility_code"].template get<std::string>();
      std::tuple<int32_t, int32_t, int32_t, int32_t> facility_timings{
        0, 0, 0, 0
      };

      if (facility_timings_map.contains(facility_code)) {
        facility_timings = facility_timings_map[facility_code];
      }

      auto transport_center = std::make_shared<TransportCenter>(facility_code);
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
      solver->add_node(transport_center);

      if (!facility["property_id"].is_null()) {
        std::string property_id =
          facility["property_id"].template get<std::string>();

        if (!property_id.empty()) {
          if (!facility_groups.contains(property_id)) {
            facility_groups[property_id] = std::vector<std::string>{};
          }

          facility_groups[property_id].push_back(facility_code);
        }
      }
    });

    auto pages =
      response_json["result"]["total_page_count"].template get<int16_t>();
    if (page < pages)
      init_nodes(page + 1);
  } else {
    std::stringstream response_raw;
    Poco::StreamCopier::copyStream(response_stream, response_raw);
    app.logger().error(moirai::format("Unable to fetch facility data: <{}>: {}",
                                      response.getStatus(),
                                      response_raw.str()));
  }
}

void
SolverWrapper::init_custody()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();

  for (auto const& [key, value] : facility_groups) {

    for (size_t i = 0; i < value.size(); ++i) {
      for (size_t j = 0; j < value.size(); ++j) {
        if (i != j) {
          auto [vertex_primary, has_vertex_primary] =
            solver->add_node(value[i]);
          auto [vertex_secondary, has_vertex_seconday] =
            solver->add_node(value[j]);

          if (has_vertex_primary and has_vertex_seconday) {
            std::string name =
              moirai::format("CUSTODY-{}-{}", value[i], value[j]);
            solver->add_edge(vertex_primary,
                             vertex_secondary,
                             std::make_shared<TransportEdge>(name, name));
            app.logger().debug(moirai::format("Added custody edge: {}", name));
          } else {
            app.logger().error(
              moirai::format("Colocated facilities {}:{} or {}:{} missing",
                             value[i],
                             has_vertex_primary,
                             value[j],
                             has_vertex_seconday));
          }
        }
      }
    }
  }
}

void
SolverWrapper::init_edges()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  std::string path(edge_init_uri.getPathAndQuery());

  if (path.empty())
    path = "/";

  Poco::Net::HTTPSClientSession session(edge_init_uri.getHost(),
                                        edge_init_uri.getPort());
  Poco::Net::HTTPRequest request(
    Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
  request.setCredentials("Bearer", edge_init_auth_token);
  session.sendRequest(request);
  Poco::Net::HTTPResponse response;
  std::istream& response_stream = session.receiveResponse(response);
  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
    auto response_json = nlohmann::json::parse(response_stream);
    auto data = response_json["data"];

    app.logger().debug(moirai::format("Got {} edges", data.size()));

    std::for_each(data.begin(), data.end(), [&app, this](auto const& route) {
      std::string uuid =
        route["route_schedule_uuid"].template get<std::string>();
      std::string name = route["name"].template get<std::string>();
      std::string route_type = route["route_type"].template get<std::string>();
      to_lower(route_type);
      std::string reporting_time =
        route["reporting_time"].template get<std::string>();
      TIME_OF_DAY offset{ datemod(std::chrono::duration_cast<TIME_OF_DAY>(
                                    time_string_to_time(reporting_time)) -
                                    DURATION{ 330 },
                                  std::chrono::days{ 1 }) };
      auto stops = route["halt_centers"];

      if (!stops.is_array()) {
        app.logger().error(
          moirai::format("Edge<{}> stops is not an array", uuid));
        return;
      }
      for (int i = 0; i < stops.size(); ++i) {
        for (int j = i + 1; j < stops.size(); ++j) {
          auto source = stops[i];
          auto target = stops[j];

          std::string departure = source["rel_etd"].template get<std::string>();
          std::string arrival = target["rel_eta"].template get<std::string>();

          TIME_OF_DAY departure_as_time(offset +
                                        std::chrono::duration_cast<TIME_OF_DAY>(
                                          time_string_to_time(departure)));
          DURATION duration(std::chrono::duration_cast<TIME_OF_DAY>(
            time_string_to_time(arrival) -
            std::chrono::duration_cast<TIME_OF_DAY>(
              time_string_to_time(departure))));

          if (departure_as_time.count() < 0 or duration.count() < 0) {
            app.logger().error(
              moirai::format("Edge<{}>: Departure<{}> or duration<{}> negative",
                             uuid,
                             departure_as_time.count(),
                             duration.count()));
            return;
          }

          std::string source_center_code =
            source["center_code"].template get<std::string>();
          std::string target_center_code =
            target["center_code"].template get<std::string>();

          auto [source_vertex, has_source_vertex] =
            solver->add_node(source_center_code);
          auto [target_vertex, has_target_vertex] =
            solver->add_node(target_center_code);
          auto edge = std::make_shared<TransportEdge>(
            moirai::format("{}.{}", uuid, i * (stops.size() - 1) + j - i - 1),
            name,
            departure_as_time,
            duration,
            route_type == "air" ? VehicleType::AIR : VehicleType::SURFACE,
            route_type == "carting" ? MovementType::CARTING
                                    : MovementType::LINEHAUL);
          if (has_source_vertex and has_target_vertex) {
            solver->add_edge(source_vertex, target_vertex, edge);
          } else {
            app.logger().error(moirai::format(
              "Edge<{}>: Source<{}>:{} or Target<{}>:{} vertex missing",
              uuid,
              source_center_code,
              has_source_vertex,
              target_center_code,
              has_target_vertex));
          }
        }
      }
    });
  } else {
    std::stringstream response_raw;
    Poco::StreamCopier::copyStream(response_stream, response_raw);
    app.logger().error(moirai::format("Unable to fetch route data: <{}>: {}",
                                      response.getStatus(),
                                      response_raw.str()));
  }
}

auto
SolverWrapper::find_paths(
  std::string bag,
  std::string bag_source,
  std::string bag_target,
  int32_t bag_start,
  CLOCK bag_end,
  std::vector<std::tuple<std::string, int32_t, std::string>>& packages) const
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  CLOCK ZERO = CLOCK{ std::chrono::minutes{ 0 } };
  CLOCK start = ZERO + std::chrono::minutes{ bag_start };
  CLOCK bag_pdd = bag_end;
  // bag_pdd = CLOCK::max();
  CLOCK bag_earliest_pdd = CLOCK::max();
  const auto [source, has_source] = solver->add_node(bag_source);
  const auto [target, has_target] = solver->add_node(bag_target);

  if (!has_source || !has_target) {
    app.logger().debug(
      "{}: Pathing failed. Source <{}>: {} or Target <{}>: {} missing",
      bag,
      bag_source,
      has_source,
      bag_target,
      has_target);
    return nlohmann::json({});
  }

  auto solution_earliest_start_segment =
    solver->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start);
  std::shared_ptr<Segment> solution_ultimate_start_segment = nullptr;

  bool critical = false;

  if (solution_earliest_start_segment != nullptr) {

    auto segment = solution_earliest_start_segment;
    while (segment->next != nullptr)
      segment = segment->next;
    bag_earliest_pdd = segment->distance;

    if (bag_pdd < bag_earliest_pdd)
      critical = true;
    else {
      auto child_earliest = *std::min_element(
        packages.begin(), packages.end(), [](auto const& lhs, auto const& rhs) {
          return std::get<1>(lhs) < std::get<1>(rhs);
        });

      const auto [child_target, has_child_target] =
        solver->add_node(std::get<0>(child_earliest));

      if (child_target != target and has_child_target) {
        auto child_pdd =
          ZERO + std::chrono::minutes(std::get<1>(child_earliest));
        auto child_critical_start_segment =
          solver->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
            child_target, target, child_pdd);

        if (child_critical_start_segment != nullptr) {
          auto child_pdd_at_parent_target =
            child_critical_start_segment->distance;

          if (child_pdd_at_parent_target < bag_earliest_pdd) {
            critical = true;
          }
        }
      }
    }
  } else {
    return nlohmann::json({});
  }

  if (bag_pdd == CLOCK::max()) {
    bag_pdd = bag_earliest_pdd;
  }

  if (not critical) {
    solution_ultimate_start_segment =
      solver->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        target, source, bag_pdd);
  }

  nlohmann::json response;
  response["_id"] = bag;
  response["waybill"] = bag;
  response["package"] = bag;

  if (packages.size() > 0) {
    response["package"] = std::get<2>(packages[0]);
  }

  response["earliest"]["locations"] =
    parse_path<PathTraversalMode::FORWARD>(solution_earliest_start_segment);
  response["earliest"]["first"] = response["earliest"]["locations"][0];

  if (solution_ultimate_start_segment != nullptr) {
    response["ultimate"]["locations"] =
      parse_path<PathTraversalMode::REVERSE>(solution_ultimate_start_segment);
    response["ultimate"]["first"] = response["ultimate"]["locations"][0];
  }
  response["pdd"] = date::format("%D %T", bag_pdd);
  return response;
}

void
SolverWrapper::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  app.logger().debug("Initializing solver");
  app.logger().debug("Processing loads");

  while (running or load_queue->size_approx() > 0) {
    try {
      Poco::Thread::sleep(200);

      if (solution_queue->size_approx() > 1000)
        continue;
      std::string payloads[100];
      app.logger().debug(
        moirai::format("C: Queue size: {}", load_queue->size_approx()));

      if (size_t num_packages = load_queue->try_dequeue_bulk(payloads, 100);
          num_packages > 0) {
        std::for_each(
          payloads,
          payloads + num_packages,
          [&app, this](const std::string& payload) {
            nlohmann::json data = nlohmann::json::parse(payload);

            if (data["id"].is_null() or data["location"].is_null() or
                data["destination"].is_null() or data["time"].is_null()) {
              app.logger().debug(moirai::format(
                "Null data against mandatory fields. {}", data.dump()));
              return;
            }
            app.logger().debug(
              moirai::format("Recieved data: {}", data.dump()));
            std::vector<std::tuple<std::string, int32_t, std::string>> packages;

            for (auto& waybill : data["items"]) {
              if (waybill["ipdd_destination"].is_null() or
                  waybill["cn"].is_null() or waybill["id"].is_null())
                return;
              packages.emplace_back(
                waybill["cn"].template get<std::string>(),
                iso_to_date(
                  waybill["ipdd_destination"].template get<std::string>(), true)
                  .time_since_epoch()
                  .count(),
                waybill["id"].template get<std::string>());
            }

            std::string bag_identifier = data["id"].template get<std::string>();
            std::string current_location =
              data["location"].template get<std::string>();
            std::string item_destination =
              data["destination"].template get<std::string>();

            if (current_location.empty() or item_destination.empty() or
                current_location == item_destination)
              return;

            CLOCK tmax = CLOCK::max();

            if (!data["ipdd_destination"].is_null() and
                !data["ipdd_destination"].empty()) {
              std::string ipdd =
                data["ipdd_destination"].template get<std::string>();

              if (ipdd.length() > 11) {
                tmax = iso_to_date(ipdd);
              }
            }

            nlohmann::json solution =
              find_paths(bag_identifier,
                         current_location,
                         item_destination,
                         iso_to_date(data["time"].template get<std::string>())
                           .time_since_epoch()
                           .count(),
                         tmax,
                         packages);

            if (solution.empty()) {
              app.logger().debug(
                moirai::format("No legitimate paths for payload: {}", payload));
              return;
            }
            solution["cs_slid"] =
              data["cs_slid"].is_null()
                ? ""
                : data["cs_slid"].template get<std::string>();

            solution["cs_act"] = data["cs_act"].is_null()
                                   ? ""
                                   : data["cs_act"].template get<std::string>();
            solution["pid"] = data["pid"].is_null()
                                ? ""
                                : data["pid"].template get<std::string>();
            solution_queue->enqueue(solution.dump());
          });
      }
    } catch (const std::exception& exc) {
      app.logger().error(moirai::format("Exception occurred: {}", exc.what()));
    }
  }
}
