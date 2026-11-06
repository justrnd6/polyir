fun checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign(): Double {
    val big: UInt = 4000000000u
    var seen: Long = 0L
    if (big > 3000000000u) {
        seen = seen + 1L
    }
    if (big >= big) {
        seen = seen + 10L
    }
    if (big < 5u) {
        seen = seen + 100L
    }
    return offInt(seen, 11L)
}

fun checkAnUnsignedDivisionAndRemainder(): Double {
    val big: UInt = 4000000000u
    val half: UInt = big / 2u
    val rest: UInt = big % 7u
    return offInt(half.toLong(), 2000000000L) + offInt(rest.toLong(), 3L)
}

fun checkAnUnsignedRightShiftFillsWithZeros(): Double {
    val big: UInt = 4000000000u
    val shifted: UInt = big shr 28u.toInt()
    val m: ULong = (0L - 1L).toULong()
    return offInt(shifted.toLong(), 14L) + offInt((m shr 63uL.toInt()).toLong(), 1L)
}

fun checkAnUnsignedWidensWithZeroBits(): Double {
    val big: UInt = 4000000000u
    val small: UByte = 200u.toUByte()
    return offInt(big.toLong(), 4000000000L) + offInt(small.toLong(), 200L)
}

fun checkNarrowUnsignedArithmeticWrapsAtItsSize(): Double {
    val b: UByte = 200u.toUByte()
    val c: UByte = 100u.toUByte()
    val sum: UByte = (b.toUInt() + c.toUInt()).toUByte()
    val under: UByte = (c.toUInt() - b.toUInt()).toUByte()
    return offInt(sum.toLong(), 44L) + offInt(under.toLong(), 156L)
}

fun checkAnUnsignedGoesToAFloatAsItsValue(): Double {
    val big: UInt = 4000000000u
    val asDouble: Double = big.toDouble()
    return offInt(asDouble.toLong(), 4000000000L)
}

fun checkTheLargestU64IsLargerThanFive(): Double {
    val m: ULong = (0L - 1L).toULong()
    var seen: Long = 0L
    if (m > 5uL) {
        seen = seen + 1L
    }
    if (m / 2uL == 9223372036854775807uL) {
        seen = seen + 10L
    }
    return offInt(seen, 11L)
}

data class Pixel(val r: UByte, val g: UByte, val b: UByte)

fun checkAnUnsignedFieldKeepsItsValueAndStructsCompare(): Double {
    val a: Pixel = Pixel(250u.toUByte(), 128u.toUByte(), 7u.toUByte())
    val b: Pixel = Pixel(250u.toUByte(), 128u.toUByte(), 7u.toUByte())
    var seen: Long = 0L
    if (a == b) {
        seen = seen + 1L
    }
    seen = seen + (a.g.toLong())
    return offInt(seen, 129L)
}

fun checkUnsignedKeysAreOrdinaryKeys(): Double {
    val m: MutableMap<UInt, Long> = mutableMapOf()
    val k: UInt = 4000000000u
    m[k] = 7L
    m[5u] = 9L
    return offInt(m[k] ?: 0L, 7L) + offInt(m[5u] ?: 0L, 9L)
}
