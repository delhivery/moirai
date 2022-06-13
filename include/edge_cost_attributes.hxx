#ifndef moirai_edge_costs
#define moirai_edge_costs

#include "date_utils.hxx"
#include <functional>
#include <vector>

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

  TemporalEdgeCostAttributes(const minutes&,
                             const time_of_day&,
                             const minutes&,
                             const minutes&,
                             const std::vector<uint8_t>&);

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
