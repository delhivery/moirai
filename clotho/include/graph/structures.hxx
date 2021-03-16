#ifndef GRAPH_STRUCTURES
#define GRAPH_STRUCTURES

#include "typedefs.hxx"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ambasta {
enum Process
{
  // inb
  I = 0,
  // out
  O = 1,
  // transfer
  X = 2,
};

enum Direction
{
  // Forward
  F = 0,
  // Reverse
  R = 1,
};

struct Construct
{
protected:
  std::string m_label;
  std::unordered_map<Process, MINUTES> m_latencies;

public:
  Construct(const std::string&);

  template<Process P>
  MINUTES latency() const
  {
    return m_latencies.at(P);
  }

  template<Process P>
  void latency(const MINUTES& minutes)
  {
    m_latencies[P] = minutes;
  }

  std::string label() const;
};

struct Node : public Construct
{
public:
  Node(const std::string&);
};

struct Route : public Construct
{
protected:
  LEVY m_cost;
  bool m_restricted;
  bool m_discrete;
  MINUTES m_departure;
  MINUTES m_duration;

public:
  Route(const std::string&, const MINUTES, const MINUTES, const bool = false);

  Route(const std::string&);

  MINUTES departure() const;

  template<Direction D>
  MINUTES departure(std::shared_ptr<Node>) const;

  /*template<>
  MINUTES departure<Direction::F>(std::shared_ptr<Node>) const;

  template<>
  MINUTES departure<Direction::R>(std::shared_ptr<Node>) const;*/

  MINUTES duration() const;

  MINUTES duration(std::shared_ptr<Node>, std::shared_ptr<Node>) const;
};
}
#endif
