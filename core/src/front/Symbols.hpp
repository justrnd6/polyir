// SymbolMap: a hash table keyed by Symbol, for the checker's declaration tables and scopes. Open addressing over the symbol's 32-bit id; values live in a
// deque, so a pointer `find` returned stays valid while more entries are added. The first insertion of a key wins (the Nina checker searches its
// declaration lists front to back, so a later declaration of the same name is never found).
#pragma once

#include "front/Ast.hpp"

namespace polyir::front {

  // A template because each table holds its own kind of value (a function's signature, a struct's fields, a local binding, ...).
  template <typename V>
  struct SymbolMap {
    Vector<uint32_t> slots = Vector<uint32_t>(16, 0);  // the hash table: index + 1 into `values`; 0 is an empty slot
    Vector<uint32_t> keys;  // the symbol id of each value, in insertion order
    Deque<V> values;
  };

  template <typename V> const V* Find(const SymbolMap<V>& map, Symbol key);

  template <typename V> V* Find(SymbolMap<V>& map, Symbol key);

  template <typename V> bool Contains(const SymbolMap<V>& map, Symbol key);

  template <typename V> size_t Size(const SymbolMap<V>& map);

  template <typename V> bool Insert(SymbolMap<V>& map, Symbol key, V value);

  template <typename V> const V* At(const SymbolMap<V>& map, Symbol key);

  template <typename V> size_t SlotOf(const SymbolMap<V>& map, uint32_t id);

  template <typename V> void Grow(SymbolMap<V>& map);

  template <typename V> inline const V* Find(const SymbolMap<V>& map, Symbol key) {
    return At(map, key);
  }

  // The same lookup for a caller that changes the value it finds.
  template <typename V> inline V* Find(SymbolMap<V>& map, Symbol key) {
    return const_cast<V*>(At(map, key));
  }

  template <typename V> inline bool Contains(const SymbolMap<V>& map, Symbol key) {
    return At(map, key) != nullptr;
  }

  template <typename V> inline size_t Size(const SymbolMap<V>& map) {
    return map.values.size();
  }

  // Adds `value` under `key` unless the key is already there (then the map is unchanged and the answer is false).
  template <typename V> inline bool Insert(SymbolMap<V>& map, Symbol key, V value) {
    if (Contains(map, key)) {
      return false;
    }

    map.keys.push_back(key.id);
    map.values.push_back(move(value));
    map.slots[SlotOf(map, key.id)] = static_cast<uint32_t>(map.values.size());

    if (map.values.size() * 2 > map.slots.size()) {
      Grow(map);
    }

    return true;
  }

  // The value stored for `key`, or null.
  template <typename V> inline const V* At(const SymbolMap<V>& map, Symbol key) {
    const uint32_t found = map.slots[SlotOf(map, key.id)];

    return found == 0 ? nullptr : &map.values[found - 1];
  }

  // The slot holding `id`, or the empty slot where it belongs (Fibonacci hashing, then linear probing). A helper of Insert and At.
  template <typename V> inline size_t SlotOf(const SymbolMap<V>& map, uint32_t id) {
    const size_t mask = map.slots.size() - 1;
    size_t slot = (id * 2654435761u) & mask;  // 2654435761 is 2^32 divided by the golden ratio: it spreads nearby ids apart

    while (map.slots[slot] != 0 && map.keys[map.slots[slot] - 1] != id) {
      slot = (slot + 1) & mask;
    }

    return slot;
  }

  // A table twice the size, every key put back where its hash now points. A helper of Insert.
  template <typename V> inline void Grow(SymbolMap<V>& map) {
    map.slots.assign(map.slots.size() * 2, 0);

    for (size_t i = 0; i < map.keys.size(); i++) {
      map.slots[SlotOf(map, map.keys[i])] = static_cast<uint32_t>(i + 1);
    }
  }

}  // namespace polyir::front
