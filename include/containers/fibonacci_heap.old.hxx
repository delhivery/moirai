#include <algorithm>
#include <functional>
#include <iomanip>
#include <limits>
#include <vector>

template<typename ValueT, typename PriorityT>
class Entry
{
private:
  size_t m_degree;
  bool m_is_marked;
  Entry* m_next;
  Entry* m_prev;
  Entry* m_parent;
  Entry* m_child;
  ValueT m_value;
  PriorityT m_priority;

public:
  Entry()
    : m_degree(0)
    , m_is_marked(false)
    , m_next(nullptr)
    , m_prev(nullptr)
    , m_parent(nullptr)
    , m_child(nullptr)
  {}

  Entry(const ValueT& value, const PriorityT& priority)
    : Entry()
  {
    m_next = this;
    m_prev = this;
    m_value = value;
    m_priority = priority;
  }

  const ValueT& value() const { return m_value; }

  void value(const ValueT& value) { m_value = value; }

  const PriorityT& priority() const { return m_priority; }

  void priority(const PriorityT& priority) { m_priority = priority; }

  size_t degree() const { return m_degree; }

  void degree(size_t degree) { m_degree = degree; }

  bool is_marked() const { return m_is_marked; }

  void is_marked(bool is_marked) { m_is_marked = is_marked; }

  Entry* next() const { return m_next; }

  void next(Entry* entry) { m_next = entry; }

  Entry* prev() const { return m_prev; }

  void prev(Entry* entry) { m_prev = entry; }

  Entry* parent() const { return m_parent; }

  void parent(Entry* entry) { m_parent = entry; }

  Entry* child() const { return m_child; }

  void child(Entry* entry) { m_child = entry; }
};

template<typename ValueT, typename PriorityT>
class FibonacciHeap
{
  using EntryT = Entry<ValueT, PriorityT>;

private:
  EntryT* m_smallest;
  size_t m_size = 0;

  friend EntryT* merge_lists(EntryT*, EntryT*);

  void cut_node(EntryT* entry)
  {
    entry->is_marked(false);

    if (entry->parent() == nullptr)
      return;

    if (entry->next() != entry) {
      entry->next()->prev(entry->prev());
      entry->next()->next(entry->next());
    }

    if (entry->parent()->child() == entry) {
      if (entry->next() != entry)
        entry->parent()->child(entry->next());
      else
        entry->parent()->child(nullptr);
    }

    entry->parent()->degree(entry->parent()->degree() - 1);

    entry->prev(entry);
    entry->next(entry);
    m_smallest = merge_lists(m_smallest, entry);

    if (entry->parent()->is_marked())
      cut_node(entry->parent());
    else
      entry->parent()->is_marked(true);
    entry->parent(nullptr);
  }

  void decrease_key_unchecked(EntryT* entry, const PriorityT& priority)
  {
    entry->priority(priority);

    if (entry->parent() != nullptr and
        entry->priority() <= entry->parent()->priority())
      cut_node(entry);

    if (entry->priority() <= m_smallest->priority())
      m_smallest = entry;
  }

public:
  EntryT* push_back(const ValueT& value, const PriorityT& priority)
  {
    EntryT* entry = new EntryT(value, priority);
    m_smallest = merge_lists(m_smallest, entry);
    ++m_size;
    return entry;
  }

  EntryT* smallest() const { return m_smallest; }

  size_t size() const { return m_size; }

  bool empty() const { return m_size == 0 and m_smallest == nullptr; }

  void clear()
  {
    m_size = 0;
    m_smallest = nullptr;
  }

  friend FibonacciHeap* merge(const FibonacciHeap*, const FibonacciHeap*);

  EntryT* pop()
  {
    if (empty())
      return nullptr;

    --m_size;
    EntryT* smallest = m_smallest;

    if (m_smallest->next() == m_smallest)
      m_smallest = nullptr;
    else {
      m_smallest->prev()->next(m_smallest->next());
      m_smallest->next()->prev(m_smallest->prev());
      m_smallest = m_smallest->next();
    }

    if (smallest->child() != nullptr) {
      EntryT* current = smallest->child();
      do {
        current->parent(nullptr);
        current = current->next();
      } while (current != smallest->child());
    }

    m_smallest = merge_lists(m_smallest, smallest->child());

    if (m_smallest == nullptr)
      return smallest;

    std::vector<EntryT*> tree_table{};
    std::vector<EntryT*> to_visit{};

    for (EntryT* current = m_smallest;
         to_visit.empty() or to_visit[0] != current;
         current = current->next())
      to_visit.push_back(current);

    for (EntryT* current : to_visit) {

      while (true) {

        while (current->degree() >= tree_table.size())
          tree_table.push_back(nullptr);

        if (tree_table[current->degree()] == nullptr) {
          tree_table[current->degree()] = current;
          break;
        }

        EntryT* other = tree_table[current->degree()];
        tree_table[current->degree()] = nullptr;

        EntryT* min = other->priority() < current->priority() ? other : current;
        EntryT* max = other->priority() < current->priority() ? current : other;

        max->next()->prev(max->prev());
        max->prev()->next(max->next());

        max->next(max);
        max->prev(max);
        min->child(merge_lists(min->child(), max));
        max->parent(min);
        max->is_marked(false);
        min->degree(min->degree() + 1);
        current = min;
      }

      if (current->priority() <= m_smallest->priority())
        m_smallest = current;
    }
    return smallest;
  }

  void decrease_key(EntryT* entry, const PriorityT& priority)
  {
    if (priority < entry->priority())
      decrease_key_unchecked(entry, priority);
  }

  void remove(EntryT* entry)
  {
    decrease_key_unchecked(entry, std::numeric_limits<PriorityT>::min());
    pop();
  }
};

template<typename V, typename P>
Entry<V, P>*
merge_lists(Entry<V, P>* lhs, Entry<V, P>* rhs)
{
  if (lhs == nullptr and rhs == nullptr)
    return nullptr;

  if (lhs != nullptr and rhs == nullptr)
    return lhs;

  if (lhs == nullptr and rhs != nullptr)
    return rhs;

  Entry<V, P>* lhs_next = lhs->next();

  lhs->next(rhs->next());

  lhs->next()->prev(lhs);

  rhs->next(lhs_next);

  rhs->next()->prev(rhs);

  return lhs->priority() < rhs->priority() ? lhs : rhs;
}

template<typename V, typename P>
FibonacciHeap<V, P>*
merge(const FibonacciHeap<V, P>* lhs, const FibonacciHeap<V, P>* rhs)
{
  FibonacciHeap<V, P>* result = new FibonacciHeap<V, P>();
  result->m_smallest = merge_lists(lhs->m_smallest, rhs->m_smallest);
  result->m_size = lhs->m_size + rhs->m_size;
  lhs->clear();
  rhs->clear();
  return result;
}
