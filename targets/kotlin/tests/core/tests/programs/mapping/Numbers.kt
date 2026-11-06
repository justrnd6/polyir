import kotlin.system.exitProcess

fun mix(h: Long, v: Long): Long {
    return (h * 31L + v) % 1000003L
}

fun wide(): Long {
    val big: Long = 9223372036854775807L
    val wrapped: Long = big + 1L
    val product: Long = 3037000500L * 3037000500L
    val neg: Long = 0L - big
    val quotient: Long = (0L - 7L) / 2L
    val rest: Long = (0L - 7L) % 3L
    var h: Long = 7L
    h = mix(h, (wrapped shr 40L.toInt()) % 1000L)
    h = mix(h, (product shr 33L.toInt()) % 1000L)
    h = mix(h, (neg shr 50L.toInt()) % 1000L)
    h = mix(h, quotient + 100L)
    h = mix(h, rest + 100L)
    h = mix(h, big shl 3L.toInt() shr 60L.toInt())
    h = mix(h, 0L - 1L shr (5L + 100L).toInt())
    return h
}

fun thirtyTwo(): Long {
    val big: Int = 2147483647
    val wrapped: Int = big + 1
    val product: Int = (65536L * 65536L).toInt()
    val other: Int = (123456789L * 987L).toInt()
    val quotient: Int = ((0L - 7L) / 2L).toInt()
    val rest: Int = ((0L - 7L) % 3L).toInt()
    var h: Long = 11L
    h = mix(h, (wrapped.toLong()) % 100003L)
    h = mix(h, (product.toLong()) + 5L)
    h = mix(h, (other.toLong()) % 100003L)
    h = mix(h, (quotient.toLong()) + 100L)
    h = mix(h, (rest.toLong()) + 100L)
    h = mix(h, ((wrapped shr 7.toInt()).toLong()) % 100003L)
    h = mix(h, ((big shl 5.toInt()).toLong()) % 100003L)
    return h
}

fun unsigned32(): Long {
    val top: UInt = 4294967295u
    val wrapped: UInt = top + 2u
    val product: UInt = (4000000000L * 7L).toUInt()
    val under: UInt = (3L - 5L).toUInt()
    val shifted: UInt = top shl 4u.toInt()
    var h: Long = 13L
    h = mix(h, wrapped.toLong())
    h = mix(h, (product.toLong()) % 100003L)
    h = mix(h, (under.toLong()) % 100003L)
    h = mix(h, (shifted.toLong()) % 100003L)
    h = mix(h, ((top shr 3u.toInt()).toLong()) % 100003L)
    h = mix(h, ((top / 3u).toLong()) % 100003L)
    h = mix(h, (top % 1000u).toLong())
    return h
}

fun narrow(): Long {
    val a: Byte = 127.toByte()
    val b: Byte = (a.toInt() + 1.toByte().toInt()).toByte()
    val c: Short = 32767.toShort()
    val d: Short = (c.toInt() + 1.toShort().toInt()).toShort()
    val e: UByte = 250u.toUByte()
    val f: UByte = (e.toUInt() + 10u.toUByte().toUInt()).toUByte()
    val g: UShort = 65535u.toUShort()
    val hh: UShort = (g.toUInt() + 3u.toUShort().toUInt()).toUShort()
    val m: Byte = (0L - 128L).toByte()
    val n: Byte = (m.toInt() / ((0L - 1L).toByte()).toInt()).toByte()
    var h: Long = 17L
    h = mix(h, (b.toLong()) + 200L)
    h = mix(h, (d.toLong()) + 40000L)
    h = mix(h, f.toLong())
    h = mix(h, hh.toLong())
    h = mix(h, (n.toLong()) + 200L)
    h = mix(h, ((b.toInt() shr 2.toByte().toInt()).toByte().toLong()) + 200L)
    h = mix(h, (e.toUInt() shl 3u.toUByte().toInt()).toUByte().toLong())
    return h
}

fun casts(): Long {
    val big: Long = 5000000000L
    val cut: Int = big.toInt()
    val tiny: Byte = 300L.toByte()
    val uns: UInt = (0L - 5L).toUInt()
    val up: ULong = (0L - 1L).toULong()
    val back: Long = up.toLong()
    val f: Double = 3.99
    val g: Double = 0.0 - 3.99
    var h: Long = 19L
    h = mix(h, (cut.toLong()) % 100003L)
    h = mix(h, (tiny.toLong()) + 200L)
    h = mix(h, (uns.toLong()) % 100003L)
    h = mix(h, (up shr 60uL.toInt()).toLong())
    h = mix(h, back + 5L)
    h = mix(h, f.toLong())
    h = mix(h, (g.toLong()) + 10L)
    h = mix(h, (f * 2.5).toLong())
    return h
}

fun main(): Long {
    var h: Long = 1L
    h = mix(h, wide())
    h = mix(h, thirtyTwo())
    h = mix(h, unsigned32())
    h = mix(h, narrow())
    h = mix(h, casts())
    return h % 251L
}

fun main(args: Array<String>) {
    exitProcess(main().toInt())
}
