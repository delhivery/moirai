#ifndef moirai_edge_costs
#define moirai_edge_costs

#include "date_utils.hxx"
#include <vector>

enum PathTraversalMode : std::uint8_t
{
  FORWARD = 0,
  REVERSE = 1,
};

class TemporalEdgeCostAttributes
{
private:
  minutes mLoading;
  time_of_day mDeparture;
  time_of_day mArrival;
  minutes mUnloading;
  minutes mDuration;
  uint8_t mDepartureDays = 0;
  uint8_t mArrivalDays = 0;
  bool mTransient = false;

public:
  TemporalEdgeCostAttributes() = default;

  TemporalEdgeCostAttributes(const minutes&,
                             const time_of_day&,
                             const minutes&,
                             const minutes&,
                             const std::vector<uint8_t>&);

  template<PathTraversalMode>
  int8_t next_working_day(const weekday&) const;

  auto transient() const -> bool;

  minutes loading() const;

  time_of_day departure() const;

  time_of_day arrival() const;

  minutes unloading() const;

  minutes duration() const;
};

struct EdgeTraversalCost
{
  template<PathTraversalMode>
  datetime operator()(const datetime&, const TemporalEdgeCostAttributes&) const;
};

datetime get_departure(datetime, time_of_day);

#endif
