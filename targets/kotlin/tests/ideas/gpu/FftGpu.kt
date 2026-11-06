fun fftStagesGpu(device: Long, stage: Long, first: Long, second: Long, total: Long, batch: Long, direction: Long): Long {
    var source: Long = first
    var target: Long = second
    val scratch: Long = metalEmptyBuffer(device, batch * (total / 2L) * 4L)
    var n: Long = total
    var s: Long = 1L
    run {
        var pass: Long = 0L
        while (pass < 30L) {
            if (n > 1L) {
                val settings: MutableList<Int> = MutableList(4) { 0 }
                settings[0] = total.toInt()
                settings[1] = n.toInt()
                settings[2] = s.toInt()
                settings[3] = direction.toInt()
                if (metalDispatch(device, stage, batch * (total / 2L), source, metalBuffer(device, settings.toLong(), 16L), target, scratch) == 0L) {
                    return 0L
                }
                val swap: Long = source
                source = target
                target = swap
                n = n / 2L
                s = s * 2L
            }
            pass = pass + 1L
        }
    }
    return source
}
