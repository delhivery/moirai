#ifndef GRAPH_TYPEDEFS_HXX
#define GRAPH_TYPEDEFS_HXX

#include <clotho/typedefs.hxx>
#include <memory>
#include <string>
#include <unordered_map>

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
  bool m_unrestricted;
  bool m_discrete;
  TIME_OF_DAY m_departure;
  MINUTES m_duration;

public:
  Route(const std::string&,
        const TIME_OF_DAY&,
        const MINUTES&,
        const bool = false);

  Route(const std::string&);

  TIME_OF_DAY departure() const;

  template<Algorithm A>
  TIME_OF_DAY departure(std::shared_ptr<Node>) const;

  bool unrestricted() const { return m_unrestricted; }

  void unrestricted(bool restricted) { m_unrestricted = restricted; }

  MINUTES duration() const;

  MINUTES duration(std::shared_ptr<Node>, std::shared_ptr<Node>) const;
};
}
#endif
