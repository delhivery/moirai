#ifndef MOIRAI_PROCESSOR_HXX
#define MOIRAI_PROCESSOR_HXX

#include "solver.hxx"
#include <nlohmann/json.hpp>

auto
parse_path(const std::vector<Segment>&) -> nlohmann::json;

#endif
