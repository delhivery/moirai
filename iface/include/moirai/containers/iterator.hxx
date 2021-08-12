#ifndef MOIRAI_CONTAINERS_ITERATOR
#define MOIRAI_CONTAINERS_ITERATOR

#include <cstddef>
namespace ambasta {
template<typename T>
class Iterator
{
public:
  using difference_type = std::ptrdiff_t;
  using value_type = T;

  Iterator();

  bool operator==(const Sentinel&) const;

  T& operator*() const;

  Iterator& operator++()
  { /* do stuff */
    return *this;
  }

  void operator++(int) { ++*this; }

private:
  // Implementation
};
}

#endif
