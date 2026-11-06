import kotlin.system.exitProcess

data class Pixel(val r: UByte, val g: UByte, val b: UByte)

fun comparison(): Long {
    val big: UInt = 4000000000u
    if (big > 3000000000u && big >= big && big < 5u == false) {
        return 1L
    }
    return 0L
}

fun division(): Long {
    val big: UInt = 4000000000u
    val half: UInt = big / 2u
    val rest: UInt = big % 7u
    if (half.toLong() == 2000000000L && rest.toLong() == 3L) {
        return 1L
    }
    return 0L
}

fun shifting(): Long {
    val big: UInt = 4000000000u
    val top: ULong = (0L - 1L).toULong()
    if ((big shr 28u.toInt()).toLong() == 14L && (top shr 63uL.toInt()).toLong() == 1L) {
        return 1L
    }
    return 0L
}

fun widening(): Long {
    val big: UInt = 4000000000u
    val small: UByte = 200u.toUByte()
    if (big.toLong() == 4000000000L && small.toLong() == 200L) {
        return 1L
    }
    return 0L
}

fun wrapping(): Long {
    val a: UByte = 200u.toUByte()
    val b: UByte = 100u.toUByte()
    val sum: UByte = (a.toUInt() + b.toUInt()).toUByte()
    val under: UByte = (b.toUInt() - a.toUInt()).toUByte()
    val product: UShort = 300u.toUShort()
    val wide: UShort = (product.toUInt() * 300u.toUShort().toUInt()).toUShort()
    if (sum.toLong() == 44L && under.toLong() == 156L && wide.toLong() == 24464L) {
        return 1L
    }
    return 0L
}

fun floats(): Long {
    val big: UInt = 4000000000u
    if ((big.toDouble()).toLong() == 4000000000L) {
        return 1L
    }
    return 0L
}

fun largest(): Long {
    val top: ULong = (0L - 1L).toULong()
    if (top > 5uL && top / 2uL == 9223372036854775807uL) {
        return 1L
    }
    return 0L
}

fun fnv(n: Long): UInt {
    var h: UInt = 2166136261u
    run {
        var i: Long = 0L
        while (i < n) {
            h = (h xor (i.toUInt())) * 16777619u
            i = i + 1L
        }
    }
    return h
}

fun hashing(): Long {
    if (fnv(3L).toLong() == 581859880L) {
        return 1L
    }
    return 0L
}

fun fields(): Long {
    val a: Pixel = Pixel(250u.toUByte(), 128u.toUByte(), 7u.toUByte())
    val b: Pixel = Pixel(250u.toUByte(), 128u.toUByte(), 7u.toUByte())
    if (a == b && a.g.toLong() == 128L && a.r > a.b) {
        return 1L
    }
    return 0L
}

fun keys(): Long {
    val m: MutableMap<UInt, Long> = mutableMapOf()
    val k: UInt = 4000000000u
    m[k] = 7L
    m[5u] = 9L
    if (m[k] ?: 0L == 7L && m[5u] ?: 0L == 9L) {
        return 1L
    }
    return 0L
}

fun main(): Long {
    return comparison() + division() + shifting() + widening() + wrapping() + floats() + largest() + hashing() + fields() + keys()
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
