import java.util.Objects;

final class buffer {
    static long fillToCapacity() {
        final long[] arr = new long[4];
        arr[0] = 10L;
        arr[1] = 20L;
        arr[2] = 30L;
        arr[3] = 40L;
        return arr[0] + arr[1] + arr[2] + arr[3];
    }

    static long capacityCheck() {
        final long[] arr = new long[4];
        arr[0] = 1L;
        arr[1] = 2L;
        return (long) arr.length;
    }

    static long letBoundIndexedFill() {
        final long[] arr = new long[3];
        arr[0] = 5L;
        arr[1] = 6L;
        arr[2] = 7L;
        return arr[0] + arr[1] + arr[2];
    }

    static long sumViaLoop(long n) {
        final long[] arr = new long[10];
        for (long i = 0L; i < n; i = i + 1L) {
            arr[(int) i] = i;
        }
        long total = 0L;
        for (long i = 0L; i < n; i = i + 1L) {
            total = total + arr[(int) i];
        }
        return total;
    }

    static long stringElements() {
        final String[] names = new String[2];
        names[0] = "first";
        names[1] = "second";
        if (!Objects.equals(names[0], "first")) {
            return 0L;
        }
        if (!Objects.equals(names[1], "second")) {
            return 0L;
        }
        return 1L;
    }

    static long sumParam(long[] arr) {
        return arr[0] + arr[1] + arr[2] + (long) arr.length;
    }

    static long viaParam() {
        final long[] arr = new long[3];
        arr[0] = 100L;
        arr[1] = 200L;
        arr[2] = 300L;
        return sumParam(arr);
    }
}
