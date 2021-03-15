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

struct NodeOrEdgeProperties
{
private:
  std::unordered_map<Process, MINUTES> m_latencies;

public:
  template<Process P>
  MINUTES latency() const
  {
    return m_latencies.at(P);
  }

  template<Process P>
  void latency(const MINUTES& minutes) const
  {
    m_latencies[P] = minutes;
  }
};

struct Node : public NodeOrEdgeProperties
{
private:
  std::vector<std::string> labels;

public:
  Node() = default;

  Node(const std::string&);

  Node(const std::vector<std::string>&);

  std::vector<std::string> label() const;
};

struct Edge : public NodeOrEdgeProperties
{
private:
  std::vector<std::string> m_labels;
  LEVY m_cost;
  bool restricted;

public:
  Edge();

  Edge(const std::vector<std::string>&);
};

struct ContinuousEdge : public Edge
{
public:
  ContinuousEdge(const std::vector<std::string>&);
};

struct DiscreteEdge : public Edge
{
private:
  MINUTES m_departure;
  MINUTES m_duration;

public:
  DiscreteEdge(const std::vector<std::string>&, MINUTES, MINUTES, bool);
};

template<Direction D>
void weight(std::shared_ptr<Edge>,
            std::shared_ptr<Node>,
            std::shared_ptr<Node>);

template<>
void
weight<Direction::F>(std::shared_ptr<Edge> edge,
                     std::shared_ptr<Node> source,
                     std::shared_ptr<Node> target)
{
}

template<>
void
weight<Direction::R>(std::shared_ptr<Edge> edge,
                     std::shared_ptr<Node> target,
                     std::shared_ptr<Node> source)
{}
}
#endif
