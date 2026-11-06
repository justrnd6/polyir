#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std::string_literals;

#include "Check.hpp"

#ifndef POLYIR_CORE_HPP
#define POLYIR_CORE_HPP

// What polyir shares (a struct, a list, a map, a set) is held by `Ref`, a shared pointer, so two names for one value see
// each other's changes. `same` compares what two values hold and `hashOf` hashes it, for the maps and sets that hold them.
template <typename T>
using Ref = std::shared_ptr<T>;

struct Same;
struct Hash;

template <typename T>
using List = Ref<std::vector<T>>;

template <typename K, typename V>
using Map = Ref<std::unordered_map<K, V, Hash, Same>>;

template <typename T>
using Set = Ref<std::unordered_set<T, Hash, Same>>;

template <typename T>
bool same(const T& a, const T& b);
template <typename T>
bool same(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b);
template <typename T>
bool same(const std::vector<T>& a, const std::vector<T>& b);
template <typename K, typename V, typename H, typename E>
bool same(const std::unordered_map<K, V, H, E>& a, const std::unordered_map<K, V, H, E>& b);
template <typename T, typename H, typename E>
bool same(const std::unordered_set<T, H, E>& a, const std::unordered_set<T, H, E>& b);
template <typename T>
bool same(const std::optional<T>& a, const std::optional<T>& b);
template <typename T>
std::size_t hashOf(const T& value);
template <typename T>
std::size_t hashOf(const std::shared_ptr<T>& value);
template <typename T>
std::size_t hashOf(const std::vector<T>& items);

struct Same {
  template <typename T>
  bool operator()(const T& a, const T& b) const {
    return same(a, b);
  }
};

struct Hash {
  template <typename T>
  std::size_t operator()(const T& value) const {
    return hashOf(value);
  }
};

template <typename T>
bool same(const T& a, const T& b) {
  return a == b;
}

// Two shared values are the same when they hold the same; an interface's value, with no `==`, only when it is one value.
template <typename T>
bool same(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) {
  if constexpr (requires { *a == *b; }) {
    return a == b || (a != nullptr && b != nullptr && *a == *b);
  } else {
    return a == b;
  }
}

template <typename T>
bool same(const std::vector<T>& a, const std::vector<T>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), Same{});
}

template <typename K, typename V, typename H, typename E>
bool same(const std::unordered_map<K, V, H, E>& a, const std::unordered_map<K, V, H, E>& b) {
  return a.size() == b.size() && std::all_of(a.begin(), a.end(), [&](const auto& entry) {
    const auto found = b.find(entry.first);
    return found != b.end() && same(entry.second, found->second);
  });
}

template <typename T, typename H, typename E>
bool same(const std::unordered_set<T, H, E>& a, const std::unordered_set<T, H, E>& b) {
  return a.size() == b.size() && std::all_of(a.begin(), a.end(), [&](const T& item) { return b.contains(item); });
}

template <typename T>
bool same(const std::optional<T>& a, const std::optional<T>& b) {
  return a.has_value() == b.has_value() && (!a.has_value() || same(*a, *b));
}

template <typename T>
std::size_t hashOf(const T& value) {
  if constexpr (requires { value.hash(); }) {
    return value.hash();
  } else {
    return std::hash<T>{}(value);
  }
}

template <typename T>
std::size_t hashOf(const std::shared_ptr<T>& value) {
  if constexpr (requires { value->hash(); }) {
    return value == nullptr ? 0 : value->hash();
  } else {
    return std::hash<std::shared_ptr<T>>{}(value);
  }
}

template <typename T>
std::size_t hashOf(const std::vector<T>& items) {
  std::size_t hash = items.size();
  for (const T& item : items) {
    hash = hash * 31 + hashOf(item);
  }
  return hash;
}

// A struct's `==` and hash: its fields, each compared with the other's (`allSame(x, other.x, y, other.y)`).
inline bool allSame() {
  return true;
}

template <typename T, typename... Rest>
bool allSame(const T& a, const T& b, const Rest&... rest) {
  return same(a, b) && allSame(rest...);
}

inline std::size_t hashAll() {
  return 0;
}

template <typename T, typename... Rest>
std::size_t hashAll(const T& first, const Rest&... rest) {
  return hashOf(first) * 31 + hashAll(rest...);
}

// New lists, maps and sets.
template <typename T>
List<T> newList() {
  return std::make_shared<std::vector<T>>();
}

template <typename T>
List<T> listOf(std::initializer_list<T> items) {
  return std::make_shared<std::vector<T>>(items);
}

template <typename T>
List<T> newFilled(int64_t count, const T& zero) {
  return std::make_shared<std::vector<T>>(count, zero);
}

template <typename K, typename V>
Map<K, V> newMap() {
  return std::make_shared<std::unordered_map<K, V, Hash, Same>>();
}

template <typename T>
Set<T> newSet() {
  return std::make_shared<std::unordered_set<T, Hash, Same>>();
}
#endif

#ifndef POLYIR_SPARSE_HPP
#define POLYIR_SPARSE_HPP

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
template <typename T>
struct Sparse {
  std::vector<int64_t> rows;
  std::vector<int64_t> cols;
  std::vector<T> values;
  T zero;

  explicit Sparse(const T& zero) : zero(zero) {}

  int64_t size() const {
    return values.size();
  }

  T get(int64_t row, int64_t col) const {
    T found = zero;
    for (std::size_t i = 0; i < values.size(); i++) {
      if (rows[i] == row && cols[i] == col) {
        found = values[i];
      }
    }
    return found;
  }

  void set(const T& value, int64_t row, int64_t col) {
    rows.push_back(row);
    cols.push_back(col);
    values.push_back(value);
  }
};
#endif

#ifndef POLYIR_GRID_HPP
#define POLYIR_GRID_HPP

// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(value, i, j)`
// writes one.
template <typename T>
struct Grid {
  std::vector<int> dims;
  std::vector<T> cells;

  Grid(std::vector<int> sizes, const T& zero) : dims(std::move(sizes)) {
    std::size_t count = 1;
    for (const int size : dims) {
      count *= size;
    }
    cells.assign(count, zero);
  }

  template <typename... Indices>
  std::size_t place(Indices... indices) const {
    std::size_t at = 0;
    std::size_t i = 0;
    ((at = at * dims[i++] + (std::size_t) indices), ...);
    return at;
  }

