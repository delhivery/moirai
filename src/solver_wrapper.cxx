#include "solver_wrapper.hxx"
#include "date_utils.hxx"
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
#include <execution>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fstream>
#include <istream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

constexpr uint16_t OFFSET_IST = -1 * 330 * 60;

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* load_queue,
  moodycamel::ConcurrentQueue<std::string>* solution_queue,
  const std::shared_ptr<Solver> solver,
  const std::filesystem::path& center_timings_filename)
  : load_queue(load_queue)
  , solution_queue(solution_queue)
  , solver(solver)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  running = true;
}

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* load_queue,
  moodycamel::ConcurrentQueue<std::string>* solution_queue
#ifdef WITH_NODE_FILE
  ,
  std::filesystem::path& node_file
#else
  ,
  const std::string& node_uri,
  const std::string& node_idx,
  const std::string& node_user,
  const std::string& node_pass
#endif
#ifdef WITH_EDGE_FILE
  ,
  std::filesystem::path& edge_file
#else
  ,
  const std::string& edge_uri,
  const std::string& edge_auth
#endif
  )
  : load_queue(load_queue)
  , solution_queue(solution_queue)
#ifdef WITH_NODE_FILE
  , node_file(node_file)
#else
  , node_uri(node_uri)
  , node_idx(node_idx)
  , node_user(node_user)
  , node_pass(node_pass)
#endif
#ifdef WITH_EDGE_FILE
  , edge_file(edge_file)
#else
  , edge_uri(edge_uri)
  , edge_auth(edge_auth)
#endif
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  solver = std::make_shared<Solver>();
  init_nodes();
  init_custody();
  init_edges();
  app.logger().debug(fmt::format("Initialized graph: {}", solver->show()));
  running = true;
}

void
SolverWrapper::init_nodes()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  auto data = nlohmann::json::array();

#ifdef WITH_NODE_FILE
  std::ifstream infile{ node_file, std::ios::in };
  infile >> data;
  infile.close();
#else
  Poco::Net::HTTPSClientSession session(node_uri.getHost(), node_uri.getPort());
  std::vector<std::string> hits{};

  nlohmann::json query = { { "query",
                             { "term", { "active", { "value", true } } },
                             "_source",
                             {
                               "name",
                               "facility_code",
                               "property_id",
                               "facility_attributes.OutboundProcessingTime",
                               "facility_attributes.CenterArrivalCutoff",
                             },
                             "sort",
                             { "facility_code:asc" } } };
  do {
    std::string query_string{ query.dump() };
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET,
                                   fmt::format("{}/_search", node_idx),
                                   Poco::Net::HTTPMessage::HTTP_1_1);
    request.setCredentials("Basic",
                           getEncodedCredentials(node_user, node_pass));
    request.setContentType("application/json");
    request.setContentLength(query_string.size());
    session.sendRequest(request) << query_string;

    Poco::Net::HTTPResponse response;
    std::istream& response_stream = session.receiveResponse(response);

    if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
      auto response_json = nlohmann::json::parse(response_stream);
      auto data_l = response_json["hits"]["hits"];

      if (data.empty())
        data = data_l;
      else {
        data.insert(data.begin(), data_l.begin(), data_l.end());
      }
      query["search_after"] = getJSONValue<std::string>(
        getJSONValue<nlohmann::json>(data_l, data_l.size() - 1),
        "facility_code");
    }
  } while (hits.size() > 0);
#endif

  std::for_each(data.begin(), data.end(), [this](auto const& facility) {
    std::string node_id, node_name, node_lh_o, node_arrc, node_prop;

    node_id = getJSONValue<std::string>(facility, "facility_code");
    node_name = getJSONValue<std::string>(facility, "name");
    node_lh_o = getJSONValue<std::string>(
      facility, "facility_attributes.OutboundProcessingTime");
    node_arrc = getJSONValue<std::string>(
      facility, "facility_attributes.CenterArrivalCutoff");
    node_prop = getJSONValue<std::string>(facility, "property_id");

    auto node = std::make_shared<TransportCenter>(node_id, node_name);
    node->set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(
      time_string_to_time(node_lh_o));
    node->set_cutoff(time_string_to_time(node_arrc));
    auto [vertex, has_vertex] = solver->add_node(node);

    if (!node_prop.empty() && has_vertex) {
      if (!colocated_nodes.contains(node_prop))
        colocated_nodes[node_prop] = std::vector<Node<Graph>>{};
      colocated_nodes[node_prop].emplace_back(vertex);
    }
  });

  std::for_each(
    colocated_nodes.begin(), colocated_nodes.end(), [this](const auto& entry) {
      const auto [group_id, nodes] = entry;
      for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
          auto node_i = nodes[i];
          auto node_j = nodes[j];

          solver->add_edge(node_i,
                           node_j,
                           std::make_shared<TransportEdge>(
                             fmt::format("CUSTODY-{}-{}", node_i, node_j),
                             fmt::format("CUSTODY-{}-{}", node_i, node_j)));

          solver->add_edge(node_j,
                           node_i,
                           std::make_shared<TransportEdge>(
                             fmt::format("CUSTODY-{}-{}", node_j, node_i),
                             fmt::format("CUSTODY-{}-{}", node_j, node_i)));
        }
      }
    });
}

