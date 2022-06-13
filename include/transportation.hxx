#ifndef MOIRAI_TRANSPORTATION
#define MOIRAI_TRANSPORTATION

#include "date_utils.hxx" // for DURATION, COST, TIME_OF_DAY
#include "edge_cost_attributes.hxx"
#include <cstdint> // for uint8_t
#include <map>     // for map, map<>::mapped_type
#include <memory>  // for shared_ptr
#include <string>  // for string
#include <utility> // for pair, make_pair

enum VehicleType : std::uint8_t
{
  SURFACE = 0,
  AIR = 1,
};

enum MovementType : std::uint8_t
{
  CARTING = 0,
  LINEHAUL = 1,
};

enum ProcessType : std::uint8_t
{
  INBOUND = 0,
  OUTBOUND = 1,
  CUSTODY = 2,
};

template<MovementType, ProcessType>
using Latency = minutes;

class TransportCenter
{
private:
  std::string mCode, mName;
  std::map<std::pair<MovementType, ProcessType>, minutes> mLatencies;
  time_of_day mCutoff;

public:
  TransportCenter() = default;

  TransportCenter(std::string,
                  std::string,
                  time_of_day,
                  MovementType,
                  ProcessType,
                  minutes);

  [[nodiscard]] auto code() const -> std::string;

  template<MovementType M, ProcessType P>
  void set_latency(Latency<M, P> latency);
  /*
  {
    mLatencies[std::make_pair(M, P)] = latency;
  }
  */

  template<MovementType M, ProcessType P>
  auto get_latency() -> Latency<M, P>;
  /*
  {
    auto const key = std::make_pair(M, P);

    return mLatencies.contains(key) ? mLatencies[key] : Latency<M, P>(0);
  }
  */
  auto cutoff() -> time_of_day; // { return mCutoff; }
};

class TransportEdge : public TemporalEdgeCostAttributes
{
private:
  std::string mCode;
  std::string mName;

  VehicleType mVehicle;
  MovementType mMovement;

  minutes mOutDockSource;
  minutes mInDockTarget;

public:
  TransportEdge(std::string, std::string);

  TransportEdge(std::string,
                std::string,
                const VehicleType&,
                const MovementType&,
                const minutes&,
                const minutes&,
                const minutes&,
                const time_of_day&,
                const minutes&,
                const minutes&,
                const std::vector<uint8_t>&);

  [[nodiscard]] auto code() const -> std::string;

  [[nodiscard]] auto vehicle() const -> VehicleType;
};

class TransportationLoadAttributes
{
private:
  std::string mIdx;
  std::string mTargetIdx;
  datetime mReachBy;

public:
  TransportationLoadAttributes();

  TransportationLoadAttributes(std::string, std::string, datetime);

  [[nodiscard]] auto reach_by() const -> datetime;
};

#endif