  template <typename... Indices>
  T get(Indices... indices) const {
    return cells[place(indices...)];
  }

  template <typename... Indices>
  void set(const T& value, Indices... indices) {
    cells[place(indices...)] = value;
  }
};
#endif

#ifndef POLYIR_LISTS_HPP
#define POLYIR_LISTS_HPP

// A push used as a value gives the list; `contains` asks a list for a value; a map gives a key it does not hold its zero.
template <typename T>
List<T> pushed(List<T> items, std::type_identity_t<T> item) {
  items->push_back(item);
  return items;
}

template <typename T>
bool contains(const List<T>& items, const std::type_identity_t<T>& item) {
  return std::any_of(items->begin(), items->end(), [&](const T& each) { return same(each, item); });
}

template <typename K, typename V>
V getOr(const Map<K, V>& table, const std::type_identity_t<K>& key, const std::type_identity_t<V>& zero) {
  const auto found = table->find(key);
  return found == table->end() ? zero : found->second;
}

template <typename K, typename V>
std::vector<K> keysOf(const Map<K, V>& table) {
  std::vector<K> keys;
  for (const auto& entry : *table) {
    keys.push_back(entry.first);
  }
  return keys;
}

template <typename T>
List<T> truncate(List<T> items, int64_t count) {
  if (count < (int64_t) items->size()) {
    items->resize(std::max<int64_t>(count, 0));
  }
  return items;
}

template <typename T>
List<T> concat(const List<T>& a, const List<T>& b) {
  auto out = std::make_shared<std::vector<T>>(*a);
  out->insert(out->end(), b->begin(), b->end());
  return out;
}
#endif

#ifndef POLYIR_EITHER_HPP
#define POLYIR_EITHER_HPP

// An `either`: an ok value or an error, and which one it holds.
template <typename T, typename E>
struct Either {
  bool isOk = false;
  std::optional<T> okHeld;
  std::optional<E> errHeld;

  static Either ok(T value) {
    Either out;
    out.isOk = true;
    out.okHeld = std::move(value);
    return out;
  }

  static Either err(E value) {
    Either out;
    out.errHeld = std::move(value);
    return out;
  }

  T okValue() const {
    return *okHeld;
  }

  E errValue() const {
    return *errHeld;
  }

  bool operator==(const Either& other) const {
    return isOk == other.isOk && same(okHeld, other.okHeld) && same(errHeld, other.errHeld);
  }

  std::size_t hash() const {
    return isOk ? hashOf(*okHeld) : hashOf(*errHeld) * 31 + 1;
  }
};
#endif

#ifndef POLYIR_COPIES_HPP
#define POLYIR_COPIES_HPP

// A copy holds the same items in a new list, map, set or struct; `with` makes one with one more.
template <typename T>
Ref<T> copyOf(const Ref<T>& value) {
  return std::make_shared<T>(*value);
}

template <typename T>
List<T> withItem(const List<T>& items, const std::type_identity_t<T>& item) {
  auto out = copyOf(items);
  out->push_back(item);
  return out;
}

template <typename T>
Set<T> withItem(const Set<T>& items, const std::type_identity_t<T>& item) {
  auto out = copyOf(items);
  out->insert(item);
  return out;
}

template <typename K, typename V>
Map<K, V> withEntry(const Map<K, V>& table, const std::type_identity_t<K>& key, const std::type_identity_t<V>& value) {
  auto out = copyOf(table);
  out->insert_or_assign(key, value);
  return out;
}
#endif

#ifndef POLYIR_THROWN_HPP
#define POLYIR_THROWN_HPP

// What `throw` throws: the code thrown.
struct Thrown {
  int64_t value;
};
#endif

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

struct Bag;
struct Pt;
struct Pocket;
struct Shelf;
struct Cabinet;
struct Crate;
struct GrowKey;
struct OptPoint;

struct Bag {
  List<int64_t> items;
  int64_t total;

  Bag(List<int64_t> items, int64_t total) : items(items), total(total) {}

  bool operator==(const Bag& other) const {
    return allSame(items, other.items, total, other.total);
  }

  std::size_t hash() const {
    return hashAll(items, total);
  }
};

struct Pt {
  int64_t x;
  int64_t y;

  Pt(int64_t x, int64_t y) : x(x), y(y) {}

  bool operator==(const Pt& other) const {
    return allSame(x, other.x, y, other.y);
  }

  std::size_t hash() const {
    return hashAll(x, y);
  }
};

struct Pocket {
  List<int64_t> items;

  Pocket(List<int64_t> items) : items(items) {}

  bool operator==(const Pocket& other) const {
    return allSame(items, other.items);
  }

  std::size_t hash() const {
    return hashAll(items);
  }
};

struct Shelf {
  List<int64_t> books;
  std::string tags;

  Shelf(List<int64_t> books, std::string tags) : books(books), tags(tags) {}

  bool operator==(const Shelf& other) const {
    return allSame(books, other.books, tags, other.tags);
  }

  std::size_t hash() const {
    return hashAll(books, tags);
  }
};

struct Cabinet {
  List<int64_t> drawer;

  Cabinet(List<int64_t> drawer) : drawer(drawer) {}

  bool operator==(const Cabinet& other) const {
    return allSame(drawer, other.drawer);
  }

  std::size_t hash() const {
    return hashAll(drawer);
  }
};

struct Crate {
  List<int64_t> items;

  Crate(List<int64_t> items) : items(items) {}

  bool operator==(const Crate& other) const {
    return allSame(items, other.items);
  }

  std::size_t hash() const {
    return hashAll(items);
  }
};

struct GrowKey {
  int64_t a;
  int64_t b;

  GrowKey(int64_t a, int64_t b) : a(a), b(b) {}

  bool operator==(const GrowKey& other) const {
    return allSame(a, other.a, b, other.b);
  }

  std::size_t hash() const {
    return hashAll(a, b);
  }
};

struct OptPoint {
  int64_t x;
  int64_t y;

  OptPoint(int64_t x, int64_t y) : x(x), y(y) {}

  bool operator==(const OptPoint& other) const {
    return allSame(x, other.x, y, other.y);
  }

  std::size_t hash() const {
    return hashAll(x, y);
  }
};

