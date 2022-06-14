#include "edge_cost_attributes.hxx"
#include <bit>
#include <climits>

TemporalEdgeCostAttributes::TemporalEdgeCostAttributes()
  : mTransient(true)
{
}

TemporalEdgeCostAttributes::TemporalEdgeCostAttributes(
  const minutes& loading,
  const time_of_day& departure,
  const minutes& duration,
  const minutes& unloading,
  const std::vector<uint8_t>& departureDays)
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
