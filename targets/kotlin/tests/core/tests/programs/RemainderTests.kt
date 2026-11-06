fun checkFloatRemainder(): Double {
    val a: Float = 7.5f
    val b: Float = 2.0f
    return off(a % b, 1.5f)
}

fun checkFloatRemainderKeepsTheSignOfTheLeftNumber(): Double {
    val a: Float = (0.0 - 7.5).toFloat()
    val b: Float = 2.0f
    return off(a % b, (0.0 - 1.5).toFloat())
}

fun checkFloatRemainderIgnoresTheSignOfTheRightNumber(): Double {
    val a: Float = 7.5f
    val b: Float = (0.0 - 2.0).toFloat()
    return off(a % b, 1.5f)
}

fun checkFloatRemainderOfNumbersThatDivideEvenly(): Double {
    val b: Float = 47.0f
    val c: Float = 97.0f
    return off((2867.0 % (b.toDouble())).toFloat(), 0.0f) + off((5969.0 % (b.toDouble())).toFloat(), 0.0f) + off((9409.0 % (c.toDouble())).toFloat(), 0.0f)
}

fun checkFloatRemainderOfZero(): Double {
    val a: Float = 0.0f
    val b: Float = 5.0f
    return off(a % b, 0.0f)
}

fun checkFloatRemainderOfAFractionOfOne(): Double {
    val a: Float = 0.75f
    val b: Float = 1.0f
    return off(a % b, 0.75f)
}

fun checkWholeNumberRemainder(): Double {
    val a: Long = 17L
    val b: Long = 5L
    return offInt(a % b, 2L) + offInt((0L - a) % b, 0L - 2L)
}
