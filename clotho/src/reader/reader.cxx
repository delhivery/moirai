#include "reader/reader.hxx"
#include <bitset>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sysexits.h>
#include <thread>

using namespace std::literals;

namespace ambasta {
struct Center
{
  std::string code;
  std::string name;
  // processing inbound time
  // processing outbound time
  int16_t pit, pot;
};

enum TransportModeFlags
{
  NONE = 0 << 0,
  SURFACE = 1 << 0,
  AIR = 1 << 1,
};

inline constexpr TransportModeFlags
operator|(TransportModeFlags lhs, TransportModeFlags rhs)
{
  return static_cast<TransportModeFlags>(static_cast<int>(lhs) |
                                         static_cast<int>(rhs));
}
// Use tag dispatching to provide additional information on behavior
// Use strong types to provide additional information on data
//
// Note: When tag dispatching idiom is used
// the function that recieves a dummy tag parameter
// should be a simple inline wrapper around other function
// that implements the required functionality
struct Edge
{
  Center source;
  Center target;
  std::string code;
  std::string mode;
  std::string type;
  // processing loading time
  // processing unloading time
  int16_t plt, put;
  int16_t departure;
  int16_t duration;
  int16_t cost;
  std::bitset<2> modal;

  std::string to_string() const { return modal.to_string(); }
};

Reader::Reader(std::shared_ptr<CLI::App> app)
  : utils::Component(app)
{}

int
Reader::main(std::stop_token stop_token)
{
  while (!stop_token.stop_requested()) {
    std::this_thread::sleep_for(0.2s);
    auto edges = fetch_edges();

    for (auto& edge : edges) {
      nlohmann::json j_edge = edge;
    }
  }
  return EX_OK;
}

};
