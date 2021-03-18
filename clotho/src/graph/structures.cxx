#include "graph/structures.hxx"
#include "typedefs.hxx"

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
             const TIME_OF_DAY& departure,
             const MINUTES& duration,
             const bool unrestricted)
  : Construct(label)
  , m_departure(departure)
  , m_duration(duration)
  , m_unrestricted(unrestricted)
  , m_discrete(true)
{}

Route::Route(const std::string& label)
  : Construct(label)
  , m_departure(0)
  , m_duration(0)
  , m_unrestricted(true)
  , m_discrete(false)
{}

TIME_OF_DAY
Route::departure() const
{
  return m_departure;
}

template<>
TIME_OF_DAY
Route::departure<Algorithm::SHORTEST>(std::shared_ptr<Node> source) const
{

  return static_cast<TIME_OF_DAY>(static_cast<MINUTES>(m_departure) -
                                  source->latency<Process::O>() -
                                  latency<Process::I>());
}

template<>
TIME_OF_DAY
Route::departure<Algorithm::INVERSE_SHORTEST>(
  std::shared_ptr<Node> target) const
{
  return static_cast<TIME_OF_DAY>(static_cast<MINUTES>(m_departure) +
                                  latency<Process::O>() +
                                  target->latency<Process::I>());
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
