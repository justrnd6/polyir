fun gcd(a0: Long, b0: Long): Long {
    var label: Long = 0L
    var a: Long = 0L
    var b: Long = 0L
    var result: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                a = a0
                b = b0
                label = 1L
                continue@machine
            }
            1L -> {
                if (b == 0L) {
                    result = a
                    label = 3L
                    continue@machine
                } else {
                    label = 2L
                    continue@machine
                }
            }
            2L -> {
                val r: Long = a % b
                a = b
                b = r
                label = 1L
                continue@machine
            }
            3L -> {
                return result
            }
        }
    }
}
