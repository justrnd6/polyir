data class Point(val x: Long, val y: Long)
data class Named(val name: String, val at: Point)

fun checkAnArrayGrowsByMakingANewOne(): Double {
    val a0: MutableList<Long> = mutableListOf()
    val a1: MutableList<Long> = (a0 + 10L).toMutableList()
    val a2: MutableList<Long> = (a1 + 20L).toMutableList()
    return offInt(a0.size.toLong(), 0L) + offInt(a1.size.toLong(), 1L) + offInt(a2.size.toLong(), 2L) + offInt(a2[0], 10L) + offInt(a2[1], 20L)
}

fun checkThawAndFreezeCopy(): Double {
    val a: MutableList<Long> = mutableListOf(1L, 2L)
    val m: MutableList<Long> = a.toMutableList()
    m.add(3L)
    val f: MutableList<Long> = m.toMutableList()
    m.add(4L)
    return offInt(a.size.toLong(), 2L) + offInt(m.size.toLong(), 4L) + offInt(f.size.toLong(), 3L) + offInt(f[2], 3L)
}

fun checkAMapGrowsByMakingANewOne(): Double {
    val m0: MutableMap<Long, Long> = mutableMapOf()
    val m1: MutableMap<Long, Long> = (m0 + (1L to 100L)).toMutableMap()
    var m: MutableMap<Long, Long> = m1
    run {
        var i: Long = 2L
        while (i < 30L) {
            m = (m + (i to i * 100L)).toMutableMap()
            i = i + 1L
        }
    }
    return offInt(m0.size.toLong(), 0L) + offInt(m1.size.toLong(), 1L) + offInt(m.size.toLong(), 29L) + offInt(m[1L] ?: 0L, 100L) + offInt(m[29L] ?: 0L, 2900L) + offInt(m1[2L] ?: 0L, 0L)
}

fun checkWithReplacesAnExistingKey(): Double {
    val m: MutableMap<Long, Long> = ((mutableMapOf() + (1L to 10L)).toMutableMap() + (1L to 11L)).toMutableMap()
    return offInt(m.size.toLong(), 1L) + offInt(m[1L] ?: 0L, 11L)
}

fun checkHasTellsPresentFromAbsent(): Double {
    val m: MutableMap<String, Long> = (mutableMapOf() + ("a" to 0L)).toMutableMap()
    var score: Long = 0L
    if (m.containsKey("a")) {
        score = score + 1L
    }
    if (m.containsKey("b") == false) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkASetKeepsEachMemberOnce(): Double {
    val s: MutableSet<Long> = (((mutableSetOf() + 7L).toMutableSet() + 8L).toMutableSet() + 7L).toMutableSet()
    var score: Long = 0L
    if (s.contains(7L)) {
        score = score + 1L
    }
    if (s.contains(9L) == false) {
        score = score + 2L
    }
    return offInt(s.size.toLong(), 2L) + offInt(score, 3L)
}

fun checkAMutsetChangesInPlace(): Double {
    val s: MutableSet<String> = mutableSetOf()
    s.add("x")
    s.add("y")
    s.add("x")
    var score: Long = 0L
    if (s.contains("y")) {
        score = score + 1L
    }
    if (s.contains("z") == false) {
        score = score + 2L
    }
    return offInt(s.size.toLong(), 2L) + offInt(score, 3L)
}

fun checkAMutmapChangesInPlace(): Double {
    val m: MutableMap<Long, Long> = mutableMapOf()
    m[1L] = 10L
    m[2L] = 20L
    m[1L] = 11L
    return offInt(m.size.toLong(), 2L) + offInt(m[1L] ?: 0L, 11L) + offInt(m[2L] ?: 0L, 20L)
}

fun checkAStructCanBeAKey(): Double {
    val seen: MutableMap<Point, Long> = mutableMapOf()
    seen[Point(1L, 2L)] = 10L
    seen[Point(3L, 4L)] = 20L
    seen[Point(1L, 2L)] = 11L
    var score: Long = 0L
    if (seen.containsKey(Point(3L, 4L))) {
        score = score + 1L
    }
    if (seen.containsKey(Point(9L, 9L)) == false) {
        score = score + 2L
    }
    return offInt(seen.size.toLong(), 2L) + offInt(seen[Point(1L, 2L)] ?: 0L, 11L) + offInt(score, 3L)
}

fun checkAStructWithAStringIsAKeyByContent(): Double {
    val names: MutableSet<Named> = (((mutableSetOf() + Named("a" + "b", Point(1L, 1L))).toMutableSet() + Named("ab", Point(1L, 1L))).toMutableSet() + Named("c", Point(2L, 2L))).toMutableSet()
    var score: Long = 0L
    if (names.contains(Named("c", Point(2L, 2L)))) {
        score = score + 1L
    }
    return offInt(names.size.toLong(), 2L) + offInt(score, 1L)
}

fun checkAnImmutableArrayIsAKey(): Double {
    val lists: MutableMap<MutableList<Long>, String> = ((mutableMapOf() + (mutableListOf(1L, 2L) to "one-two")).toMutableMap() + (mutableListOf(3L) to "three")).toMutableMap()
    var score: Long = 0L
    if (lists[mutableListOf(1L, 2L)] ?: "" == "one-two") {
        score = score + 1L
    }
    if (lists[mutableListOf(3L)] ?: "" == "three") {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn(): Double {
    val a: MutableMap<Long, String> = ((mutableMapOf() + (1L to "a")).toMutableMap() + (2L to "b")).toMutableMap()
    val b: MutableMap<Long, String> = ((mutableMapOf() + (2L to "b")).toMutableMap() + (1L to "a")).toMutableMap()
    val c: MutableMap<Long, String> = ((mutableMapOf() + (2L to "b")).toMutableMap() + (1L to "z")).toMutableMap()
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != c) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn(): Double {
    val a: MutableSet<String> = ((mutableSetOf() + "x").toMutableSet() + "y").toMutableSet()
    val b: MutableSet<String> = ((mutableSetOf() + "y").toMutableSet() + "x").toMutableSet()
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a != (mutableSetOf() + "x").toMutableSet()) {
        score = score + 2L
    }
    return offInt(score, 3L)
}

fun checkMutableAndImmutableListsAreEqualByContent(): Double {
    val a: MutableList<Long> = mutableListOf(1L, 2L)
    val b: MutableList<Long> = mutableListOf(1L, 2L)
    var score: Long = 0L
    if (a == b) {
        score = score + 1L
    }
    if (a === b) {
        score = score + 100L
    }
    return offInt(score, 1L)
}

fun checkRepeatedWithInALoopIsNotQuadraticInMemory(): Double {
    var total: Long = 0L
    run {
        var r: Long = 0L
        while (r < 200L) {
            var a: MutableList<Long> = mutableListOf()
            var m: MutableMap<Long, Long> = mutableMapOf()
            var s: MutableSet<Long> = mutableSetOf()
            run {
                var i: Long = 0L
                while (i < 20L) {
                    a = (a + i).toMutableList()
                    m = (m + (i to i * 2L)).toMutableMap()
                    s = (s + i % 7L).toMutableSet()
                    i = i + 1L
                }
            }
            total = total + a.size.toLong() + (m[19L] ?: 0L) + s.size.toLong()
            r = r + 1L
        }
    }
    return offInt(total, 200L * 65L)
}
