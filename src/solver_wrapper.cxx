#include "solver_wrapper.hxx"
#include "date_utils.hxx"
#include "generator.hxx"
#include "processor.hxx"
#include "transportation.hxx"
#include "utils.hxx"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Util/ServerApplication.h>
#include <algorithm>
#include <coroutine>
#include <cstddef>
#include <execution>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fstream>
#include <istream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* loadQueue,
  moodycamel::ConcurrentQueue<std::string>* solutionQueue,
  std::shared_ptr<Solver> solver)
  : mSolverPtr(std::move(solver))
  , mLoadQueuePtr(loadQueue)
  , mSolutionQueuePtr(solutionQueue)
{
  // Poco::Util::Application& app = Poco::Util::Application::instance();
  mRunning = true;
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
  , mNodeFile(nodeFile)
#else
  , node_uri(node_uri)
  , node_idx(node_idx)
  , node_user(node_user)
  , node_pass(node_pass)
#endif
#ifdef WITH_EDGE_FILE
  , mEdgeFile(edgeFile)
#else
  , edge_uri(edge_uri)
  , edge_auth(edge_auth)
#endif
{
  // Poco::Util::Application& app = Poco::Util::Application::instance();
  mSolverPtr = std::make_shared<Solver>();
  init_nodes();
  init_edges();
  mRunning = true;
}

void
SolverWrapper::init_nodes()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  auto data = nlohmann::json::array();

#ifdef WITH_NODE_FILE
  std::ifstream infile{ mNodeFile, std::ios::in };
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
      query["search_after"] = data_l.back()["facility_code"];
    }
  } while (hits.size() > 0);
#endif

  std::unordered_map<std::string, std::vector<Node<Graph>>> colocatedNodes;

  std::for_each(
    data.begin(), data.end(), [this, &colocatedNodes](auto const& facility) {
      auto node = std::make_shared<TransportCenter>(
        facility["facility_code"],
        facility["name"],
        parse_time(facility["facility_attributes.CenterArrivalCutoff"]),
        MovementType::LINEHAUL,
        ProcessType::OUTBOUND,
        parse_time(facility["facility_attributes.OutboundProcessingTime"]));
      auto [vertex, has_vertex] = mSolverPtr->add_node(node);
      auto propertyId = facility["property_id"];

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
          added = mSolverPtr
                    ->add_edge(nodeI,
                               nodeJ,
                               std::make_shared<TransportEdge>(
                                 fmt::format("CUSTODY-{}-{}", nodeI, nodeJ),
                                 fmt::format("CUSTODY-{}-{}", nodeI, nodeJ)))
                    .second;
          if (not added) {
            app.logger().debug("Unable to add custody edge");
          }
          added = mSolverPtr
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
  auto data = nlohmann::json::array();
#ifdef WITH_EDGE_FILE
  std::ifstream infile{ mEdgeFile, std::ios::in };
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
    data = nlohmann::json::parse(response_stream)["data"];
  }
#endif
  load_edges(data);
}

generator<int> squires(int max) {
  for (int i = 0; i < max; i++) {
        co_yield i * i;
  }
}

