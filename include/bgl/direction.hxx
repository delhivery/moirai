#ifndef MOIRAI_GRAPH_DIRECTION
#define MOIRAI_GRAPH_DIRECTION
#include <type_traits>
#include <vector>

enum class Direction
{
  Directed = 0,
  Undirected,
  Bidirectional
};

template<Direction direction>
class is_directed : std::true_type
{};

template<>
class is_directed<Direction::Undirected> : std::false_type
{};

template<Direction direction>
class is_bidirectional_t : std::false_type
{};

template<>
class is_bidirectional_t<Direction::Bidirectional> : std::true_type
{};

namespace detail {
template<template<typename...> class ContainerT>
class is_random_access : std::false_type
{};

template<>
class is_random_access<std::vector> : std::true_type
{};
}

#endif
