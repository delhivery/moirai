#include "transportation.hxx"
#include <utility>

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
  mLatencies[std::make_pair(movement, process)] = duration;
}

TransportEdge::TransportEdge(std::string code, std::string name)
  : mCode(std::move(code))
  , mName(std::move(name))
  , mVehicle(VehicleType::SURFACE)
  , mMovement(MovementType::CARTING)
{
}

TransportEdge::TransportEdge(std::string code,
                             std::string name,
                             const VehicleType& vehicle,
                             const MovementType& movement,
                             const minutes& outDockSource,
                             const minutes& inDockTarget,
                             const minutes& loadingTime,
                             const time_of_day& departure,
                             const minutes& duration,
                             const minutes& unloadingTime,
                             const std::vector<uint8_t>& workingDays)
  : TemporalEdgeCostAttributes(outDockSource + loadingTime,
                               departure,
                               duration,
                               unloadingTime + inDockTarget,
                               workingDays)
  , mCode(std::move(code))
  , mName(std::move(name))
  , mVehicle(vehicle)
  , mMovement(movement)
  , mOutDockSource(outDockSource)
  , mInDockTarget(inDockTarget)
{
}
