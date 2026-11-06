final class switch_ {
    static long dayKind(long day) {
        long label = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    if (day == 0L) {
                        label = 1L;
                        continue machine;
                    } else if (day == 6L) {
                        label = 1L;
                        continue machine;
                    } else if (day == 1L) {
                        label = 2L;
                        continue machine;
                    } else if (day == 2L) {
                        label = 2L;
                        continue machine;
                    } else if (day == 3L) {
                        label = 2L;
                        continue machine;
                    } else if (day == 4L) {
                        label = 2L;
                        continue machine;
                    } else if (day == 5L) {
                        label = 2L;
                        continue machine;
                    } else {
                        label = 3L;
                        continue machine;
                    }
                }
                case 1: {
                    return 0L;
                }
                case 2: {
                    return 1L;
                }
                case 3: {
                    return 0L - 1L;
                }
            }
        }
    }
}
