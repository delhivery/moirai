#ifndef TRANSPORT_CENTER
#define TRANSPORT_CENTER

#include "date.hxx"
#include "hash.hxx"
#include "transport.hxx"
#include <string>
#include <unordered_map>

using Action = std::pair<Process, Movement>;

struct ActionHash {
  auto operator()(Action action) const -> size_t;
};

class Center {
private:
  std::string mCode;
  std::string mName;
  time_of_day mCutoff;
  std::unordered_map<Action, minutes, ActionHash> mLatencies;

public:
  Center(std::string, // code
         std::string, // name
         time_of_day, // cutoff
         minutes      // duration
  );

  void latency(Process, Movement, minutes);

  [[nodiscard]] auto code() const -> std::string;

  [[nodiscard]] auto latency(Process, Movement) const -> minutes;

  [[nodiscard]] auto cutoff() const -> time_of_day; // { return mCutoff; }

  friend auto std::hash<Center>::operator()(const Center &) const -> size_t;
};

#endif
