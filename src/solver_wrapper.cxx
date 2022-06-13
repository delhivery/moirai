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
#include <list>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* loadQueue,
  moodycamel::ConcurrentQueue<std::string>* solutionQueue,
  std::shared_ptr<Solver> solver)
  : solver(solver)
  , mLoadQueuePtr(loadQueue)
  , mSolutionQueuePtr(solutionQueue)
{
  // Poco::Util::Application& app = Poco::Util::Application::instance();
  running = true;
}

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* loadQueuePtr,
  moodycamel::ConcurrentQueue<std::string>* solutionQueuePtr
#ifdef WITH_NODE_FILE
  ,
  std::filesystem::path& nodeFile
#else
  ,
  const std::string& node_uri,
  const std::string& node_idx,
  const std::string& node_user,
  const std::string& node_pass
#endif
#ifdef WITH_EDGE_FILE
  ,
  std::filesystem::path& edgeFile
#else
  ,
  const std::string& edge_uri,
  const std::string& edge_auth
#endif
  )
  : mLoadQueuePtr(loadQueuePtr)
  , mSolutionQueuePtr(solutionQueuePtr)
#ifdef WITH_NODE_FILE
  , nodeFile(nodeFile)
#else
  , node_uri(node_uri)
  , node_idx(node_idx)
  , node_user(node_user)
  , node_pass(node_pass)
#endif
#ifdef WITH_EDGE_FILE
  , edgeFile(edgeFile)
#else
  , edge_uri(edge_uri)
  , edge_auth(edge_auth)
#endif
{
  // Poco::Util::Application& app = Poco::Util::Application::instance();
  solver = std::make_shared<Solver>();
  init_nodes();
  init_custody();
  init_edges();
  running = true;
}

void
SolverWrapper::init_nodes()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  auto data = nlohmann::json::array();

#ifdef WITH_NODE_FILE
  std::ifstream infile{ nodeFile, std::ios::in };
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

  std::unordered_map<std::string, std::vector<Node<Graph>>> colocatedNodes;

  std::for_each(
    data.begin(), data.end(), [this, &colocatedNodes](auto const& facility) {
      auto index = getJSONValue<std::string>(facility, "facility_code");
      auto name = getJSONValue<std::string>(facility, "name");
      auto outProcessTime = getJSONValue<std::string>(
        facility, "facility_attributes.OutboundProcessingTime");
      auto arrivalCutoff = getJSONValue<std::string>(
        facility, "facility_attributes.CenterArrivalCutoff");
      auto propertyId = getJSONValue<std::string>(facility, "property_id");

      auto node = std::make_shared<TransportCenter>(
        index, name, parse_time(arrivalCutoff));
      node->template set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(
        parse_time(outProcessTime));

      auto [vertex, has_vertex] = solver->add_node(node);

      if (not propertyId.empty() && has_vertex) {
        if (not colocatedNodes.contains(propertyId)) {
          colocatedNodes[propertyId] = std::vector<Node<Graph>>{};
        }
        colocatedNodes[propertyId].emplace_back(vertex);
      }
    });

  std::for_each(
    colocatedNodes.begin(),
    colocatedNodes.end(),
    [this, &app](const auto& entry) {
      const auto [group_id, nodes] = entry;
      for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
          auto nodeI = nodes[i];
          auto nodeJ = nodes[j];
          bool added = false;
          added = solver
                    ->add_edge(nodeI,
                               nodeJ,
                               std::make_shared<TransportEdge>(
                                 fmt::format("CUSTODY-{}-{}", nodeI, nodeJ),
                                 fmt::format("CUSTODY-{}-{}", nodeI, nodeJ)))
                    .second;
          if (not added) {
            app.logger().debug("Unable to add custody edge");
          }
          added = solver
                    ->add_edge(nodeJ,
                               nodeI,
                               std::make_shared<TransportEdge>(
                                 fmt::format("CUSTODY-{}-{}", nodeJ, nodeI),
                                 fmt::format("CUSTODY-{}-{}", nodeJ, nodeI)))
                    .second;

          if (not added) {
            app.logger().debug("Unable to add rev custody edge");
          }
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
  std::ifstream infile{ edgeFile, std::ios::in };
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
          edge_loading,
          edge_unloading,
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
  const std::string& item,
  const std::string& item_source_idx,
  const std::string& item_target_idx,
  const datetime item_start,
  const datetime item_target_limit,
  const std::vector<TransportationLoadAttributes>& subitems) const
  -> nlohmann::json
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  datetime item_reach_by = item_target_limit;
  nlohmann::json response = { { "_id", item },
                              { "waybill", item },
                              { "package", item } };

  std::vector<Segment> item_route_e, item_route_u;

  datetime item_target_arrival_e{ datetime::max() };

  auto [item_source, has_item_source] = solver->add_node(item_source_idx);
  auto [item_target, has_item_target] = solver->add_node(item_target_idx);

  if (has_item_source and has_item_target) {
    item_route_e =
      solver->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        item_source, item_target, item_start);

    if (item_route_e.size() > 0) {
      auto item_arrival_target_e = item_route_e.back().distance();

      if (item_arrival_target_e < item_reach_by) {
        std::vector<std::vector<Segment>> subitems_route_u;
        subitems_route_u.reserve(subitems.size());
        auto subitems_reach_by_min =
          std::min_element(subitems.begin(),
                           subitems.end(),
                           [](const auto& lhs, const auto& rhs) {
                             return lhs.reach_by() < rhs.reach_by();
                           });

        if (subitems_reach_by_min->reach_by() > item_arrival_target_e) {
          std::for_each(
            subitems.begin(),
            subitems.end(),
            [&item_target, &subitems_route_u, this](auto& subitem) {
              const auto [subitem_target, has_subitem_target] =
                solver->add_node(subitem.m_target_idx);

              if (has_subitem_target)
                subitems_route_u.push_back(
                  find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
                    subitem_target, item_target, subitem.m_reach_by));
            });

          auto subitems_depart_by_min = std::min_element(
            subitems_route_u.begin(),
            subitems_route_u.end(),
            [](auto const& lhs, auto const& rhs) {
              return lhs.back().m_distance < rhs.back().m_distance;
            });

          if (subitems_depart_by_min->back().mDistance >
              item_arrival_target_e) {
            item_reach_by = subitems_depart_by_min->back().mDistance;
            item_route_u =
              solver
                ->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
                  item_target,
                  item_source,
                  subitems_depart_by_min->back().mDistance);
          }
        }
      }
    }

    if (item_route_e.size() > 0) {
      auto item_path_e = parse_path(item_route_e);
      response["earliest"] = { { "locations", item_path_e },
                               { "first", item_path_e.front() } };

      if (item_route_e.size() > 1)
        response["earliest"]["second"] = item_path_e[1];
    } else {
      response["error"] =
        fmt::format("No route to destination found from {} to {}",
                    item_source_idx,
                    item_target_idx);
    }

    if (item_route_u.size() > 0) {
      auto item_path_u = parse_path(item_route_u);
      response["ultimate"] = { { "locations", item_path_u },
                               { "first", item_path_u.front() } };

      if (item_route_u.size() > 1)
        response["ultimate"]["second"] = item_path_u[1];
    }
  } else
    response["error"] = fmt::format(
      "{}{}{}",
      has_item_source ? "" : fmt::format("Unknown source<{}>", item_source_idx),
      has_item_source or has_item_target ? "" : ", ",
      has_item_target ? ""
                      : fmt::format("Unknown target<{}>", item_target_idx));

  response["pdd"] = date::format("%D %T", item_reach_by);
  response["pdd_ts"] = item_reach_by.time_since_epoch().count();

  return response;
}

