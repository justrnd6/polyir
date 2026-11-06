// A new list, set or map with one more item.
static <T> ArrayList<T> withItem(ArrayList<T> items, T item) {
    final ArrayList<T> out = new ArrayList<>(items);
    out.add(item);
    return out;
}

static <T> HashSet<T> withItem(HashSet<T> items, T item) {
    final HashSet<T> out = new HashSet<>(items);
    out.add(item);
    return out;
}

static <K, V> HashMap<K, V> withEntry(HashMap<K, V> table, K key, V value) {
    final HashMap<K, V> out = new HashMap<>(table);
    out.put(key, value);
    return out;
}
