#ifndef POLYIR_COROUTINE_HPP
#define POLYIR_COROUTINE_HPP

// A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
// finished; a `co_yield` hears whether the coroutine was closed.
template <typename T>
class Coroutine {
 public:
  struct promise_type {
    std::optional<T> value;
    bool closing = false;

    Coroutine get_return_object() {
      return Coroutine(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_always initial_suspend() noexcept {
      return {};
    }

    std::suspend_always final_suspend() noexcept {
      return {};
    }

    void return_void() {}

    void unhandled_exception() {
      throw;
    }

    struct Heard {
      promise_type* promise;

      bool await_ready() noexcept {
        return false;
      }

      void await_suspend(std::coroutine_handle<>) noexcept {}

      bool await_resume() noexcept {
        return promise->closing;
      }
    };

    Heard yield_value(T next) {
      value = std::move(next);
      return Heard{this};
    }
  };

  explicit Coroutine(std::coroutine_handle<promise_type> handle) : handle(handle) {}

  Coroutine(Coroutine&& other) noexcept : handle(std::exchange(other.handle, {})) {}

  Coroutine(const Coroutine&) = delete;

  ~Coroutine() {
    if (handle) {
      handle.destroy();
    }
  }

  Variant resume() {
    if (finished || handle.done()) {
      finished = true;
      return Variant{1, std::any()};
    }
    started = true;
    handle.promise().value.reset();
    handle.resume();
    finished = handle.done();
    return finished ? Variant{1, std::any()} : makeVariant(0, *handle.promise().value);
  }

  bool done() const {
    return finished;
  }

  void close() {
    if (started && !finished && !handle.done()) {
      handle.promise().closing = true;
      handle.resume();
    }
    finished = true;
  }

 private:
  std::coroutine_handle<promise_type> handle;
  bool started = false;
  bool finished = false;
};
#endif
