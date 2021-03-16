#include "graph/structures.hxx"

namespace ambasta {

Construct::Construct(const std::string& label)
  : m_label(label)
{}

std::string
Construct::label() const
{
  return m_label;
}

Node::Node(const std::string& label)
  : Construct(label)
{}

Route::Route(const std::string& label,
             const MINUTES departure,
             const MINUTES duration,
             const bool restricted)
  : Construct(label)
  , m_departure(departure)
  , m_duration(duration)
  , m_restricted(restricted)
  , m_discrete(true)
{}

Route::Route(const std::string& label)
  : Construct(label)
  , m_departure(0)
  , m_duration(0)
  , m_restricted(false)
  , m_discrete(false)
{}

MINUTES
Route::departure() const
{
  return m_departure;
}

template<>
MINUTES
Route::departure<Direction::F>(std::shared_ptr<Node> source) const
{

  return m_departure - source->latency<Process::O>() - latency<Process::I>();
}

template<>
MINUTES
Route::departure<Direction::R>(std::shared_ptr<Node> target) const
{
  return m_departure + latency<Process::O>() + target->latency<Process::I>();
}

MINUTES
Route::duration() const
{
  return m_duration;
}

MINUTES
Route::duration(std::shared_ptr<Node> source,
                std::shared_ptr<Node> target) const
{
  return source->latency<Process::O>() + latency<Process::I>() + m_duration +
         latency<Process::O>() + target->latency<Process::I>();
}
}
