#ifndef MOIRAI_CONContainerTAINERS_CONContainerTAINERS
#define MOIRAI_CONContainerTAINERS_CONContainerTAINERS

//
// This file implements the C++ named requirement for Container
// as specified by https://en.cppreference.com/w/cpp/named_req/Container
//

#include <concepts>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace ambasta {

template <class ContainerT>
concept Container = requires(ContainerT container,
                             const ContainerT const_container) {
  // value_type is type T
  typename ContainerT::value_type;
  typename ContainerT::reference;
  typename ContainerT::const_reference;
  typename ContainerT::iterator;
  typename ContainerT::const_iterator;
  typename ContainerT::difference_type;
  typename ContainerT::size_type;

  // To handle "something" being cast to std::string instead of char[]
  // when ContainerT is a container of std::string
  requires std::same_as<typename ContainerT::value_type,
                        std::decay<typename ContainerT::value_type>>;

  // reference should have type T&
  requires std::same_as<typename ContainerT::reference,
                        typename ContainerT::value_type &>;

  // const_reference should have type const T&
  requires std::same_as<typename ContainerT::const_reference,
                        const typename ContainerT::value_type &>;

  // std::swap(a, b)
  requires not std::swappable<typename ContainerT::value_type> or
      std::swappable<ContainerT>;

  // CopyConstructible
  requires not std::copy_constructible<typename ContainerT::value_type> or
      std::copy_constructible<ContainerT>;

  // Destructible<T>
  requires std::destructible<typename ContainerT::value_type>;

  // EqualityComparable
  requires not std::equality_comparable<typename ContainerT::value_type> or
      std::equality_comparable<ContainerT>;

  // Copyable
  requires not std::copyable<typename ContainerT::value_type> or
      std::copyable<ContainerT>;

  // Copyable and DefaultConstructible
  requires not std::semiregular<typename ContainerT::value_type> or
      std::semiregular<ContainerT>;

  // Copyable, DefaultConstructible, EqualityComparable
  requires not std::regular<typename ContainerT::value_type> or
      std::regular<ContainerT>;

  // iterator = LegacyForwardIterator
  requires std::forward_iterator<typename ContainerT::iterator>;
  // iterator value should point to T
  requires std::same_as < std::iter_value_t<typename ContainerT::iterator>,
  typename ContainerT::value_type > ;
  // *iterator should point to T& or const T&
  requires std::same_as < std::iter_reference_t<typename ContainerT::iterator>,
  typename ContainerT::reference > or
      std::same_as<std::iter_reference_t<typename ContainerT::iterator>,
                   typename ContainerT::const_reference>;
  // iterator should be convertible to const_iterator
  requires std::convertible_to<typename ContainerT::iterator,
                               typename ContainerT::const_iterator>;
  // const_iterator = LegacyForwardIterator
  requires std::forward_iterator<typename ContainerT::const_iterator>;
  // const iterator should point to T
  requires std::same_as <
      std::iter_value_t<typename ContainerT::const_iterator>,
  typename ContainerT::value_type > ;
  // *const_iterator should point to const T&
  requires std::same_as <
      std::iter_reference_t<typename ContainerT::const_iterator>,
  typename ContainerT::const_reference > ;

  // difference_type should have type signed integer
  requires std::signed_integral<typename ContainerT::difference_type>;
  // size_type should be unsigned integer
  requires std::unsigned_integral<typename ContainerT::size_type>;
  // size_type should be large enough to represent all positive values of
  // difference_type
  requires std::in_range<typename ContainerT::size_type>(
      std::numeric_limits<typename ContainerT::difference_type>::max());
  // difference_type must be same as iterator_traits::difference_type for
  // iterator
  requires std::same_as<typename ContainerT::difference_type,
                        typename std::iterator_traits<
                            typename ContainerT::iterator>::difference_type>;
  // difference_type must be same as iterator_traits::difference_type for
  // const_iterator
  requires std::same_as<
      typename ContainerT::difference_type,
      typename std::iterator_traits<
          typename ContainerT::const_iterator>::difference_type>;

  // a.begin() should have type iterator
  { container.begin() } -> std::same_as<typename ContainerT::iterator>;

  // a.end() should have type iterator
  { container.end() } -> std::same_as<typename ContainerT::iterator>;

  // b.begin() should have type const_iterator
  {
    const_container.begin()
    } -> std::same_as<typename ContainerT::const_iterator>;

  // b.end() should have type const_iterator
  {
    const_container.end()
    } -> std::same_as<typename ContainerT::const_iterator>;

  // a.cbegin() should have type const_iterator
  { container.cbegin() } -> std::same_as<typename ContainerT::const_iterator>;

  // a.cend() should have type const_iterator
  { container.cend() } -> std::same_as<typename ContainerT::const_iterator>;

  // a.size() should have type size_type
  { container.size() } -> std::same_as<typename ContainerT::size_type>;

  // b.size() should have type size_type
  { const_container.size() } -> std::same_as<typename ContainerT::size_type>;

  // a.max_size() should have type size_type
  { container.max_size() } -> std::same_as<typename ContainerT::size_type>;

  // a.empty should be convertible to bool
  { container.empty() } -> std::convertible_to<bool>;
};

template <typename ContainerT>
concept SequenceContainer = Container<ContainerT> and
    requires(ContainerT &container, const ContainerT &const_container) {
  typename ContainerT::allocator_type;
  // ContainerT::value_type is copyinsertable if MoveInsertable(ContainerT::value_type)
  { container.front() } -> std::same_as<typename ContainerT::reference>;
};

template <typename ContainerT>
concept sequence_container = Container<ContainerT> and
    requires(ContainerT &container, ContainerT const &const_container) {
  { container.front() } -> std::same_as<typename ContainerT::reference>;
  {
    const_container.front()
    } -> std::same_as<typename ContainerT::const_reference>;
};

template <typename ContainerT>
concept reversible_container = Container<ContainerT> and
    requires(ContainerT &container, ContainerT const &const_container) {
  requires std::bidirectional_iterator<typename ContainerT::iterator>;
  requires std::bidirectional_iterator<typename ContainerT::const_iterator>;
};

template <typename ContainerT>
concept double_ended_container = sequence_container<ContainerT> and
    reversible_container<ContainerT> and
    requires(ContainerT &container, ContainerT const &const_container) {
  { container.back() } -> std::same_as<typename ContainerT::reference>;
  {
    const_container.back()
    } -> std::same_as<typename ContainerT::const_reference>;
};

template <typename ContainerT, typename ValueT>
concept container_of = Container<ContainerT> and
    std::same_as<ValueT, typename ContainerT::value_type>;

template <typename ContainerT>
concept random_access_container = double_ended_container<ContainerT> and
    requires(ContainerT &container, ContainerT const &const_container,
             typename ContainerT::size_type const size) {
  requires std::random_access_iterator<typename ContainerT::iterator>;
  requires std::random_access_iterator<typename ContainerT::const_iterator>;

  { container[size] } -> std::same_as<typename ContainerT::reference>;
  {
    const_container[size]
    } -> std::same_as<typename ContainerT::const_reference>;

  { container.at[size] } -> std::same_as<typename ContainerT::reference>;
  {
    const_container.at[size]
    } -> std::same_as<typename ContainerT::const_reference>;
};

template <typename ContainerT, typename ValueT>
concept random_access_container_of =
    container_of<ContainerT, ValueT> and random_access_container<ContainerT>;
} // namespace ambasta
#endif
