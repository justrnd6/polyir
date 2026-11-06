fun collisionAndOverwrite(): Long {
    val hm: MutableMap<Long, Long> = mutableMapOf()
    hm[0L] = 100L
    hm[5L] = 500L
    hm[1L] = 10L
    hm[1L] = 20L
    return (hm[0L] ?: 0L) * 10000L + (hm[5L] ?: 0L) * 10L + (hm[1L] ?: 0L)
}

fun missingKeyIsZero(): Long {
    val hm: MutableMap<Long, Long> = mutableMapOf()
    hm[0L] = 7L
    return hm[42L] ?: 0L
}

fun sizeCheck(): Long {
    val hm: MutableMap<Long, Long> = mutableMapOf()
    hm[0L] = 1L
    hm[1L] = 2L
    hm[1L] = 99L
    return hm.size.toLong() * 100L
}

fun stringKeyedWriteAndReadBack(): Long {
    val hm: MutableMap<String, Long> = mutableMapOf()
    hm["one"] = 1L
    hm["two"] = 2L
    hm["one"] = 11L
    return (hm["one"] ?: 0L) * 100L + (hm["two"] ?: 0L)
}

fun stringKeyMissingIsZero(): Long {
    val hm: MutableMap<String, Long> = mutableMapOf()
    hm["present"] = 5L
    return hm["absent"] ?: 0L
}

fun hashmapPtrValueMissIsSafe(): Long {
    val hm: MutableMap<Long, String> = mutableMapOf()
    hm[1L] = "present"
    val missing: String = hm[99L] ?: ""
    return 1L
}
