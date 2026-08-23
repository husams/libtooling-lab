#ifndef FACTS_TOOL_STORAGE_GENERATOR_H
#define FACTS_TOOL_STORAGE_GENERATOR_H

// A small repository-owned coroutine range. Keeping this implementation local
// gives the storage API one stable single-pass range type on every supported
// standard library. std::generator is deliberately not selected when present:
// libc++ 22 lacks it, and the public range type must not vary by toolchain.

#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <utility>

namespace facts {

template <typename T>
class Generator {
public:
  struct promise_type {
    // The yielded object lives in the *caller's* co_yield full-expression,
    // which outlives the suspension, so the promise only needs a pointer.
    T *value = nullptr;
    std::exception_ptr error;

    Generator get_return_object() {
      return Generator(
          std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_always initial_suspend() const noexcept { return {}; }

    std::suspend_always final_suspend() const noexcept { return {}; }

    std::suspend_always yield_value(T &yielded) noexcept {
      value = std::addressof(yielded);
      return {};
    }

    std::suspend_always yield_value(T &&yielded) noexcept {
      value = std::addressof(yielded);
      return {};
    }

    void return_void() const noexcept {}

    void unhandled_exception() { error = std::current_exception(); }
  };

  class iterator {
  public:
    using iterator_concept = std::input_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    explicit iterator(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    T &operator*() const { return *handle_.promise().value; }

    T *operator->() const { return handle_.promise().value; }

    iterator &operator++() {
      handle_.resume();
      rethrowIfFailed();
      return *this;
    }

    void operator++(int) { ++*this; }

    bool operator==(std::default_sentinel_t) const {
      return !handle_ || handle_.done();
    }

  private:
    void rethrowIfFailed() const {
      if (handle_.done() && handle_.promise().error) {
        std::rethrow_exception(handle_.promise().error);
      }
    }

    std::coroutine_handle<promise_type> handle_;
  };

  Generator() = default;

  Generator(Generator &&other) noexcept
      : handle_(std::exchange(other.handle_, {})) {}

  Generator &operator=(Generator &&other) noexcept {
    if (this != &other) {
      destroy();
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  ~Generator() { destroy(); }

  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  // Resuming here runs the body up to the first co_yield: nothing the
  // coroutine owns (a prepared statement, say) is acquired until iteration.
  iterator begin() {
    if (handle_) {
      handle_.resume();
      if (handle_.done() && handle_.promise().error) {
        std::rethrow_exception(handle_.promise().error);
      }
    }
    return iterator(handle_);
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  explicit Generator(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {}

  void destroy() noexcept {
    if (handle_) {
      handle_.destroy();
      handle_ = {};
    }
  }

  std::coroutine_handle<promise_type> handle_;
};

} // namespace facts

#endif // FACTS_TOOL_STORAGE_GENERATOR_H