int64_t pushThree(List<int64_t> xs);
int64_t pushMany(List<int64_t> xs, int64_t count);
double checkAPushInsideAFunctionIsSeenByTheCaller();
double checkManyPushesInsideAFunctionLeaveTheCallerValid();
double checkTwoNamesForOneDarrayShareIt();
double checkAnElementWrittenInAFunctionIsSeenByTheCaller();
int64_t overwriteFirst(List<int64_t> xs, int64_t value);
List<int64_t> makeFilled(int64_t count);
double checkAReturnedDarrayIsTheOneTheFunctionBuilt();
double checkAStructFieldIsTheSameDarray();
double checkAMethodStylePushOnAFieldWorks();
double checkConcatenationMakesANewDarray();
double checkAClosureSharesTheDarrayItCaptures();
double checkPushGivesBackTheSameDarray();
double checkReassigningAVarSwitchesDarrays();
double checkAListOfListsSharesItsInnerLists();
double checkADarrayOfStructsKeepsTheStructs();
double checkAThousandPushesKeepEveryElement();
double checkAListInsideAnEitherIsTheSameList();
int64_t fillSparse(Ref<Sparse<int64_t>> sp);
double checkASparseWrittenInAFunctionIsSeenByTheCaller();
double checkLastWriteWinsForASparse();
double checkOneDimensionalSparseSharesToo();
int64_t fillMap(Map<int64_t, int64_t> hm);
double checkAHashmapWrittenInAFunctionIsSeenByTheCaller();
double checkTwoNamesForOneHashmapShareIt();
double checkAStringKeyedHashmapIsShared();
int64_t addWord(Map<std::string, int64_t> hm, std::string word, int64_t value);
double checkAHashmapOfListsKeepsTheLists();
int64_t fillGrid(Ref<Grid<int64_t>> g);
double checkAGridFilledInAFunctionIsSeenByTheCaller();
double checkTwoNamesForOneGridShareIt();
double checkAGridStartsZeroed();
double checkDarrayCopyIsIndependent();
double checkGridAndTensorCopyAreIndependent();
double checkSparseCopyIsIndependent();
double checkHashmapCopyIsIndependent();
double checkCopyOfAStringKeyedHashmap();
List<int64_t> makeBag(int64_t n);
int64_t sumGrid(Ref<Grid<int64_t>> g);
int64_t holdsOnto(List<List<int64_t>> store, List<int64_t> d);
double checkAReturnedContainerSurvives();
double checkAContainerLentToACalleeIsStillUsableAfterwards();
double checkAContainerGivenToACalleeThatKeepsItSurvives();
double checkAContainerReturnedFromInsideAnIfSurvives();
int64_t earlyExit(int64_t n);
double checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue();
std::string greeting(std::string name);
int64_t shout(std::string s);
std::function<int64_t()> counterFrom(int64_t start);
int64_t applyTwice(std::function<int64_t()> f);
double checkAReturnedStringSurvives();
double checkAStringLentToACalleeIsUsableAfterwards();
double checkAStringUsedAsAHashmapKeyIsKept();
double checkAClosureThatIsReturnedKeepsItsBox();
double checkAClosureGivenToACalleeKeepsWorking();
double checkAClosureOnlyCalledLocallyInALoop();
double checkTwoClosuresShareOneCapturedName();
List<int64_t> madeHere();
List<int64_t> madeThroughAnother();
List<int64_t> madeInPlace();
List<int64_t> sameAsArgument(List<int64_t> d);
List<int64_t> elementOf(List<List<int64_t>> xs);
List<int64_t> fieldOf(Ref<Pocket> p);
List<int64_t> keptAndReturned(List<List<int64_t>> store);
List<int64_t> sometimesTheArgument(int64_t flag, List<int64_t> d);
double checkAFreshResultIsUsableAndReleased();
double checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner();
double checkAnObjectKeptByTheCalleeSurvivesItsCaller();
List<int64_t> handedOn();
double checkAResultHandedOnAgainIsStillFresh();
double checkADiscardedFreshResultIsHarmless();
Ref<Pocket> madeStruct(int64_t n);
std::string makeLabel(std::string a);
double checkAStructAndAStringHandedOverByACall();
Ref<Shelf> stockedShelf(int64_t n);
List<int64_t> openDrawer(Ref<Cabinet> c);
double checkAnOwnedFieldIsUsableThenReleasedWithItsStruct();
double checkAReplacedFieldValueIsReleasedAndTheNewOneWorks();
double checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct();
double checkACopiedStructSharesNothingItMustNotFree();
double checkAListOfListsReleasesItsElements();
List<int64_t> firstOf(List<List<int64_t>> xs);
double checkAnElementReadOutOfAListIsNotReleasedWithIt();
double checkAMapPastItsStartingCapacityKeepsEveryEntry();
double checkAGrownMapIsStillTheSameMapForItsAliases();
double checkStringKeysSurviveGrowing();
double checkStructKeysSurviveGrowing();
double checkASetPastItsStartingCapacityKeepsEveryMember();
int64_t readAt(List<int64_t> xs, int64_t i);
int64_t caughtCode(List<int64_t> xs, int64_t i);
double checkAnIndexPastTheEndOfAListThrowsMinusOne();
double checkANegativeIndexThrowsMinusOne();
double checkAnIndexInsideTheListStillReads();
double checkAnEmptyListCannotBeRead();
int64_t divideBy(int64_t a, int64_t b);
double checkDividingByZeroThrowsMinusFour();
double checkAFixedArrayAndAGridAreCheckedPerDimension();
std::optional<int64_t> optionalFind(List<int64_t> xs, int64_t want);
std::optional<Ref<OptPoint>> optionalPoint(int64_t ok);
std::optional<std::string> optionalName(int64_t ok);
int64_t unwrapCode(std::optional<int64_t> o);
double checkAnOptionalHoldsAValueOrNone();
double checkAnOptionalOfAStructAndOfAStringWork();
double checkUnwrappingNoneThrowsMinusThree();

inline int64_t pushThree(List<int64_t> xs) {
  xs->push_back(3);
  return (int64_t) xs->size();
}

inline int64_t pushMany(List<int64_t> xs, int64_t count) {
  for (int64_t i = 0; i < count; i = i + 1) {
    xs->push_back(i * 2);
  }
  return (int64_t) xs->size();
}

