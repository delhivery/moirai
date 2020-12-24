#include "date_utils.hxx"
#include "transportation.hxx"
#include <bits/stdint-intn.h>
#include <cstdint>
#include <date/date.h>
#include <iostream>

std::uint16_t
datemod(DURATION lhs, DURATION rhs)
{
  std::int16_t count_lhs = lhs.count();
  std::int16_t count_rhs = rhs.count();

  return ((count_lhs % count_rhs) + count_rhs) % count_rhs;
}

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::FORWARD>(CLOCK start,
                                                                COST cost) const
{
  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(cost.first - minutes_start,
                              std::chrono::days{ 1 }) };
  return start + wait_time + cost.second;
}

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::REVERSE>(CLOCK start,
                                                                COST cost) const
{

  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(minutes_start - cost.first,
                              std::chrono::days{ 1 }) };
  return start - wait_time - cost.second;
}
