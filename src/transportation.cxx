#include "transportation.hxx"

template<>
CLOCK
TransportEdge::weight<PathTraversalMode::FORWARD>(CLOCK start)
{
  return start;
}

template<>
CLOCK
TransportEdge::weight<PathTraversalMode::REVERSE>(CLOCK start)
{
  return start;
}