void
SolverWrapper::init_edges()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  auto data = nlohmann::json::array();
#ifdef WITH_EDGE_FILE
  std::ifstream infile{ edge_file, std::ios::in };
  infile >> data;
  infile.close();
#else
  std::string path(edge_uri.getPathAndQuery());

  if (path.empty())
    path = "/";

  Poco::Net::HTTPSClientSession session(edge_uri.getHost(), edge_uri.getPort());
  Poco::Net::HTTPRequest request(
    Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
  request.setCredentials("Bearer", edge_auth);
  session.sendRequest(request);

  Poco::Net::HTTPResponse response;
  std::istream& response_stream = session.receiveResponse(response);

  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK) {
    data = getJSONValue<nlohmann::json>(nlohmann::json::parse(response_stream),
                                        "data");
  }
#endif

  std::for_each(data.begin(), data.end(), [this](auto const& route) {
    auto edge_id = getJSONValue<std::string>(route, "route_schedule_uuid");
    auto edge_name = getJSONValue<std::string>(route, "name");
    auto edge_type = getJSONValue<std::string>(route, "route_type");
    auto offset =
      static_cast<uint16_t>(getJSONValue<double>(route, "reporting_time_ss")) +
      OFFSET_IST;
    auto stops = getJSONValue<nlohmann::json>(route, "halt_centers");
    // TIME_OF_DAY offset{ reporting_time - 330 * 60 };
    uint16_t n_stops = stops.size();

    for (size_t idx_s = 0; idx_s < n_stops; ++idx_s)
      for (size_t idx_t = idx_s + 1; idx_t < n_stops; ++idx_t) {
        auto source = stops[idx_s];
        auto target = stops[idx_t];

        auto source_rel_arr =
          static_cast<uint16_t>(getJSONValue<double>(source, "rel_eta_ss"));
        auto source_rel_dep =
          static_cast<uint16_t>(getJSONValue<double>(source, "rel_etd_ss"));
        auto target_rel_arr =
          static_cast<uint16_t>(getJSONValue<double>(target, "rel_eta_ss"));
        auto target_rel_dep =
          static_cast<uint16_t>(getJSONValue<double>(target, "rel_etd_ss"));

        // auto terminal = idx_t == stops.size() - 1;

        auto edge_dep = offset + source_rel_dep;
        auto edge_dur = target_rel_arr - source_rel_dep;
        auto edge_loading = source_rel_dep - source_rel_arr;
        auto edge_unloading = target_rel_dep - target_rel_arr;

        auto source_node_code = getJSONValue(source, "center_code");
        auto target_node_code = getJSONValue(target, "center_code");

        auto [source_node, has_source] = solver->add_node(source_node_code);
        auto [target_node, has_target] = solver->add_node(target_node_code);
        // start = NX - (N^2 - N)/2
        auto base = [](size_t idx, size_t n) {
          return idx * n - (idx * idx - idx) / 2;
        };

        auto edge = std::make_shared<TransportEdge>(
          fmt::format(
            "{}.{}", edge_id, base(idx_s, n_stops - 1) + idx_t - idx_s - 1),
          edge_name,
          edge_dep,
          edge_dur,
          edge_dur_loading,
          edge_dur_unloading,
          edge_type == "air" ? VehicleType::AIR : VehicleType::SURFACE,
          edge_type == "carting" ? MovementType::CARTING
                                 : MovementType::LINEHAUL,
          idx_t == stops.size() - 1);

        assert(edge_dep > 0 and edge_dur > 0);

        if (has_source and has_target)
          solver->add_edge(source_node, target_node, edge);
      }
  });
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
  app.logger().debug(fmt::format("Bag end: {:%Y-%m-%d %H:%M:%S}", bag_end));
  // bag_pdd = CLOCK::max();
  CLOCK bag_earliest_pdd = CLOCK::max();
  const auto [source, has_source] = solver->add_node(bag_source);
  const auto [target, has_target] = solver->add_node(bag_target);

  app.logger().debug("Finding path for {} with tmax: {}", bag, bag_end);

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

    app.logger().debug(fmt::format(
      "Bad pdd {:%Y-%m-%d %H:%M:%S} earliest_pdd: {:%Y-%m-%d %H:%M:%S}",
      bag_pdd,
      bag_earliest_pdd));
    if (bag_pdd < bag_earliest_pdd)
      critical = true;
    else if (packages.size() > 0) {
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

          if (bag_pdd == CLOCK::max())
            bag_pdd = child_pdd_at_parent_target;

          if (child_pdd_at_parent_target < bag_earliest_pdd) {
            critical = true;
          }
        }
      }
    }
  } else
    critical = true;

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

  if (auto earliest_path =
        parse_path<PathTraversalMode::FORWARD>(solution_earliest_start_segment);
      earliest_path.size() > 0) {
    response["earliest"]["locations"] = earliest_path;
    response["earliest"]["first"] = earliest_path[0];

    if (earliest_path.size() > 1)
      response["earliest"]["second"] = earliest_path[1];
  }

  if (auto ultimate_path =
        parse_path<PathTraversalMode::REVERSE>(solution_ultimate_start_segment);
      ultimate_path.size() > 0) {
    response["ultimate"]["locations"] = ultimate_path;
    response["ultimate"]["first"] = ultimate_path[0];

    if (ultimate_path.size() > 1)
      response["ultimate"]["second"] = ultimate_path[1];
  }

  response["pdd"] = date::format("%D %T", bag_pdd);
  response["pdd_ts"] = bag_pdd.time_since_epoch().count();

  return response;
}

