#ifndef MOIRAI_PROCESSOR_HXX
#define MOIRAI_PROCESSOR_HXX

#include "solver.hxx"
#include <nlohmann/json.hpp>

nlohmann::json
parse_path(const Segment*);

#endif