inline double checkAPushInsideAFunctionIsSeenByTheCaller() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(1);
  pushThree(xs);
  return offInt((int64_t) xs->size(), 2) + offInt(xs->at(1), 3);
}

inline double checkManyPushesInsideAFunctionLeaveTheCallerValid() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(7);
  pushMany(xs, 100);
  return offInt((int64_t) xs->size(), 101) + offInt(xs->at(0), 7) + offInt(xs->at(1), 0) + offInt(xs->at(100), 198);
}

inline double checkTwoNamesForOneDarrayShareIt() {
  const List<int64_t> a = newList<int64_t>();
  const List<int64_t> b = a;
  a->push_back(5);
  b->push_back(6);
  return offInt((int64_t) a->size(), 2) + offInt((int64_t) b->size(), 2) + offInt(a->at(1), 6) + offInt(b->at(0), 5);
}

inline double checkAnElementWrittenInAFunctionIsSeenByTheCaller() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(1);
  xs->push_back(2);
  overwriteFirst(xs, 40);
  return offInt(xs->at(0), 40) + offInt(xs->at(1), 2);
}

inline int64_t overwriteFirst(List<int64_t> xs, int64_t value) {
  xs->at(0) = value;
  return 0;
}

inline List<int64_t> makeFilled(int64_t count) {
  const List<int64_t> xs = newList<int64_t>();
  for (int64_t i = 0; i < count; i = i + 1) {
    xs->push_back(i + 1);
  }
  return xs;
}

inline double checkAReturnedDarrayIsTheOneTheFunctionBuilt() {
  const List<int64_t> xs = makeFilled(4);
  xs->push_back(50);
  return offInt((int64_t) xs->size(), 5) + offInt(xs->at(3), 4) + offInt(xs->at(4), 50);
}

inline double checkAStructFieldIsTheSameDarray() {
  const List<int64_t> xs = newList<int64_t>();
  const Ref<Bag> bag = std::make_shared<Bag>(xs, 0);
  xs->push_back(8);
  bag->items->push_back(9);
  return offInt((int64_t) (bag->items)->size(), 2) + offInt((int64_t) xs->size(), 2) + offInt(bag->items->at(1), 9);
}

inline double checkAMethodStylePushOnAFieldWorks() {
  const Ref<Bag> bag = std::make_shared<Bag>(newList<int64_t>(), 0);
  bag->items->push_back(4);
  bag->items->push_back(5);
  return offInt((int64_t) (bag->items)->size(), 2) + offInt(bag->items->at(1), 5);
}

inline double checkConcatenationMakesANewDarray() {
  const List<int64_t> a = newList<int64_t>();
  const List<int64_t> b = newList<int64_t>();
  a->push_back(1);
  b->push_back(2);
  const List<int64_t> c = concat(a, b);
  a->push_back(99);
  return offInt((int64_t) c->size(), 2) + offInt(c->at(0), 1) + offInt(c->at(1), 2) + offInt((int64_t) a->size(), 2);
}

inline double checkAClosureSharesTheDarrayItCaptures() {
  const List<int64_t> xs = newList<int64_t>();
  const std::function<int64_t(int64_t)> add = [=](int64_t v) -> int64_t {
    xs->push_back(v);
    return (int64_t) xs->size();
  };
  add(10);
  add(20);
  return offInt((int64_t) xs->size(), 2) + offInt(xs->at(1), 20);
}

inline double checkPushGivesBackTheSameDarray() {
  List<int64_t> xs = newList<int64_t>();
  xs = pushed(xs, 5);
  xs = pushed(xs, 6);
  return offInt((int64_t) xs->size(), 2) + offInt(xs->at(1), 6);
}

inline double checkReassigningAVarSwitchesDarrays() {
  List<int64_t> xs = newList<int64_t>();
  const List<int64_t> other_ = newList<int64_t>();
  other_->push_back(1);
  other_->push_back(2);
  other_->push_back(3);
  xs = other_;
  xs->push_back(4);
  return offInt((int64_t) other_->size(), 4) + offInt((int64_t) xs->size(), 4);
}

inline double checkAListOfListsSharesItsInnerLists() {
  const List<List<int64_t>> outer = newList<List<int64_t>>();
  const List<int64_t> a = newList<int64_t>();
  a->push_back(1);
  a->push_back(2);
  outer->push_back(a);
  const List<int64_t> b = newList<int64_t>();
  b->push_back(10);
  outer->push_back(b);
  a->push_back(3);
  const List<int64_t> first = outer->at(0);
  const List<int64_t> second = outer->at(1);
  return offInt((int64_t) outer->size(), 2) + offInt((int64_t) first->size(), 3) + offInt(first->at(2), 3) + offInt(second->at(0), 10);
}

inline double checkADarrayOfStructsKeepsTheStructs() {
  const List<Ref<Pt>> ps = newList<Ref<Pt>>();
  ps->push_back(std::make_shared<Pt>(1, 2));
  ps->push_back(std::make_shared<Pt>(3, 4));
  const Ref<Pt> p = ps->at(1);
  return offInt((int64_t) ps->size(), 2) + offInt(p->x, 3) + offInt(p->y, 4);
}

inline double checkAThousandPushesKeepEveryElement() {
  const List<int64_t> xs = newList<int64_t>();
  for (int64_t i = 0; i < 1000; i = i + 1) {
    xs->push_back(i);
  }
  int64_t total = 0;
  for (int64_t i = 0; i < (int64_t) xs->size(); i = i + 1) {
    total = total + xs->at(i);
  }
  return offInt((int64_t) xs->size(), 1000) + offInt(total, 499500);
}

inline double checkAListInsideAnEitherIsTheSameList() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(4);
  const Either<List<int64_t>, int64_t> r = Either<List<int64_t>, int64_t>::ok(xs);
  xs->push_back(5);
  const List<int64_t> back = r.okValue();
  return offInt((int64_t) back->size(), 2) + offInt(back->at(1), 5);
}

inline int64_t fillSparse(Ref<Sparse<int64_t>> sp) {
  sp->set(5, 1, 1);
  sp->set(7, 2, 3);
  return 0;
}

