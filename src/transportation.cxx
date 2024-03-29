#include "transportation.hxx"
// #include <utility>

TransportCenter::TransportCenter(std::string code,
                                 std::string name,
                                 time_of_day cutoff,
                                 MovementType movement,
                                 ProcessType process,
                                 minutes duration)
  : mCode(std::move(code))
  , mName(std::move(name))
  , mCutoff(cutoff)
{

  constexpr auto hashLatencies = [](const LatencyType& latency) -> size_t {
    return latency.first << sizeof(MovementType) * CHAR_BIT | latency.second;
  };

  mLatencies = LatencyMap(0, hashLatencies);
  mLatencies[std::make_pair(movement, process)] = duration;
}

void
TransportCenter::latency(MovementType movement,
                         ProcessType process,
                         minutes lat)
{
  mLatencies.emplace(std::make_pair(movement, process), lat);
}

auto
TransportCenter::code() const -> std::string
{
  return mCode;
}

auto
TransportCenter::latency(MovementType movementType,
                         ProcessType processType) const -> minutes
{
  std::pair<MovementType, ProcessType> key(movementType, processType);

  if (mLatencies.contains(key)) {
    return mLatencies.at(key);
  }
  return minutes(0);
}

auto
TransportCenter::cutoff() const -> time_of_day
{
  return mCutoff;
}

TransportEdge::TransportEdge(std::string code, std::string name)
  : mCode(std::move(code))
  , mName(std::move(name))
  , mVehicle(VehicleType::SURFACE)
  , mMovement(MovementType::CARTING)
{
}

auto
TransportEdge::code() const -> std::string
{
  return mCode;
}

auto
TransportEdge::vehicle() const -> VehicleType
{
  return mVehicle;
}

TransportationLoadAttributes::TransportationLoadAttributes(std::string idx,
                                                           std::string target,
                                                           datetime reachBy)
  : mIdx(std::move(idx))
  , mTargetIdx(std::move(target))
  , mReachBy(std::move(reachBy))
{
}

auto
TransportationLoadAttributes::idx() const -> std::string
{
  return mIdx;
}

auto
TransportationLoadAttributes::target() const -> std::string
{
  return mTargetIdx;
}

auto
TransportationLoadAttributes::reach_by() const -> datetime
{
  return mReachBy;
}
