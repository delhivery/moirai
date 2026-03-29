#pragma once

#include "solver.hxx"
#include <nlohmann/json_fwd.hpp>

template <PathTraversalMode P>
auto parse_path(const std::shared_ptr<Segment> &) -> nlohmann::json;
