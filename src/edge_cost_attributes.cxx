#include "edge_cost_attributes.hxx"
#include <bit>
#include <climits>

TemporalEdgeCostAttributes::TemporalEdgeCostAttributes(
  const minutes& loading,
  const time_of_day& departure,
  const minutes& duration,
  const minutes& unloading,
  const std::vector<uint8_t>& departureDays)
  : mLoading(loading)
  , mDeparture(departure)
  , mDuration(duration)
  , mUnloading(unloading)
  , mTransient(false)
{
  static_assert(sizeof(mDepartureDays) == 1);
  static_assert(sizeof(mArrivalDays) == 1);

  mArrival = mDeparture + mDuration;

  for (const auto& departureDay : departureDays)
    mDepartureDays |= 1 << CHAR_BIT - departureDay - 2;
  mArrivalDays = mDepartureDays;

  uint8_t arrivalOffset = 0;
  auto cDuration = mDuration;

  while (cDuration > days(1)) {
    arrivalOffset++;
    arrivalOffset %= 7;
    cDuration -= days(1);
  }

  if (mArrival < mDeparture)
    arrivalOffset = (arrivalOffset + 1) % 7;

  mArrivalDays = mDepartureDays << CHAR_BIT - arrivalOffset;
  mArrivalDays >>= 1;
  mArrivalDays |= mDepartureDays >> arrivalOffset;
}

auto
TemporalEdgeCostAttributes::transient() const -> bool
{
  return mTransient;
}

template<>
int8_t
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::FORWARD>(
  const weekday& startDay) const
{
  uint8_t toShift = startDay.c_encoding();
  uint8_t lShift = mDepartureDays << toShift + 1;
  lShift >>= 1;
  uint8_t rShift = mDepartureDays >> 7 - toShift;
  uint8_t result = lShift | rShift;
  return std::countl_zero(result) - 1;
}

template<>
int8_t
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::REVERSE>(
  const weekday& startDay) const
{
  uint8_t toShift = startDay.c_encoding();
  uint8_t rShift = mDepartureDays >> CHAR_BIT - toShift - 2;
  uint8_t lShift = mDepartureDays << toShift + 2;
  lShift >>= 1;
  uint8_t result = lShift | rShift;
  return std::countr_zero(result);
};

minutes
TemporalEdgeCostAttributes::loading() const
{
  return mLoading;
}

time_of_day
TemporalEdgeCostAttributes::departure() const
{
  return mDeparture;
}

time_of_day
TemporalEdgeCostAttributes::arrival() const
{
  return mArrival;
}

minutes
TemporalEdgeCostAttributes::unloading() const
{
  return mUnloading;
}

minutes
TemporalEdgeCostAttributes::duration() const
{
  return mDuration;
}

template<>
datetime
EdgeTraversalCost::operator()<PathTraversalMode::FORWARD>(
  const datetime& arrival,
  const TemporalEdgeCostAttributes& costAttrs) const
{
  if (costAttrs.transient())
    return arrival;
  auto arrivalDay = std::chrono::floor<days>(arrival);
  auto arrivalWeekday = weekday(std::chrono::sys_days(arrivalDay));
  time_of_day arrivalTime = arrival - arrivalDay;
  arrivalTime += costAttrs.loading();

  minutes timeIdle(0);
  days baseOffset(0), daysIdle(0);

  // If departure earlier than start time, then we need to at least wait until
  // next day to depart. So we need to find the next available working day
  // starting stary_day +1
  if (arrivalTime > costAttrs.departure())
    baseOffset = days(1);
  daysIdle = days(costAttrs.next_working_day<PathTraversalMode::FORWARD>(
    arrivalWeekday + baseOffset));

  timeIdle = baseOffset + daysIdle + costAttrs.departure().to_duration() -
             arrivalTime.to_duration();
  return arrival + costAttrs.loading() + timeIdle + costAttrs.duration() +
         costAttrs.unloading();
}

template<>
datetime
EdgeTraversalCost::operator()<PathTraversalMode::REVERSE>(
  const datetime& arrival,
  const TemporalEdgeCostAttributes& costAttrs) const
{
  if (costAttrs.transient())
    return arrival;
  auto arrivalDay = std::chrono::floor<days>(arrival);
  auto arrivalWeekday = weekday(std::chrono::sys_days(arrivalDay));
  time_of_day arrivalTime = arrival - arrivalDay;
  arrivalTime -= costAttrs.unloading();

  minutes timeIdle(0);
  days baseOffset(0), daysIdle(0);

  if (arrivalTime < costAttrs.arrival())
    baseOffset = days(1);
  daysIdle = days(costAttrs.next_working_day<PathTraversalMode::REVERSE>(
    arrivalWeekday - baseOffset));
  timeIdle = baseOffset + daysIdle + arrivalTime.to_duration() -
             costAttrs.arrival().to_duration();
  return arrival - costAttrs.unloading() - timeIdle - costAttrs.duration() -
         costAttrs.loading();
}
