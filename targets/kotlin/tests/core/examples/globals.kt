var counter: Long = 0L

fun bump(): Long {
    counter = counter + 1L
    return counter
}
