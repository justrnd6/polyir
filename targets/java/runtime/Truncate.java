// Shortens a list to at most `count` items, and gives it back.
static <T> ArrayList<T> truncate(ArrayList<T> items, long count) {
    while (items.size() > Math.max(count, 0)) {
        items.remove(items.size() - 1);
    }
    return items;
}
