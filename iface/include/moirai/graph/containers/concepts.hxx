#ifndef GRAPH_CONTAINERS_CONCEPTS
#define GRAPH_CONTAINERS_CONCEPTS

#include <concepts>
#include <functional>

template <typename StoredEdgeT>
concept StoredEdgeConcept = std::equality_comparable<StoredEdgeT> and
    std::strict_weak_order<std::less<StoredEdgeT>, StoredEdgeT, StoredEdgeT> and
    requires() {
  typename StoredEdgeT::property_type;
};
#endif
