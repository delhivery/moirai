#include "date_utils.hxx"
#include "transportation.hxx"

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::FORWARD>(CLOCK start,
                                                                COST cost) const
{
  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };

  DURATION wait_time{ (cost.first - minutes_start).count() %
                      std::chrono::days{ 1 }.count() };
  return start + wait_time + cost.second;
}

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::REVERSE>(CLOCK start,
                                                                COST cost) const
{

  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };

  DURATION wait_time{ (minutes_start - cost.first).count() %
                      std::chrono::days{ 1 }.count() };
  return start - wait_time - cost.second;
}
