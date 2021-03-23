#include <clotho/graph/typedefs.hxx>

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
             const bool unrestricted,
             const LEVY levy)
  : Construct(label)
  , m_departure(departure)
  , m_duration(duration)
  , m_unrestricted(unrestricted)
  , m_discrete(true)
  , m_levy(levy)
{}

Route::Route(const std::string& label)
  : Construct(label)
  , m_departure(0)
  , m_duration(0)
  , m_unrestricted(true)
  , m_discrete(false)
  , m_levy(1)
{}

TIME_OF_DAY
Route::departure() const
{
  return m_departure;
}

template<>
TIME_OF_DAY
Route::departure<Algorithm::SHORTEST>(const Node* source) const
{

  return static_cast<TIME_OF_DAY>(static_cast<MINUTES>(m_departure) -
                                  source->latency<Process::O>() -
                                  latency<Process::I>());
}

template<>
TIME_OF_DAY
Route::departure<Algorithm::INVERSE_SHORTEST>(const Node* target) const
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
Route::duration(const Node* source, const Node* target) const
{
  return source->latency<Process::O>() + latency<Process::I>() + m_duration +
         latency<Process::O>() + target->latency<Process::I>();
}

LEVY
Route::levy() const
{
  return m_levy;
}
}
