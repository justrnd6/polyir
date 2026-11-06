fun gcd(a: Long, b: Long): Long {
    var x: Long = a
    var y: Long = b
    while (y != 0L) {
        val r: Long = x % y
        x = y
        y = r
    }
    return x
}

fun lcm(a: Long, b: Long): Long {
    return a / gcd(a, b) * b
}