void
SolverWrapper::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  app.logger().debug("Initializing solver");
  app.logger().debug("Processing loads");
  app.logger().debug(fmt::format("0. Facility timings has {} entries",
                                 facility_timings_map.size()));

  while (running or load_queue->size_approx() > 0) {
    try {
      Poco::Thread::sleep(200);

      if (solution_queue->size_approx() > 10000)
        continue;
      std::string payloads[100];
      // app.logger().debug(fmt::format("C: Queue size: {}",
      // load_queue->size_approx()));

      if (size_t num_packages = load_queue->try_dequeue_bulk(payloads, 64);
          num_packages > 0) {
        std::for_each(
          payloads,
          payloads + num_packages,
          [&app, this](const std::string& payload) {
            nlohmann::json data = nlohmann::json::parse(payload);

            if (data["id"].is_null() or data["location"].is_null() or
                data["destination"].is_null() or data["time"].is_null()) {
              app.logger().debug(fmt::format(
                "Null data against mandatory fields. {}", data.dump()));
              return;
            }

            app.logger().debug(fmt::format("Recieved data: {}", data.dump()));
            std::vector<std::tuple<std::string, int32_t, std::string>> packages;

            app.logger().debug(fmt::format("1. Facility timings has {} entries",
                                           facility_timings_map.size()));

            for (auto& waybill : data["items"]) {
              if (waybill["ipdd_destination"].is_null() or
                  waybill["cn"].is_null() or waybill["id"].is_null())
                return;
              auto waybill_cn = waybill["cn"].template get<std::string>();

              auto waybill_tmax = iso_to_date(
                waybill["ipdd_destination"].template get<std::string>(), true);

              if (facility_timings_map.contains(waybill_cn))
                waybill_tmax = iso_to_date(
                  waybill["ipdd_destination"].template get<std::string>(),
                  std::get<4>(facility_timings_map[waybill_cn]));

              packages.emplace_back(waybill_cn,
                                    waybill_tmax.time_since_epoch().count(),
                                    waybill["id"].template get<std::string>());
            }

            app.logger().debug(fmt::format("2. Facility timings has {} entries",
                                           facility_timings_map.size()));

            std::string bag_identifier = data["id"].template get<std::string>();
            std::string current_location =
              data["location"].template get<std::string>();
            std::string item_destination =
              data["destination"].template get<std::string>();

            if (current_location.empty() or item_destination.empty() or
                current_location == item_destination)
              return;

            CLOCK tmax = CLOCK::max();
            app.logger().debug(fmt::format("3. Facility timings has {} entries",
                                           facility_timings_map.size()));

            if (!data["ipdd_destination"].is_null() and
                !data["ipdd_destination"].empty()) {
              app.logger().debug(
                fmt::format("4. Facility timings has {} entries",
                            facility_timings_map.size()));
              std::string ipdd =
                data["ipdd_destination"].template get<std::string>();

              app.logger().debug(
                fmt::format("5. Facility timings has {} entries",
                            facility_timings_map.size()));
              if (ipdd.length() > 11) {
                tmax = iso_to_date(ipdd, true);
                app.logger().debug(
                  fmt::format("6. Facility timings has {} entries",
                              facility_timings_map.size()));

                if (facility_timings_map.contains(item_destination))
                  tmax = iso_to_date(
                    ipdd, std::get<4>(facility_timings_map[item_destination]));
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
                fmt::format("No legitimate paths for payload: {}", payload));
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
      app.logger().error(fmt::format("Exception occurred: {}", exc.what()));
    }
  }
}