inline double checkASparseWrittenInAFunctionIsSeenByTheCaller() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  fillSparse(sp);
  return offInt(sp->get(1, 1), 5) + offInt(sp->get(2, 3), 7) + offInt(sp->get(9, 9), 0) + offInt((int64_t) sp->size(), 2);
}

inline double checkLastWriteWinsForASparse() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  sp->set(1, 3, 3);
  sp->set(2, 3, 3);
  sp->set(3, 3, 3);
  return offInt(sp->get(3, 3), 3);
}

inline double checkOneDimensionalSparseSharesToo() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  const Ref<Sparse<int64_t>> alias = sp;
  alias->set(100, 0, 0);
  sp->set(200, 7, 0);
  return offInt(sp->get(0, 0), 100) + offInt(alias->get(7, 0), 200);
}

inline int64_t fillMap(Map<int64_t, int64_t> hm) {
  hm->insert_or_assign(1, 10);
  hm->insert_or_assign(2, 20);
  hm->insert_or_assign(1, 11);
  return 0;
}

inline double checkAHashmapWrittenInAFunctionIsSeenByTheCaller() {
  const Map<int64_t, int64_t> hm = newMap<int64_t, int64_t>();
  fillMap(hm);
  return offInt(getOr(hm, 1, 0), 11) + offInt(getOr(hm, 2, 0), 20) + offInt(getOr(hm, 3, 0), 0) + offInt((int64_t) hm->size(), 2);
}

inline double checkTwoNamesForOneHashmapShareIt() {
  const Map<int64_t, int64_t> a = newMap<int64_t, int64_t>();
  const Map<int64_t, int64_t> b = a;
  a->insert_or_assign(5, 50);
  b->insert_or_assign(6, 60);
  return offInt((int64_t) a->size(), 2) + offInt(getOr(b, 5, 0), 50) + offInt(getOr(a, 6, 0), 60);
}

inline double checkAStringKeyedHashmapIsShared() {
  const Map<std::string, int64_t> hm = newMap<std::string, int64_t>();
  addWord(hm, "one"s, 1);
  addWord(hm, "two"s, 2);
  return offInt(getOr(hm, "one"s, 0), 1) + offInt(getOr(hm, "two"s, 0), 2) + offInt((int64_t) hm->size(), 2);
}

inline int64_t addWord(Map<std::string, int64_t> hm, std::string word, int64_t value) {
  hm->insert_or_assign(word, value);
  return 0;
}

inline double checkAHashmapOfListsKeepsTheLists() {
  const Map<int64_t, List<int64_t>> hm = newMap<int64_t, List<int64_t>>();
  const List<int64_t> l = newList<int64_t>();
  hm->insert_or_assign(7, l);
  l->push_back(5);
  l->push_back(6);
  const List<int64_t> got = getOr(hm, 7, newList<int64_t>());
  return offInt((int64_t) got->size(), 2) + offInt(got->at(1), 6);
}

inline int64_t fillGrid(Ref<Grid<int64_t>> g) {
  for (int64_t r = 0; r < 2; r = r + 1) {
    for (int64_t c = 0; c < 3; c = c + 1) {
      g->set(r * 10 + c, r, c);
    }
  }
  return 0;
}

inline double checkAGridFilledInAFunctionIsSeenByTheCaller() {
  const Ref<Grid<int64_t>> g = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 3}, 0);
  fillGrid(g);
  return offInt(g->get(1, 2), 12) + offInt(g->get(0, 1), 1);
}

inline double checkTwoNamesForOneGridShareIt() {
  const Ref<Grid<int64_t>> a = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 3}, 0);
  const Ref<Grid<int64_t>> b = a;
  a->set(9, 1, 1);
  return offInt(b->get(1, 1), 9);
}

inline double checkAGridStartsZeroed() {
  const Ref<Grid<int64_t>> t = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 2, 2}, 0);
  t->set(5, 1, 1, 1);
  return offInt(t->get(0, 1, 0), 0) + offInt(t->get(1, 1, 1), 5);
}

inline double checkDarrayCopyIsIndependent() {
  const List<int64_t> a = newList<int64_t>();
  a->push_back(1);
  a->push_back(2);
  const List<int64_t> b = copyOf(a);
  b->push_back(3);
  a->push_back(9);
  return offInt((int64_t) a->size(), 3) + offInt((int64_t) b->size(), 3) + offInt(b->at(2), 3) + offInt(a->at(2), 9) + offInt(b->at(0), 1);
}

inline double checkGridAndTensorCopyAreIndependent() {
  const Ref<Grid<int64_t>> g = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 2}, 0);
  g->set(5, 1, 1);
  const Ref<Grid<int64_t>> h = copyOf(g);
  g->set(6, 1, 1);
  const Ref<Grid<int64_t>> t = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 2, 2}, 0);
  t->set(7, 1, 0, 1);
  const Ref<Grid<int64_t>> u = copyOf(t);
  t->set(8, 1, 0, 1);
  return offInt(h->get(1, 1), 5) + offInt(g->get(1, 1), 6) + offInt(u->get(1, 0, 1), 7);
}

inline double checkSparseCopyIsIndependent() {
  const Ref<Sparse<int64_t>> s = std::make_shared<Sparse<int64_t>>(0);
  s->set(7, 1, 2);
  const Ref<Sparse<int64_t>> c = copyOf(s);
  s->set(8, 3, 4);
  c->set(9, 5, 6);
  return offInt((int64_t) s->size(), 2) + offInt((int64_t) c->size(), 2) + offInt(c->get(1, 2), 7);
}

inline double checkHashmapCopyIsIndependent() {
  const Map<int64_t, int64_t> m = newMap<int64_t, int64_t>();
  m->insert_or_assign(1, 10);
  const Map<int64_t, int64_t> c = copyOf(m);
  m->insert_or_assign(2, 20);
  c->insert_or_assign(3, 30);
  return offInt((int64_t) m->size(), 2) + offInt((int64_t) c->size(), 2) + offInt(getOr(c, 1, 0), 10) + offInt(getOr(c, 3, 0), 30);
}

inline double checkCopyOfAStringKeyedHashmap() {
  const Map<std::string, int64_t> m = newMap<std::string, int64_t>();
  m->insert_or_assign("a"s, 1);
  const Map<std::string, int64_t> c = copyOf(m);
  m->insert_or_assign("b"s, 2);
  return offInt(getOr(c, "a"s, 0), 1) + offInt((int64_t) c->size(), 1);
}

