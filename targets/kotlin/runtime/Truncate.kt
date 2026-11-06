// Shortens a list to at most `count` items, and gives it back.
private fun <T> truncate(items: MutableList<T>, count: Long): MutableList<T> {
    while (items.size > maxOf(count, 0L)) {
        items.removeAt(items.size - 1)
    }
    return items
}
