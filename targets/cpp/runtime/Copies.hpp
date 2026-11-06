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