inline List<int64_t> makeBag(int64_t n) {
  const List<int64_t> d = newList<int64_t>();
  d->push_back(n);
  d->push_back(n + 1);
  return d;
}

inline int64_t sumGrid(Ref<Grid<int64_t>> g) {
  return g->get(0, 0) + g->get(1, 1);
}

inline int64_t holdsOnto(List<List<int64_t>> store, List<int64_t> d) {
  store->push_back(d);
  return 0;
}

inline double checkAReturnedContainerSurvives() {
  const List<int64_t> a = makeBag(10);
  const List<int64_t> b = makeBag(20);
  return offInt(a->at(0) + a->at(1) + b->at(0) + b->at(1), 62);
}

inline double checkAContainerLentToACalleeIsStillUsableAfterwards() {
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    const Ref<Grid<int64_t>> g = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 2}, 0);
    g->set(i, 0, 0);
    g->set(1, 1, 1);
    total = total + sumGrid(g) + g->get(0, 0);
  }
  return offInt(total, 2 * 1225 + 50 + 0);
}

inline double checkAContainerGivenToACalleeThatKeepsItSurvives() {
  const List<List<int64_t>> store = newList<List<int64_t>>();
  for (int64_t i = 0; i < 3; i = i + 1) {
    const List<int64_t> d = newList<int64_t>();
    d->push_back(i * 10);
    holdsOnto(store, d);
  }
  return offInt(store->at(0)->at(0) + store->at(1)->at(0) + store->at(2)->at(0), 30);
}

inline double checkAContainerReturnedFromInsideAnIfSurvives() {
  const List<int64_t> keep = newList<int64_t>();
  keep->push_back(4);
  const List<int64_t> other_ = newList<int64_t>();
  other_->push_back(5);
  int64_t total = 0;
  for (int64_t i = 0; i < 20; i = i + 1) {
    const List<int64_t> scratch = newList<int64_t>();
    scratch->push_back(i);
    if (i == 7) {
      total = total + scratch->at(0);
    }
  }
  return offInt(total + keep->at(0) + other_->at(0), 16);
}

inline int64_t earlyExit(int64_t n) {
  const List<int64_t> scratch = newList<int64_t>();
  scratch->push_back(n);
  if (n > 5) {
    return scratch->at(0) * 2;
  }
  scratch->push_back(1);
  return (int64_t) scratch->size() + scratch->at(0);
}

inline double checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue() {
  return offInt(earlyExit(9) * 100 + earlyExit(2), 18 * 100 + 4);
}

inline std::string greeting(std::string name) {
  const std::string hello = "hello, "s;
  const std::string full = hello + name + "!"s;
  return full;
}

inline int64_t shout(std::string s) {
  if (s == "hi!"s) {
    return 1;
  }
  return 0;
}

inline std::function<int64_t()> counterFrom(int64_t start) {
  auto n = std::make_shared<int64_t>(start);
  const std::function<int64_t()> next = [=]() -> int64_t {
    (*n) = (*n) + 1;
    return (*n);
  };
  return next;
}

inline int64_t applyTwice(std::function<int64_t()> f) {
  f();
  return f();
}

inline double checkAReturnedStringSurvives() {
  const std::string a = greeting("ann"s);
  const std::string b = greeting("bo"s);
  if (a != "hello, ann!"s) {
    return 1.0;
  }
  if (b != "hello, bo!"s) {
    return 1.0;
  }
  return 0.0;
}

inline double checkAStringLentToACalleeIsUsableAfterwards() {
  int64_t hits = 0;
  for (int64_t i = 0; i < 20; i = i + 1) {
    const std::string s = "h"s + "i"s + "!"s;
    hits = hits + shout(s);
    if (s == "hi!"s) {
      hits = hits + 1;
    }
  }
  return offInt(hits, 40);
}

inline double checkAStringUsedAsAHashmapKeyIsKept() {
  const Map<std::string, int64_t> m = newMap<std::string, int64_t>();
  for (int64_t i = 0; i < 3; i = i + 1) {
    const std::string key = "k"s + "ey"s;
    m->insert_or_assign(key, i);
  }
  return offInt(getOr(m, "key"s, 0), 2) + offInt((int64_t) m->size(), 1);
}

inline double checkAClosureThatIsReturnedKeepsItsBox() {
  const std::function<int64_t()> c = counterFrom(10);
  c();
  return offInt(c(), 12);
}

inline double checkAClosureGivenToACalleeKeepsWorking() {
  auto n = std::make_shared<int64_t>(0);
  const std::function<int64_t()> tick = [=]() -> int64_t {
    (*n) = (*n) + 5;
    return (*n);
  };
  return offInt(applyTwice(tick), 10) + offInt((*n), 10);
}

inline double checkAClosureOnlyCalledLocallyInALoop() {
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    auto count = std::make_shared<int64_t>(i);
    const std::function<int64_t()> bump = [=]() -> int64_t {
      (*count) = (*count) + 1;
      return (*count);
    };
    bump();
    total = total + bump() - i;
  }
  return offInt(total, 100);
}

inline double checkTwoClosuresShareOneCapturedName() {
  auto n = std::make_shared<int64_t>(0);
  const std::function<int64_t()> inc = [=]() -> int64_t {
    (*n) = (*n) + 1;
    return (*n);
  };
  const std::function<int64_t()> read = [=]() { return (*n) * 10; };
  inc();
  inc();
  return offInt(read(), 20);
}

inline List<int64_t> madeHere() {
  const List<int64_t> d = newList<int64_t>();
  d->push_back(1);
  return d;
}

inline List<int64_t> madeThroughAnother() {
  const List<int64_t> d = madeHere();
  d->push_back(2);
  return d;
}

inline List<int64_t> madeInPlace() {
  return newList<int64_t>();
}

inline List<int64_t> sameAsArgument(List<int64_t> d) {
  return d;
}

inline List<int64_t> elementOf(List<List<int64_t>> xs) {
  return xs->at(0);
}

inline List<int64_t> fieldOf(Ref<Pocket> p) {
  return p->items;
}

inline List<int64_t> keptAndReturned(List<List<int64_t>> store) {
  const List<int64_t> d = newList<int64_t>();
  store->push_back(d);
  return d;
}

