#include "clotho/typedefs.hxx"
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <clotho/graph/earliest.hxx>
#include <functional>

using namespace ambasta;

bool
ShortestPathSolver::compare(const COST& lhs, const COST& rhs) const
{
  return lhs.first <= rhs.first;
}

COST
ShortestPathSolver::combine(
  const COST& distance,
  const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost) const
{
  MINUTES wait_time{ std::get<0>(cost) - TIME_OF_DAY(distance.first) };
  return std::make_pair(distance.first + wait_time + std::get<1>(cost),
                        distance.second + std::get<2>(cost));
}

COST
ShortestPathSolver::zero(const TIMESTAMP start, const LEVY levy) const
{
  return COST{ (MINUTES)start, 0 };
}

COST
ShortestPathSolver::inf(const TIMESTAMP start, const LEVY levy) const
{
  return COST{ MINUTES::max(), std::numeric_limits<LEVY>::max() };
}

const std::tuple<TIME_OF_DAY, MINUTES, LEVY>
ShortestPathSolver::weight(const EdgeDescriptor& ed) const
{

  const Route* route = (*m_graph)[ed].get();
  const Node* source = (*m_graph)[boost::source(ed, *m_graph)].get();
  const Node* target = (*m_graph)[boost::target(ed, *m_graph)].get();

  return std::make_tuple(route->departure<Algorithm::SHORTEST>(source),
                         route->duration(source, target),
                         route->levy());
}
