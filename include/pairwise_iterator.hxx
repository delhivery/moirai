#ifndef MOIRAI_NWISE_ITERATOR
#define MOIRAI_NWISE_ITERATOR

#include <iterator>
#include <tuple>
#include <utility>

template<typename ForwardIterator>
class pairwise_iterator
{
private:
  ForwardIterator m_first;
  ForwardIterator m_next;

public:
  pairwise_iterator(ForwardIterator first, ForwardIterator last)
    : m_first(first)
    , m_next(first == last ? first : std::next(first))
  {}

  bool operator!=(const pairwise_iterator& other) const
  {
    return m_next != other.m_next;
  }

  pairwise_iterator& operator++()
  {
    ++m_first;
    ++m_next;
    return *this;
  }

  typedef typename std::iterator_traits<ForwardIterator>::reference Ref;
  typedef std::pair<Ref, Ref> Pair;

  Pair operator*() const { return Pair(*m_first, *m_next); }
};

template<typename ForwardIterator>
class pairwise_range
{
private:
  ForwardIterator m_first;
  ForwardIterator m_last;

public:
  pairwise_range(ForwardIterator first, ForwardIterator last)
    : m_first(first)
    , m_last(last)
  {}

  pairwise_iterator<ForwardIterator> begin() const
  {
    return pairwise_iterator<ForwardIterator>(m_first, m_last);
  }

  pairwise_iterator<ForwardIterator> end() const
  {
    return pairwise_iterator<ForwardIterator>(m_last, m_last);
  }
};

template<typename C>
auto
make_pairwise_range(C& c) -> pairwise_range<decltype(c.begin())>
{
  return pairwise_range<decltype(c.begin())>(c.begin(), c.end());
}
#endif
