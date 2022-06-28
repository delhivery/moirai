#include "transport_center.hxx"
#include <climits>

auto ActionHash::operator()(Action action) const -> size_t {
  uint16_t hashed = static_cast<uint16_t>(action.first);
  hashed <<= CHAR_BIT;
  hashed |= static_cast<uint16_t>(action.second);
  return hashed;
}

Center::Center(std::string code, std::string name, time_of_day cutoff,
               minutes duration)
    : mCode(std::move(code)), mName(std::move(name)), mCutoff(cutoff) {}

void Center::latency(Process process, Movement movement, minutes latency) {
  auto key = std::make_pair(process, movement);
  mLatencies[key] = latency;
}

auto Center::code() const -> std::string { return mCode; }

auto Center::latency(Process process, Movement movement) const -> minutes {
  auto key = std::make_pair(process, movement);

  if (mLatencies.contains(key))
    return mLatencies.at(key);
  return minutes(0);
}

auto Center::cutoff() const -> time_of_day { return mCutoff; }
