// A coroutine: the values its body yields, one resume at a time. `resume` gives case 0 with the next value, or case 1 when
// the body has finished. A coroutine closed is not resumed again.
class Coroutine<T>(private val source: Iterator<T>) {
    private var finished = false

    fun resume(): Variant {
        if (finished || !source.hasNext()) {
            finished = true
            return Variant(1L, null)
        }
        return Variant(0L, source.next())
    }

    fun done(): Boolean = finished

    fun close() {
        finished = true
    }
}
