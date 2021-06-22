#ifndef MOIRAI_PROCESSOR_HXX
#define MOIRAI_PROCESSOR_HXX

#include "solver.hxx"
#include <nlohmann/json.hpp>

template<PathTraversalMode P>
nlohmann::json
parse_path(const Segment*);

#endif