inline List<int64_t> sometimesTheArgument(int64_t flag, List<int64_t> d) {
  if (flag == 0) {
    return d;
  }
  return newList<int64_t>();
}

inline double checkAFreshResultIsUsableAndReleased() {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    const List<int64_t> a = madeHere();
    const List<int64_t> b = madeThroughAnother();
    const List<int64_t> c = madeInPlace();
    c->push_back(i);
    total = total + (int64_t) a->size() + (int64_t) b->size() + (int64_t) c->size();
  }
  return offInt(total, 100 * 4);
}

inline double checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner() {
  const List<int64_t> src = newList<int64_t>();
  src->push_back(7);
  const List<int64_t> same_ = sameAsArgument(src);
  same_->push_back(8);
  const List<List<int64_t>> outer = newList<List<int64_t>>();
  outer->push_back(src);
  const List<int64_t> inner = elementOf(outer);
  const Ref<Pocket> pocket = std::make_shared<Pocket>(src);
  const List<int64_t> field = fieldOf(pocket);
  const List<int64_t> maybe = sometimesTheArgument(0, src);
  return offInt((int64_t) src->size(), 2) + offInt((int64_t) inner->size(), 2) + offInt((int64_t) field->size(), 2) + offInt((int64_t) maybe->size(), 2);
}

inline double checkAnObjectKeptByTheCalleeSurvivesItsCaller() {
  const List<List<int64_t>> store = newList<List<int64_t>>();
  int64_t total = 0;
  for (int64_t i = 0; i < 5; i = i + 1) {
    const List<int64_t> d = keptAndReturned(store);
    d->push_back(i);
  }
  for (int64_t j = 0; j < 5; j = j + 1) {
    total = total + store->at(j)->at(0);
  }
  return offInt(total, 10);
}

inline List<int64_t> handedOn() {
  const List<int64_t> a = madeHere();
  return a;
}

inline double checkAResultHandedOnAgainIsStillFresh() {
  const List<int64_t> a = handedOn();
  return offInt(a->at(0), 1);
}

inline double checkADiscardedFreshResultIsHarmless() {
  madeHere();
  madeThroughAnother();
  return 0.0;
}

inline Ref<Pocket> madeStruct(int64_t n) {
  const Ref<Pocket> p = std::make_shared<Pocket>(newList<int64_t>());
  p->items->push_back(n);
  return p;
}

inline std::string makeLabel(std::string a) {
  const std::string s = a + "-"s + a;
  return s;
}

inline double checkAStructAndAStringHandedOverByACall() {
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    const Ref<Pocket> p = madeStruct(i);
    const std::string s = makeLabel("x"s);
    if (s == "x-x"s) {
      total = total + p->items->at(0) + 1;
    }
  }
  return offInt(total, 1225 + 50);
}

inline Ref<Shelf> stockedShelf(int64_t n) {
  const Ref<Shelf> s = std::make_shared<Shelf>(newList<int64_t>(), "a"s + "b"s);
  s->books->push_back(n);
  s->books->push_back(n + 1);
  return s;
}

inline List<int64_t> openDrawer(Ref<Cabinet> c) {
  return c->drawer;
}

inline double checkAnOwnedFieldIsUsableThenReleasedWithItsStruct() {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    const Ref<Shelf> s = stockedShelf(i);
    total = total + (int64_t) (s->books)->size() + s->books->at(0) - i;
    if (s->tags == "ab"s) {
      total = total + 1;
    }
  }
  return offInt(total, 100 * 3);
}

inline double checkAReplacedFieldValueIsReleasedAndTheNewOneWorks() {
  const Ref<Shelf> s = stockedShelf(5);
  s->books = newList<int64_t>();
  s->books->push_back(40);
  s->tags = "x"s + "y"s;
  if (s->tags != "xy"s) {
    return 1.0;
  }
  return offInt((int64_t) (s->books)->size(), 1) + offInt(s->books->at(0), 40);
}

inline double checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct() {
  const Ref<Cabinet> c = std::make_shared<Cabinet>(newList<int64_t>());
  c->drawer->push_back(9);
  const List<int64_t> drawer = openDrawer(c);
  drawer->push_back(10);
  return offInt((int64_t) (c->drawer)->size(), 2) + offInt(drawer->at(1), 10);
}

inline double checkACopiedStructSharesNothingItMustNotFree() {
  const Ref<Crate> a = std::make_shared<Crate>(newList<int64_t>());
  a->items->push_back(3);
  const Ref<Crate> b = copyOf(a);
  b->items->push_back(4);
  return offInt((int64_t) (a->items)->size(), 2) + offInt((int64_t) (b->items)->size(), 2);
}

inline double checkAListOfListsReleasesItsElements() {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    const List<List<int64_t>> outer = newList<List<int64_t>>();
    for (int64_t j = 0; j < 3; j = j + 1) {
      outer->push_back(newList<int64_t>());
      outer->at(j)->push_back(i + j);
    }
    total = total + (int64_t) outer->size() + (int64_t) (outer->at(2))->size() + outer->at(1)->at(0) - i;
  }
  return offInt(total, 100 * (3 + 1 + 1));
}

inline List<int64_t> firstOf(List<List<int64_t>> xs) {
  const List<int64_t> first = xs->at(0);
  return first;
}

inline double checkAnElementReadOutOfAListIsNotReleasedWithIt() {
  const List<List<int64_t>> store = newList<List<int64_t>>();
  store->push_back(newList<int64_t>());
  store->at(0)->push_back(6);
  const List<int64_t> got = firstOf(store);
  got->push_back(7);
  return offInt((int64_t) (store->at(0))->size(), 2) + offInt(got->at(1), 7);
}

inline double checkAMapPastItsStartingCapacityKeepsEveryEntry() {
  const Map<int64_t, int64_t> m = newMap<int64_t, int64_t>();
  for (int64_t i = 0; i < 1000; i = i + 1) {
    m->insert_or_assign(i, i * 3);
  }
  int64_t wrong = 0;
  for (int64_t i = 0; i < 1000; i = i + 1) {
    if (getOr(m, i, 0) != i * 3) {
      wrong = wrong + 1;
    }
  }
  return offInt(wrong, 0) + offInt((int64_t) m->size(), 1000);
}

