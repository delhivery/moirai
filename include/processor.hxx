#ifndef MOIRAI_PROCESSOR_HXX
#define MOIRAI_PROCESSOR_HXX

#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20
#endif

#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 1
#endif

#include "solver.hxx"
#include <nlohmann/json.hpp>

auto
parse_path(const std::vector<Segment>&) -> nlohmann::json;

#endif
