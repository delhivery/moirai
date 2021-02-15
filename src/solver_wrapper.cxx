#include "solver_wrapper.hxx"
#include "date_utils.hxx"
#include "format.hxx"
#include "transportation.hxx"
#include "utils.hxx"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Util/ServerApplication.h>
#include <algorithm>
#include <cstddef>
#include <execution>
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
  init_timings(center_timings_filename);
  init_nodes();
  init_custody();
  init_edges();
  app.logger().information(
    moirai::format("Initialized graph: {}", solver.show()));
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
      solver.add_node(transport_center);

      if (!facility["group_id"].is_null()) {
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
          auto [vertex_primary, has_vertex_primary] = solver.add_node(value[i]);
          auto [vertex_secondary, has_vertex_seconday] =
            solver.add_node(value[j]);

          if (has_vertex_primary and has_vertex_seconday) {
            std::string name =
              moirai::format("CUSTODY-{}-{}", value[i], value[j]);
            solver.add_edge(vertex_primary,
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

    app.logger().information(moirai::format("Got {} edges", data.size()));

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
            solver.add_node(source_center_code);
          auto [target_vertex, has_target_vertex] =
            solver.add_node(target_center_code);
          auto edge = std::make_shared<TransportEdge>(
            moirai::format("{}.{}", uuid, i * (stops.size() - 1) + j - i - 1),
            name,
            departure_as_time,
            duration,
            route_type == "air" ? VehicleType::AIR : VehicleType::SURFACE,
            route_type == "carting" ? MovementType::CARTING
                                    : MovementType::LINEHAUL);
          if (has_source_vertex and has_target_vertex) {
            solver.add_edge(source_vertex, target_vertex, edge);
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
  std::vector<std::tuple<std::string, int32_t, std::string>>& packages) const
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  CLOCK ZERO = CLOCK{ std::chrono::minutes{ 0 } };
  CLOCK start = ZERO + std::chrono::minutes{ bag_start };
  CLOCK bag_pdd = CLOCK::max();
  CLOCK bag_earliest_pdd = CLOCK::max();
  const auto [source, has_source] = solver.add_node(bag_source);
  const auto [target, has_target] = solver.add_node(bag_target);

  if (!has_source || !has_target) {
    app.logger().error(
      "{}: Pathing failed. Source <{}>: {} or Target <{}>: {} missing",
      bag,
      bag_source,
      has_source,
      bag_target,
      has_target);
    return nlohmann::json({});
  }

  Path solution_earliest =
    solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start);
  Path solution_ultimate;

  if (solution_earliest.size() > 0) {

    bag_earliest_pdd = std::get<3>(solution_earliest[0]);

    for (auto const& package : packages) {
      std::string package_target_string = std::get<0>(package);
      const auto [package_target, has_package_target] =
        solver.add_node(package_target_string);

      if (has_package_target) {
        Path pkg_reverse_path =
          solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
            package_target,
            target,
            ZERO + std::chrono::minutes(std::get<1>(package)));

        if (pkg_reverse_path.size() > 0) {
          CLOCK package_pdd = std::get<3>(pkg_reverse_path[0]);

          if (package_pdd < bag_pdd and package_pdd >= bag_earliest_pdd) {
            bag_pdd = package_pdd;
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

  solution_ultimate =
    solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
      target, source, bag_pdd);

  nlohmann::json response;
  response["_id"] = bag;
  response["waybill"] = bag;
  response["package"] = bag;

  if (packages.size() > 0) {
    response["package"] = std::get<2>(packages[0]);
  }

  response["earliest"]["locations"] = {};
  response["ultimate"]["locations"] = {};

  std::shared_ptr<TransportEdge> outbound_edge = nullptr;

  for (auto idx = 0; idx < solution_earliest.size(); ++idx) {
    auto [current_node, previous_node, inbound_edge, distance_current] =
      solution_earliest[idx];

    std::string current_node_code = current_node->code;
    DURATION latency =
      current_node->get_latency<MovementType::LINEHAUL, ProcessType::INBOUND>();

    if (inbound_edge->movement == MovementType::CARTING)
      latency = current_node
                  ->get_latency<MovementType::CARTING, ProcessType::INBOUND>();
    std::string current_node_arrival =
      date::format("%D %T", distance_current - latency);

    nlohmann::json location_entry = { { "code", current_node_code },
                                      { "arrival", current_node_arrival } };

    if (outbound_edge != nullptr) {
      location_entry["departure"] = date::format(
        "%D %T", get_departure(distance_current, outbound_edge->departure));
      location_entry["route"] =
        outbound_edge->code.substr(0, outbound_edge->code.find('.'));
    }
    response["earliest"]["locations"].push_back(location_entry);
    outbound_edge = inbound_edge;
  }
  {
    auto current_node = solver.get_node(source);
    nlohmann::json location_entry = { { "code", solver.get_node(source)->code },
                                      { "arrival",
                                        date::format("%D %T", start) } };

    if (outbound_edge != nullptr) {
      location_entry["departure"] =
        date::format("%D %T", get_departure(start, outbound_edge->departure));
      location_entry["route"] =
        outbound_edge->code.substr(0, outbound_edge->code.find('.'));
    }
    response["earliest"]["locations"].push_back(location_entry);
    std::reverse(response["earliest"]["locations"].begin(),
                 response["earliest"]["locations"].end());
    response["earliest"]["first"] = location_entry;
  }
  std::shared_ptr<TransportEdge> inbound_edge = nullptr;

  for (auto idx = 0; idx < solution_ultimate.size(); ++idx) {
    auto [current_node, previous_node, outbound_edge, distance_current] =
      solution_ultimate[idx];

    std::string current_node_code = current_node->code;

    DURATION latency =
      current_node
        ->get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();

    if (outbound_edge->movement == MovementType::CARTING)
      latency = current_node
                  ->get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();

    std::string current_node_arrival = date::format("%D %T", distance_current);
    nlohmann::json location_entry = {
      { "code", current_node_code },
      { "arrival", current_node_arrival },
      { "departure",
        date::format("%D %T",
                     get_departure(distance_current, outbound_edge->departure) +
                       latency) },
      { "route", outbound_edge->code.substr(0, outbound_edge->code.find('.')) }
    };
    response["ultimate"]["locations"].push_back(location_entry);

    if (idx == 0)
      response["ultimate"]["first"] = location_entry;
  }
  {
    auto start_location = solver.get_node(target);
    nlohmann::json location_entry = { { "code", solver.get_node(target)->code },
                                      { "arrival",
                                        date::format("%D %T", bag_pdd) } };
    response["ultimate"]["locations"].push_back(location_entry);
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

  while (true) {
    // app.logger().information("SolverWrapper polling....");
    try {
      Poco::Thread::sleep(200);
      std::string payloads[8];
      app.logger().debug(

        moirai::format("C: Queue size: {}", load_queue->size_approx()));
      if (size_t num_packages = load_queue->try_dequeue_bulk(payloads, 8);
          num_packages > 0) {
        std::for_each(
          std::execution::par,
          payloads,
          payloads + num_packages,
          [&app, this](const std::string& payload) {
            nlohmann::json data = nlohmann::json::parse(payload);
            app.logger().debug(
              moirai::format("Recieved data: {}", data.dump()));
            std::vector<std::tuple<std::string, int32_t, std::string>> packages;

            for (auto& waybill : data["items"]) {
              if (!waybill["cpdd_destination"].is_null() and
                  !waybill["cn"].is_null())
                packages.emplace_back(
                  waybill["cn"].template get<std::string>(),
                  iso_to_date(
                    waybill["cpdd_destination"].template get<std::string>())
                    .time_since_epoch()
                    .count(),
                  waybill["id"].template get<std::string>());
            }

            nlohmann::json solution =
              find_paths(data["id"].template get<std::string>(),
                         data["location"].template get<std::string>(),
                         data["destination"].template get<std::string>(),
                         iso_to_date(data["time"].template get<std::string>())
                           .time_since_epoch()
                           .count(),
                         packages);

            if (solution.empty()) {
              app.logger().information(
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
