#ifndef moirai_edge_costs
#define moirai_edge_costs

#include "concepts.hxx"
#include "date.hxx"
#include <climits>
#include <concepts>
#include <functional>
#include <ranges>

enum TraversalMode : std::uint8_t {
  FORWARD = 0,
  REVERSE = 1,
};

using WeightFunction = std::function<datetime(const datetime &)>;

class CostAttributes {
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
  static constexpr int DAYS_IN_WEEK = 7;

  CostAttributes();

  CostAttributes(const minutes &,             // loading
                 const time_of_day &,         // departure
                 const minutes &,             // duration
                 const minutes &,             // unloading
                 const std::vector<uint8_t> & // working_days
  );

  template <TraversalMode>
  [[nodiscard]] auto next_working_day(const weekday &) const -> int8_t;

  [[nodiscard]] auto transient() const -> bool;

  [[nodiscard]] auto loading() const -> minutes;

  [[nodiscard]] auto departure() const -> time_of_day;

  [[nodiscard]] auto arrival() const -> time_of_day;

  [[nodiscard]] auto unloading() const -> minutes;

  [[nodiscard]] auto duration() const -> minutes;

  template <TraversalMode> [[nodiscard]] auto weight() const -> WeightFunction;
};

#endif