inline double checkAGrownMapIsStillTheSameMapForItsAliases() {
  const Map<int64_t, int64_t> m = newMap<int64_t, int64_t>();
  const Map<int64_t, int64_t> alias = m;
  for (int64_t i = 0; i < 20; i = i + 1) {
    alias->insert_or_assign(i, i);
  }
  return offInt((int64_t) m->size(), 20) + offInt(getOr(m, 19, 0), 19);
}

inline double checkStringKeysSurviveGrowing() {
  const Map<std::string, int64_t> m = newMap<std::string, int64_t>();
  m->insert_or_assign("a"s, 1);
  m->insert_or_assign("b"s, 2);
  m->insert_or_assign("c"s, 3);
  m->insert_or_assign("d"s, 4);
  m->insert_or_assign("a"s, 10);
  return offInt(getOr(m, "a"s, 0), 10) + offInt(getOr(m, "b"s, 0), 2) + offInt(getOr(m, "c"s, 0), 3) + offInt(getOr(m, "d"s, 0), 4) + offInt((int64_t) m->size(), 4);
}

inline double checkStructKeysSurviveGrowing() {
  const Map<Ref<GrowKey>, int64_t> m = newMap<Ref<GrowKey>, int64_t>();
  for (int64_t i = 0; i < 50; i = i + 1) {
    m->insert_or_assign(std::make_shared<GrowKey>(i, i + 1), i);
  }
  int64_t wrong = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    if (getOr(m, std::make_shared<GrowKey>(i, i + 1), 0) != i) {
      wrong = wrong + 1;
    }
  }
  return offInt(wrong, 0) + offInt((int64_t) m->size(), 50);
}

inline double checkASetPastItsStartingCapacityKeepsEveryMember() {
  const Set<int64_t> seen = newSet<int64_t>();
  for (int64_t i = 0; i < 100; i = i + 1) {
    seen->insert(i % 40);
  }
  int64_t absent = 0;
  if (seen->contains(39) == false) {
    absent = absent + 1;
  }
  if (seen->contains(40)) {
    absent = absent + 1;
  }
  return offInt((int64_t) seen->size(), 40) + offInt(absent, 0);
}

inline int64_t readAt(List<int64_t> xs, int64_t i) {
  return xs->at(i);
}

inline int64_t caughtCode(List<int64_t> xs, int64_t i) {
  int64_t code = 0;
  try {
    code = readAt(xs, i);
  } catch (const Thrown& thrown) {
    const int64_t c = thrown.value;
    code = c;
  }
  return code;
}

inline double checkAnIndexPastTheEndOfAListThrowsMinusOne() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(5);
  return offInt(caughtCode(xs, 1), 0 - 1) + offInt(caughtCode(xs, 100000), 0 - 1);
}

inline double checkANegativeIndexThrowsMinusOne() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(5);
  return offInt(caughtCode(xs, 0 - 1), 0 - 1);
}

inline double checkAnIndexInsideTheListStillReads() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(5);
  xs->push_back(6);
  return offInt(caughtCode(xs, 0), 5) + offInt(caughtCode(xs, 1), 6);
}

inline double checkAnEmptyListCannotBeRead() {
  const List<int64_t> xs = newList<int64_t>();
  return offInt(caughtCode(xs, 0), 0 - 1);
}

inline int64_t divideBy(int64_t a, int64_t b) {
  int64_t out = 0;
  try {
    out = a / b;
  } catch (const Thrown& thrown) {
    const int64_t c = thrown.value;
    out = c;
  }
  return out;
}

inline double checkDividingByZeroThrowsMinusFour() {
  return offInt(divideBy(10, 0), 0 - 4) + offInt(divideBy(10, 2), 5);
}

inline double checkAFixedArrayAndAGridAreCheckedPerDimension() {
  int64_t code = 0;
  const Ref<Grid<int64_t>> g = std::make_shared<Grid<int64_t>>(std::vector<int>{2, 2}, 0);
  try {
    g->set(1, 0, 5);
  } catch (const Thrown& thrown) {
    const int64_t c = thrown.value;
    code = c;
  }
  return offInt(code, 0 - 1);
}

inline std::optional<int64_t> optionalFind(List<int64_t> xs, int64_t want) {
  for (int64_t i = 0; i < (int64_t) xs->size(); i = i + 1) {
    if (xs->at(i) == want) {
      return std::optional<int64_t>(i);
    }
  }
  return std::optional<int64_t>();
}

inline std::optional<Ref<OptPoint>> optionalPoint(int64_t ok) {
  if (ok == 1) {
    return std::optional<Ref<OptPoint>>(std::make_shared<OptPoint>(7, 8));
  }
  return std::optional<Ref<OptPoint>>();
}

inline std::optional<std::string> optionalName(int64_t ok) {
  if (ok == 1) {
    return std::optional<std::string>("ann"s);
  }
  return std::optional<std::string>();
}

inline int64_t unwrapCode(std::optional<int64_t> o) {
  int64_t code = 0;
  try {
    code = present(o);
  } catch (const Thrown& thrown) {
    const int64_t c = thrown.value;
    code = c;
  }
  return code;
}

inline double checkAnOptionalHoldsAValueOrNone() {
  const List<int64_t> xs = newList<int64_t>();
  xs->push_back(10);
  xs->push_back(20);
  int64_t seen = 0;
  if (optionalFind(xs, 20).has_value()) {
    seen = seen + 1;
  }
  if (!optionalFind(xs, 99).has_value()) {
    seen = seen + 10;
  }
  return offInt(seen, 11) + offInt(present(optionalFind(xs, 20)), 1);
}

inline double checkAnOptionalOfAStructAndOfAStringWork() {
  const std::optional<Ref<OptPoint>> p = optionalPoint(1);
  int64_t seen = 0;
  if (p.has_value()) {
    seen = seen + present(p)->x + present(p)->y;
  }
  if (!optionalPoint(0).has_value()) {
    seen = seen + 100;
  }
  if (optionalName(1).has_value()) {
    seen = seen + 1000;
  }
  return offInt(seen, 1115);
}

inline double checkUnwrappingNoneThrowsMinusThree() {
  return offInt(unwrapCode(std::optional<int64_t>()), 0 - 3) + offInt(unwrapCode(std::optional<int64_t>(42)), 42);
}
