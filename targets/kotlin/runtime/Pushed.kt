// A push used as a value gives the list, as in polyir (Kotlin's add gives whether it changed the list).
private fun <T> pushed(items: MutableList<T>, item: T): MutableList<T> {
    items.add(item)
    return items
}