auto
parse_route(const nlohmann::json& routeData)
{
  using RouteInfo = std::tuple<std::string,
                               VehicleType,
                               MovementType,
                               minutes,
                               time_of_day,
                               minutes,
                               minutes,
                               std::vector<uint8_t>,
                               std::string,
                               std::string>;
  auto eUUID = routeData["route_schedule_uuid"];
  auto eName = routeData["name"];
  auto eType = routeData["route_type"];
  auto eStop = routeData["halt_centers"];
  auto eDays =
    routeData["days_of_week"] | std::views::transform([](const auto& eDay) {
      return static_cast<uint8_t>(eDay);
    });

  auto eInit = (uint16_t)getJSONValue<double>(routeData, "reporting_time_ss");
  auto base = [](size_t idx, size_t sIdx, size_t n) -> size_t {
    size_t offset = idx * n - (idx * idx - idx) / 2;
    return offset + sIdx - idx - 1;
  };

  size_t numRoutes = eStop.size();
  numRoutes *= eStop.size() - 1;
  numRoutes /= 2;
  std::vector<RouteInfo> subRoutes;
  subRoutes.reserve(numRoutes);

  for (size_t idxS = 0; idxS < eStop.size(); ++idxS) {
    for (size_t idxT = idxS + 1; idxT < eStop.size(); ++idxT) {
      auto sNode = eStop[idxS];
      auto tNode = eStop[idxT];

      auto sRelArr = (uint16_t)getJSONValue<double>(sNode, "rel_eta_ss");
      auto tRelArr = (uint16_t)getJSONValue<double>(tNode, "rel_eta_ss");
      auto sRelDep = (uint16_t)getJSONValue<double>(sNode, "rel_etd_ss");
      auto tRelDep = (uint16_t)getJSONValue<double>(tNode, "rel_etd_ss");
      auto sNodeId = getJSONValue<std::string>(sNode, "center_code");
      auto tNodeId = getJSONValue<std::string>(tNode, "center_code");

      time_of_day eDep(minutes(eInit + sRelDep));
      auto eDur = minutes(tRelArr - sRelDep);
      auto eOut = minutes(sRelDep - sRelArr);
      auto eInb = minutes(tRelDep - tRelArr);

      if (idxS > 0) {
        eOut /= 2;
      }

      if (idxT == eStop.size() - 1) {
        eInb /= 2;
      }

      auto routeInfo = std::make_tuple(
        fmt::format("{}.{}", eUUID, base(idxS, idxT, eStop.size())),
        eName,
        eType == "air" ? VehicleType::AIR : VehicleType::SURFACE,
        eType == "carting" ? MovementType::CARTING : MovementType::LINEHAUL,
        eOut,
        eDep,
        eDur,
        eInb,
        eDays,
        sNodeId,
        tNodeId);
      subRoutes.push_back(routeInfo);
    }
  }
  return subRoutes;
}

void
SolverWrapper::load_edges(const nlohmann::json& data)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();

  std::for_each(data.begin(), data.end(), [this, &app](auto const& route) {
    auto subRoutes = parse_route(route);

    for (const auto& subRoute : subRoutes) {
      auto [eIdx,
            eVehicle,
            eMovement,
            eOut,
            eDep,
            eDur,
            eInb,
            eDays,
            eSNode,
            eTNode] = subRoute;
      auto [sNodeDesc, hasSNode] = mSolverPtr->add_node(eSNode);
      auto [tNodeDesc, hasTNode] = mSolverPtr->add_node(eTNode);

      if (hasSNode and hasTNode) {
        std::shared_ptr<TransportCenter> sNodeAttr =
          mSolverPtr->get_node(sNodeDesc);
        auto tNodeAttr = mSolverPtr->get_node(tNodeDesc);
        auto edge = std::make_shared<TransportEdge>(
          eIdx,
          eIdx,
          eVehicle,
          eMovement,
          sNodeAttr->latency(eMovement, ProcessType::OUTBOUND),
          tNodeAttr->latency(eMovement, ProcessType::INBOUND),
          eOut,
          eDep,
          eDur,
          eInb,
          eDays);
        auto [edgeDesc, eAdded] =
          mSolverPtr->add_edge(sNodeDesc, tNodeDesc, edge);

        if (!eAdded) {
          app.logger().debug("Unable to add edge");
        }
      }
    }
  });
}

