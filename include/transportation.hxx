#ifndef MOIRAI_TRANSPORTATION
#define MOIRAI_TRANSPORTATION

#include "date.hxx"
#include "edge.hxx"
#include <climits>
#include <functional>
#include <string>
#include <unordered_map>

enum VehicleType : uint8_t {
  SURFACE = 0,
  AIR = 1,
};

enum MovementType : uint8_t {
  CARTING = 0,
  LINEHAUL = 1,
};

enum ProcessType : uint8_t {
  INBOUND = 0,
  OUTBOUND = 1,
  CUSTODY = 2,
};

using LatencyType = std::pair<MovementType, ProcessType>;

using LatencyHash = std::function<size_t(const LatencyType &)>;

using LatencyMap = std::unordered_map<LatencyType, minutes, LatencyHash>;

class TransportCenter {
private:
  std::string mCode;
  std::string mName;
  std::vector<minutes> mLatencies;
  time_of_day mCutoff;

public:
  TransportCenter() = default;

  TransportCenter(std::string, std::string, time_of_day, MovementType,
                  ProcessType, minutes);

  [[nodiscard]] auto code() const -> std::string;

  void latency(MovementType, ProcessType, minutes);

  [[nodiscard]] auto latency(MovementType, ProcessType) const -> minutes;

  [[nodiscard]] auto cutoff() const -> time_of_day; // { return mCutoff; }
};

class TransportEdge : public CostAttributes {
private:
  std::string mCode;
  std::string mName;

  VehicleType mVehicle;
  MovementType mMovement;

  minutes mOutDockSource;
  minutes mInDockTarget;

public:
  TransportEdge(std::string, std::string);

  TransportEdge(std::string,                 // code
                std::string,                 // name
                const VehicleType &,         // vehicle
                const MovementType &,        // movement
                const minutes &,             // outDockSource
                const minutes &,             // inDockTarget
                const minutes &,             // loadingTime
                const time_of_day &,         // departure
                const minutes &,             // duration
                const minutes &,             // unloadingTime
                const std::vector<uint8_t> & // workingDays
  );

  [[nodiscard]] auto code() const -> std::string;

  [[nodiscard]] auto vehicle() const -> VehicleType;
};

template <range_of<uint8_t> range_t> class TransportationEdgeAttributes {
private:
  std::string mCode;
  std::string mName;
  VehicleType mVehicle;
  MovementType mMovement;
  minutes mOutSource;
  time_of_day mDepSource;
  minutes mDuration;
  minutes mInTarget;
  range_t mWorkingDays;
  std::string mSource;
  std::string mTarget;

public:
  TransportationEdgeAttributes() = delete;

  TransportationEdgeAttributes(std::string code, std::string name,
                               VehicleType vehicle, MovementType movement,
                               minutes outSource, time_of_day depSource,
                               minutes duration, minutes inTarget,
                               range_t workingDays, std::string source,
                               std::string target)
      : mCode(code), mName(name), mVehicle(vehicle), mMovement(movement),
        mOutSource(outSource), mDepSource(depSource), mDuration(duration),
        mInTarget(inTarget), mWorkingDays(workingDays), mSource(source),
        mTarget(target) {}

  [[nodiscard]] auto code() const -> std::string { return mCode; }

  [[nodiscard]] auto name() const -> std::string { return mName; }

  [[nodiscard]] auto vehicle() const -> VehicleType { return mVehicle; }

  [[nodiscard]] auto movement() const -> MovementType { return mMovement; }

  [[nodiscard]] auto out_source() const -> minutes { return mOutSource; }

  [[nodiscard]] auto departure() const -> time_of_day { return mDepSource; }

  [[nodiscard]] auto duration() const -> minutes { return mDuration; }

  [[nodiscard]] auto in_target() const -> minutes { return mInTarget; }

  [[nodiscard]] auto working_days() const -> range_t { return mWorkingDays; }

  [[nodiscard]] auto source() const -> std::string { return mSource; }

  [[nodiscard]] auto target() const -> std::string { return mTarget; }
};

class TransportationLoadAttributes {
private:
  std::string mIdx;
  std::string mTargetIdx;
  datetime mReachBy;

public:
  TransportationLoadAttributes();

  TransportationLoadAttributes(std::string, std::string, datetime);

  [[nodiscard]] auto idx() const -> std::string;

  [[nodiscard]] auto target() const -> std::string;

  [[nodiscard]] auto reach_by() const -> datetime;
};

#endif