void
SolverWrapper::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  app.logger().debug("Initializing solver");
  app.logger().debug("Processing loads");

  while (running or mLoadQueuePtr->size_approx() > 0) {
    try {
      Poco::Thread::sleep(200);

      if (mSolutionQueuePtr->size_approx() > 10000)
        continue;
      std::string payloads[100];

      if (size_t num_packages = mLoadQueuePtr->try_dequeue_bulk(payloads, 64);
          num_packages > 0) {
        std::for_each(
          payloads,
          payloads + num_packages,
          [&app, this](const std::string& payload) {
            nlohmann::json data = nlohmann::json::parse(payload);

            std::list<std::string> item_required_fields{
              "id", "location", "destination", "time"
            };

            auto isNull =
              std::any_of(item_required_fields.begin(),
                          item_required_fields.end(),
                          [&data](const std::string& key) {
                            return data[key].is_null() || data[key].empty();
                          });

            if (isNull) {
              app.logger().debug(fmt::format(
                "Null data against mandatory fields. {}", data.dump()));
              return;
            }

            auto subItems =
              data["item"] | std::views::filter([](const auto& item) {
                return not(item["id"].is_null() or
                           item["ipdd_destination"].is_null() or
                           item["cn"].is_null());
              }) |
              std::views::transform([](const auto& item) {
                return TransportationLoadSubItem{
                  item["id"].template get<std::string>(),
                  item["ipdd_destination"].template get<std::string>(),
                  item["cn"].template get<std::string>()
                };
              }) |
              std::views::to_vector;
            std::string item_idx = data["id"];
            std::string item_src = data["location"];
            std::string item_tar = data["destination"];
            std::string item_arr = data["time"];
            datetime item_reach_by;

            if (data["ipdd_destination"].is_null() or
                data["ipdd_destination"].empty())
              item_reach_by = datetime::max();
            else
              item_arrive_by = date::parse(data["ipdd_destination"]);

            auto solution = find_paths(
              item_idx, item_src, item_tar, item_arr, item_arr_by, subItems);

            auto cs_slid = data["cs_slid"];
            auto cs_act = data["cs_act"];
            auto pid = data["pid"];

            if (not(cs_slid.empty() or cs_slid.is_null()))
              solution["cs_slid"] = cs_slid;

            if (not(cs_act.empty() or cs_act.is_null()))
              solution["cs_act"] = cs_act;

            if (not(pid.empty() or pid.is_null()))
              solution["pid"] = pid;
            solution_queue->enqueue(solution.dump());
          });
      }
    } catch (const std::exception& exc) {
      app.logger().error(fmt::format("Exception occurred: {}", exc.what()));
    }
  }
}