auto
SolverWrapper::find_paths(const std::string& item,
                          const std::string& item_source_idx,
                          const std::string& item_target_idx,
                          const datetime item_start,
                          const datetime item_target_limit,
                          auto& subItems) const -> nlohmann::json
{
  // Poco::Util::Application& app = Poco::Util::Application::instance();
  datetime mustReachBy = item_target_limit;
  nlohmann::json response = { { "_id", item },
                              { "waybill", item },
                              { "package", item } };

  std::vector<Segment> fRoute;
  std::vector<Segment> cRoute;

  datetime tArrival{ datetime::max() };

  auto [sNodeDesc, hasSNode] = mSolverPtr->add_node(item_source_idx);
  auto [tNodeDesc, hasTNode] = mSolverPtr->add_node(item_target_idx);

  if (hasSNode and hasTNode) {
    fRoute =
      mSolverPtr->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        sNodeDesc, tNodeDesc, item_start);

    if (not fRoute.empty()) {
      auto fRouteTArr = fRoute.back().distance();

      if (fRouteTArr < mustReachBy) {
        auto cSubRoutes =
          subItems | std::views::filter([&fRouteTArr](auto const& subItem) {
            return subItem.reach_by() > fRouteTArr;
          }) |
          std::views::filter([this](const auto& subItem) {
            return mSolverPtr->add_node(subItem.target()).second;
          }) |
          std::views::transform([&tNodeDesc, this](const auto& subItem) {
            return mSolverPtr
              ->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
                mSolverPtr->add_node(subItem.target()).first,
                tNodeDesc,
                subItem.reach_by());
          }) |
          std::views::filter(
            [](auto const& cSubRoute) { return not cSubRoute.empty(); });
        auto cSubRoute =
          *(std::min_element(cSubRoutes.begin(),
                             cSubRoutes.end(),
                             [](const auto& lhs, const auto& rhs) {
                               return lhs.back().distance() <
                                      rhs.back().distance();
                             }));

        if (cSubRoute.back().distance() > fRouteTArr) {
          cRoute =
            mSolverPtr
              ->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
                tNodeDesc, sNodeDesc, mustReachBy);
        }
      }
    }
  }
  /*

  if (fRoute.size() > 0) {
    auto item_path_e = parse_path(fRoute);
    response["earliest"] = { { "locations", item_path_e },
                             { "first", item_path_e.front() } };

    if (fRoute.size() > 1)
      response["earliest"]["second"] = item_path_e[1];
  } else {
    response["error"] =
      fmt::format("No route to destination found from {} to {}",
                  item_source_idx,
                  item_target_idx);
  }

  if (cRoute.size() > 0) {
    auto item_path_u = parse_path(cRoute);
    response["ultimate"] = { { "locations", item_path_u },
                             { "first", item_path_u.front() } };

    if (cRoute.size() > 1)
      response["ultimate"]["second"] = item_path_u[1];
  }
}
else response["error"] = fmt::format(
  "{}{}{}",
  hasSNode ? "" : fmt::format("Unknown source<{}>", item_source_idx),
  hasSNode or hasTNode ? "" : ", ",
  hasTNode ? "" : fmt::format("Unknown target<{}>", item_target_idx));

response["pdd"] = date::format("%D %T", mustReachBy);
response["pdd_ts"] = mustReachBy.time_since_epoch().count();
*/
  return response;
}

void
SolverWrapper::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  app.logger().debug("Initializing solver");
  app.logger().debug("Processing loads");

  constexpr uint sleepFor = 200;
  constexpr uint batchSize = 64;

  while (mRunning or mLoadQueuePtr->size_approx() > 0) {
    try {
      Poco::Thread::sleep(sleepFor);

      std::vector<std::string> payloads;
      payloads.reserve(batchSize);

      if (size_t nItems =
            mLoadQueuePtr->try_dequeue_bulk(payloads.begin(), batchSize);
          nItems > 0) {
        std::for_each(
          payloads.begin(),
          payloads.end(),
          [&app, this](const std::string& payload) {
            nlohmann::json data = nlohmann::json::parse(payload);

            std::list<std::string> requiredFields{
              "id", "location", "destination", "time"
            };

            auto isNull =
              std::any_of(requiredFields.begin(),
                          requiredFields.end(),
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
              std::views::transform(
                [](const auto& item) -> TransportationLoadAttributes {
                  return { item["id"],
                           item["cn"],
                           parse_datetime(item["ipdd_destination"]) };
                });

            std::vector<TransportationLoadAttributes> subItemsVec;

            for (const auto subItem : subItems) {
              subItemsVec.push_back(subItem);
            }

            std::string iIdx = data["id"];
            std::string iSrc = data["location"];
            std::string iTar = data["destination"];
            std::string iArr = data["time"];
            datetime iReachBy;

            if (data["ipdd_destination"].is_null() or
                data["ipdd_destination"].empty()) {
              iReachBy = datetime::max();
            } else {
              iReachBy = parse_datetime(data["ipdd_destination"]);
            }

            auto solution = find_paths(
              iIdx, iSrc, iTar, parse_datetime(iArr), iReachBy, subItems);

            auto rawCSlid = data["cs_slid"];
            auto rawCSAct = data["cs_act"];
            auto rawPid = data["pid"];

            if (not(rawCSlid.empty() or rawCSlid.is_null())) {
              solution["cs_slid"] = rawCSlid;
            }

            if (not(rawCSAct.empty() or rawCSAct.is_null())) {
              solution["cs_act"] = rawCSAct;
            }

            if (not(rawPid.empty() or rawPid.is_null()))
              solution["pid"] = rawPid;
            mSolutionQueuePtr->enqueue(solution.dump());
          });
      }
    } catch (const std::exception& exc) {
      app.logger().error(fmt::format("Exception occurred: {}", exc.what()));
    }
  }
}
