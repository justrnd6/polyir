#ifndef POLYIR_FAIL_HPP
#define POLYIR_FAIL_HPP

// Stops the program with a message, as polyir's `fail` does.
[[noreturn]] inline void fail(const std::string& message) {
  std::cerr << "trap: " << message << '\n';
  std::exit(1);
}

// The value an optional holds; an empty one stops the program.
template <typename T>
T present(const std::optional<T>& value) {
  if (!value.has_value()) {
    fail("unwrap of an empty optional");
  }
  return *value;
}
#endif
