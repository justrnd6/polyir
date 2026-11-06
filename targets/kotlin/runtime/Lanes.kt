// An operator lane by lane on two vectors.
private fun <T> lanes(a: List<T>, b: List<T>, op: (T, T) -> T): MutableList<T> = a.indices.map { op(a[it], b[it]) }.toMutableList()
