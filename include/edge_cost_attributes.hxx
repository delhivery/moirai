#ifndef moirai_edge_costs
#define moirai_edge_costs

#include "concepts.hxx"
#include "date_utils.hxx"
#include <climits>
#include <functional>

enum PathTraversalMode : std::uint8_t
{
  FORWARD = 0,
  REVERSE = 1,
};

using WeightFunction = std::function<datetime(const datetime&)>;

class TemporalEdgeCostAttributes
{
protected:
  minutes mLoading;
  time_of_day mDeparture;
  time_of_day mArrival;
  minutes mUnloading;
  minutes mDuration;
  uint8_t mDepartureDays = 0;
  uint8_t mArrivalDays;
  bool mTransient;

public:
  static constexpr int daysInWeek = 7;

  TemporalEdgeCostAttributes();

  template<range_of<uint8_t> range_t>
  TemporalEdgeCostAttributes(const minutes& loading,
                             const time_of_day& departure,
                             const minutes& duration,
                             const minutes& unloading,
                             const range_t& departureDays)
    : mLoading(loading)
    , mDeparture(departure)
    , mUnloading(unloading)
    , mDuration(duration)
    , mTransient(false)
  {
    static_assert(sizeof(mDepartureDays) == 1);
    static_assert(sizeof(mArrivalDays) == 1);

    mArrival = mDeparture + mDuration;

    for (const auto& departureDay : departureDays) {
      mDepartureDays |= 1 << (CHAR_BIT - departureDay - 2);
    }
    mArrivalDays = mDepartureDays;

    uint8_t arrivalOffset = 0;
    auto cDuration = mDuration;

    while (cDuration > days(1)) {
      arrivalOffset++;
      arrivalOffset %= daysInWeek;
      cDuration -= days(1);
    }

    if (mArrival < mDeparture) {
      arrivalOffset = (arrivalOffset + 1) % daysInWeek;
    }

    mArrivalDays = mDepartureDays << (CHAR_BIT - arrivalOffset);
    mArrivalDays >>= 1;
    mArrivalDays |= mDepartureDays >> arrivalOffset;
  }

  template<PathTraversalMode>
  [[nodiscard]] auto next_working_day(const weekday&) const -> int8_t;

  [[nodiscard]] auto transient() const -> bool;

  [[nodiscard]] auto loading() const -> minutes;

  [[nodiscard]] auto departure() const -> time_of_day;

  [[nodiscard]] auto arrival() const -> time_of_day;

  [[nodiscard]] auto unloading() const -> minutes;

  [[nodiscard]] auto duration() const -> minutes;

  template<PathTraversalMode>
  [[nodiscard]] auto weight() const -> WeightFunction;
};

#endif
