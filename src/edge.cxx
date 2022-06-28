#include "edge.hxx"
#include <bit>

CostAttributes::CostAttributes() : mTransient(true) {}

CostAttributes::CostAttributes(const minutes &loading,
                               const time_of_day &departure,
                               const minutes &duration,
                               const minutes &unloading,
                               const std::vector<uint8_t> &departureDays)
    : mLoading(loading), mDeparture(departure), mUnloading(unloading),
      mDuration(duration), mTransient(false) {
  static_assert(sizeof(mDepartureDays) == 1);
  static_assert(sizeof(mArrivalDays) == 1);

  mArrival = mDeparture + mDuration;

  for (const auto &departureDay : departureDays) {
    mDepartureDays |= 1 << (CHAR_BIT - departureDay - 2);
  }
  mArrivalDays = mDepartureDays;

  uint8_t arrivalOffset = 0;
  auto cDuration = mDuration;

  while (cDuration > days(1)) {
    arrivalOffset++;
    arrivalOffset %= DAYS_IN_WEEK;
    cDuration -= days(1);
  }

  if (mArrival < mDeparture) {
    arrivalOffset = (arrivalOffset + 1) % DAYS_IN_WEEK;
  }

  mArrivalDays = mDepartureDays << (CHAR_BIT - arrivalOffset);
  mArrivalDays >>= 1;
  mArrivalDays |= mDepartureDays >> arrivalOffset;
}

auto CostAttributes::transient() const -> bool { return mTransient; }

template <>
auto CostAttributes::next_working_day<TraversalMode::FORWARD>(
    const weekday &startDay) const -> int8_t {
  uint8_t toShift = startDay.c_encoding();
  uint8_t lShift = mDepartureDays << (toShift + 1);
  lShift >>= 1;
  uint8_t rShift = mDepartureDays >> (DAYS_IN_WEEK - toShift);
  uint8_t result = lShift | rShift;
  return std::countl_zero(result) - 1;
}

template <>
auto CostAttributes::next_working_day<TraversalMode::REVERSE>(
    const weekday &startDay) const -> int8_t {
  uint8_t toShift = startDay.c_encoding();
  uint8_t rShift = mDepartureDays >> (CHAR_BIT - toShift - 2);
  uint8_t lShift = mDepartureDays << (toShift + 2);
  lShift >>= 1;
  uint8_t result = lShift | rShift;
  return std::countr_zero(result);
};

auto CostAttributes::loading() const -> minutes { return mLoading; }

auto CostAttributes::departure() const -> time_of_day { return mDeparture; }

auto CostAttributes::arrival() const -> time_of_day { return mArrival; }

auto CostAttributes::unloading() const -> minutes { return mUnloading; }

auto CostAttributes::duration() const -> minutes { return mDuration; }

template <>
auto CostAttributes::weight<TraversalMode::FORWARD>() const -> WeightFunction {
  return [this](const datetime &arrival) -> datetime {
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
    daysIdle = days(
        next_working_day<TraversalMode::FORWARD>(arrivalWeekday + baseOffset));
    timeIdle = baseOffset + daysIdle + mDeparture.to_duration() -
               arrivalTime.to_duration();
    return arrival + mLoading + timeIdle + mDuration + mUnloading;
  };
}

template <>
auto CostAttributes::weight<TraversalMode::REVERSE>() const -> WeightFunction {
  return [this](const datetime &arrival) -> datetime {
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
    daysIdle = days(
        next_working_day<TraversalMode::REVERSE>(arrivalWeekday - baseOffset));
    timeIdle = baseOffset + daysIdle + arrivalTime.to_duration() -
               mArrival.to_duration();
    return arrival - mUnloading - timeIdle - mDuration - mLoading;
  };
}