// The standard library names polyir uses, introduced once so the rest of the code writes them short (the style rule against namespace-qualified
// names). Types get PascalCase aliases (`String`, `Vector<T>`), following the rule that a type's name is PascalCase, so code writes them without
// `std::`; functions and objects are brought in by their own names.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <system_error>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

namespace polyir {

  using String = std::string;
  using StringView = std::string_view;
  template <typename T>
  using View = std::span<const T>;
  template <typename T>
  using Vector = std::vector<T>;
  template <typename T>
  using Deque = std::deque<T>;
  template <typename K, typename V>
  using Map = std::map<K, V>;
  template <typename T>
  using Set = std::set<T>;
  template <typename A, typename B>
  using Pair = std::pair<A, B>;
  template <typename T>
  using Optional = std::optional<T>;
  template <typename K, typename V>
  using HashMap = std::unordered_map<K, V>;
  template <typename T>
  using SharedPtr = std::shared_ptr<T>;
  template <typename T>
  using UniquePtr = std::unique_ptr<T>;
  template <typename T>
  using InitializerList = std::initializer_list<T>;
  template <typename Signature>
  using Function = std::function<Signature>;
  using Mutex = std::mutex;
  template <typename M>
  using LockGuard = std::lock_guard<M>;
  using RuntimeError = std::runtime_error;
  using Exception = std::exception;
  using OStringStream = std::ostringstream;
  using IStringStream = std::istringstream;
  using StringStream = std::stringstream;
  using OFStream = std::ofstream;
  using OStream = std::ostream;
  using ErrorCode = std::error_code;
  using Path = std::filesystem::path;
  using IFStream = std::ifstream;
  using SteadyClock = std::chrono::steady_clock;
  using Milliseconds = std::chrono::duration<double, std::milli>;

  using std::all_of;
  using std::find_if;
  using std::any_of;
  using std::begin;
  using std::end;
  using std::cerr;
  using std::to_string;
  using std::cout;
  using std::const_pointer_cast;
  using std::find;
  using std::getline;
  using std::make_shared;
  using std::make_move_iterator;
  using std::make_unique;
  using std::none_of;
  using std::max;
  using std::memcmp;
  using std::memcpy;
  using std::min;
  using std::move;
  using std::nullopt;
  using std::snprintf;
  using std::sort;
  using std::static_pointer_cast;
  using std::strtod;
  using std::strtoll;
  using std::system;

}  // namespace polyir
