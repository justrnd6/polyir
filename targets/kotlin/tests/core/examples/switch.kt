fun dayKind(day: Long): Long {
    var label: Long = 0L
    machine@ while (true) {
        when (label) {
            0L -> {
                if (day == 0L) {
                    label = 1L
                    continue@machine
                } else if (day == 6L) {
                    label = 1L
                    continue@machine
                } else if (day == 1L) {
                    label = 2L
                    continue@machine
                } else if (day == 2L) {
                    label = 2L
                    continue@machine
                } else if (day == 3L) {
                    label = 2L
                    continue@machine
                } else if (day == 4L) {
                    label = 2L
                    continue@machine
                } else if (day == 5L) {
                    label = 2L
                    continue@machine
                } else {
                    label = 3L
                    continue@machine
                }
            }
            1L -> {
                return 0L
            }
            2L -> {
                return 1L
            }
            3L -> {
                return 0L - 1L
            }
        }
    }
}
