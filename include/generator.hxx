#ifndef MOIRAI_GENERATOR
#define MOIRAI_GENERATOR

#include <coroutine>

template<typename GeneratorT>
class PromiseT
{
private:
  using self_t = PromiseT<GeneratorT>;
  using value_t = typename GeneratorT::value_t;

  value_t mValue;
  bool mComplete;

public:
  GeneratorT get_return_object()
  {
    return { std::coroutine_handle<self_t>::from_promise(*this) };
  }

  std::suspend_never initial_suspend() { return {}; }

  std::suspend_never final_suspend() noexcept { return {}; }

  void unhandled_exception() {}

  std::suspend_always yield_value(value_t value)
  {
    mValue = value;
    return {};
  }

  auto value() const noexcept -> value_t { return mValue; }

  auto complete() const noexcept -> bool { return mComplete; }
};

template<typename GeneratorT>
class Sentinel
{
};

template<typename GeneratorT>
class Iterator
{
private:
  using self_t = Iterator<GeneratorT>;
  using value_t = typename GeneratorT::value_t;
  using sentinel_t = Sentinel<GeneratorT>;

  GeneratorT& mGenerator;

public:
  value_t operator*() const { return mGenerator.handle().promise().value(); }

  Iterator& operator++()
  {
    mGenerator.handle().resume();
    return *this;
  }

  friend bool operator==(const self_t& iter, sentinel_t)
  {
    return iter.mGenerator.handle().promise().complete();
  }

  friend bool operator!=(const self_t& iter, sentinel_t)
  {
    return not iter.mGenerator.handle().promise().complete();
  }
};

template<typename ValueT>
class generator
{
public:
  using value_t = ValueT;

private:
  using self_t = generator<ValueT>;
  using iterator_t = Iterator<self_t>;
  using sentinel_t = Sentinel<self_t>;

  std::coroutine_handle<PromiseT<self_t>> mHandle;

public:
  iterator_t begin() { return { *this }; }

  sentinel_t end() { return {}; }

  auto handle() const -> decltype(mHandle) { return mHandle; }
};

#endif
