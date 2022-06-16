#include "edge_cost_attributes.hxx"
#include <bit>     // For countl_zero/countr_zero

TemporalEdgeCostAttributes::TemporalEdgeCostAttributes()
  : mTransient(true)
{
}

auto
TemporalEdgeCostAttributes::transient() const -> bool
{
  return mTransient;
}

template<>
auto
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::FORWARD>(
  const weekday& startDay) const -> int8_t
{
  uint8_t toShift = startDay.c_encoding();
  uint8_t lShift = mDepartureDays << (toShift + 1);
  lShift >>= 1;
  uint8_t rShift = mDepartureDays >> (daysInWeek - toShift);
  uint8_t result = lShift | rShift;
  return std::countl_zero(result) - 1;
}

template<>
auto
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::REVERSE>(
  const weekday& startDay) const -> int8_t
{
  uint8_t toShift = startDay.c_encoding();
  uint8_t rShift = mDepartureDays >> (CHAR_BIT - toShift - 2);
  uint8_t lShift = mDepartureDays << (toShift + 2);
  lShift >>= 1;
  uint8_t result = lShift | rShift;
  return std::countr_zero(result);
};

auto
TemporalEdgeCostAttributes::loading() const -> minutes
{
  return mLoading;
}

auto
TemporalEdgeCostAttributes::departure() const -> time_of_day
{
  return mDeparture;
}

auto
TemporalEdgeCostAttributes::arrival() const -> time_of_day
{
  return mArrival;
}

auto
TemporalEdgeCostAttributes::unloading() const -> minutes
{
  return mUnloading;
}

auto
TemporalEdgeCostAttributes::duration() const -> minutes
{
  return mDuration;
}

template<>
auto
TemporalEdgeCostAttributes::weight<PathTraversalMode::FORWARD>() const
  -> WeightFunction
{
  return [this](const datetime& arrival) -> datetime {
    if (mTransient) {
      return arrival;
    }
    datetime arrivalWithLoading = arrival + mLoading;

    auto arrivalDay = std::chrono::floor<days>(arrivalWithLoading);
    auto arrivalWeekday = weekday(std::chrono::sys_days(arrivalDay));
    time_of_day arrivalTime = arrivalWithLoading - arrivalDay;

    minutes timeIdle(0);
    days baseOffset(0);
    days daysIdle(0);

    if (arrivalTime > mDeparture) {
      baseOffset = days(1);
    }
    daysIdle = days(next_working_day<PathTraversalMode::FORWARD>(
      arrivalWeekday + baseOffset));
    timeIdle = baseOffset + daysIdle + mDeparture.to_duration() -
               arrivalTime.to_duration();
    return arrival + mLoading + timeIdle + mDuration + mUnloading;
  };
}

template<>
auto
TemporalEdgeCostAttributes::weight<PathTraversalMode::REVERSE>() const
  -> WeightFunction
{
  return [this](const datetime& arrival) -> datetime {
    if (mTransient) {
      return arrival;
    }
    datetime arrivalWithUnloading = arrival - mUnloading;
    auto arrivalDay = std::chrono::floor<days>(arrivalWithUnloading);
    auto arrivalWeekday = weekday(std::chrono::sys_days(arrivalDay));
    time_of_day arrivalTime = arrivalWithUnloading - arrivalDay;

    minutes timeIdle(0);
    days baseOffset(0);
    days daysIdle(0);

    if (arrivalTime < mArrival) {
      baseOffset = days(1);
    }
    daysIdle = days(next_working_day<PathTraversalMode::REVERSE>(
      arrivalWeekday - baseOffset));
    timeIdle = baseOffset + daysIdle + arrivalTime.to_duration() -
               mArrival.to_duration();
    return arrival - mUnloading - timeIdle - mDuration - mLoading;
  };
}
