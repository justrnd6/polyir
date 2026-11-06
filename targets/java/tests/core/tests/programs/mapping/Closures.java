import java.util.Arrays;
import java.util.Objects;

final class Closures {
    static boolean flag = false;
    static double ratio = 1.5;
    static long hits = 0L;

    static final class Node {
        long value;
        Node next;

        Node(long value, Node next) {
            this.value = value;
            this.next = next;
        }

        Node copy() {
            return new Node(value, next);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Node that && Arrays.equals(new Object[] {value, next}, new Object[] {that.value, that.next});
        }

        @Override
        public int hashCode() {
            return Objects.hash(value, next);
        }
    }

    record Holder(Fn1<Long, Long> op, String name) {
        public long run(long x) {
            final Fn1<Long, Long> f = this.op;
            return f.call(x);
        }

        public Fn1<Long, Long> viaLater() {
            return (y) -> {
                final Fn1<Long, Long> f = this.op;
                return f.call(y) + textLength(this.name);
            };
        }
    }

    static Coroutine<Node> pairs(long n) {
        return new Coroutine<>(coroutine -> {
            for (long i = 0L; i < n; i = i + 1L) {
                if (coroutine.give(new Node(i, null))) {
                    return;
                }
            }
        });
    }

    static long sumNodes(long n) {
        final Coroutine<Node> g = pairs(n);
        long s = 0L;
        boolean done = false;
        for (long k = 0L; k < 20L && done == false; k = k + 1L) {
            final Variant r = g.resume();
            if (r.tag() == 1L) {
                done = true;
            } else {
                s = s + ((Node) r.value()).value;
            }
        }
        return s;
    }

    static long chain() {
        final Node a = new Node(1L, null);
        final Node b = new Node(2L, a);
        final Node c = new Node(3L, b);
        long sum = 0L;
        Node cur = c;
        for (long i = 0L; i < 5L && cur != null; i = i + 1L) {
            final Node n = present(cur);
            sum = sum + n.value;
            cur = n.next;
        }
        a.value = 10L;
        return sum * 100L + present(b.next).value;
    }

    static long nestedClosures(long a) {
        final long[] total = {0L};
        final Fn1<Long, Long> outer = (x) -> {
            final Fn1<Long, Long> inner = (y) -> {
                total[0] = total[0] + y;
                return x * y + a;
            };
            return inner.call(x + 1L) + inner.call(2L);
        };
        final long r = outer.call(3L);
        return r * 1000L + total[0];
    }

    static long strClosure(String prefix) {
        final String tail = "!";
        final Fn1<String, String> f = (s) -> prefix + s + tail;
        return textLength(f.call("abc"));
    }

    static long holders() {
        final Holder h = new Holder((x) -> x * x, "hh");
        final Fn1<Long, Long> later = h.viaLater();
        return h.run(5L) + later.call(2L);
    }

    static long globals() {
        hits = hits + 5L;
        flag = true;
        ratio = ratio * 2.0;
        long r = hits;
        if (flag) {
            r = r + 100L;
        }
        if (ratio > 2.5) {
            r = r + 1000L;
        }
        return r;
    }

    static long findFirst(long limit) {
        for (long i = 0L; i < limit; i = i + 1L) {
            for (long j = 0L; j < limit; j = j + 1L) {
                if (i * j == 12L) {
                    return i * 100L + j;
                }
            }
        }
        return 0L - 1L;
    }

    static long tryInLoop(long n) {
        long s = 0L;
        for (long i = 0L; i < n; i = i + 1L) {
            try {
                if (i % 2L == 1L) {
                    throw new Thrown(i);
                }
                s = s + 10L;
            } catch (Thrown thrown) {
                final long code = thrown.value;
                s = s + code;
            }
        }
        return s;
    }

    static String lines(String a, String b) {
        long label = 0L;
        String acc = "";
        long n = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    acc = a;
                    n = 0L;
                    label = 1L;
                    continue machine;
                }
                case 1: {
                    if (n < 3L) {
                        label = 2L;
                        continue machine;
                    } else {
                        label = 3L;
                        continue machine;
                    }
                }
                case 2: {
                    acc = acc + b;
                    n = n + 1L;
                    label = 1L;
                    continue machine;
                }
                case 3: {
                    return acc;
                }
            }
        }
    }

    static long lineLen() {
        return textLength(lines("x", "yz"));
    }

    static long machineStrings(long n) {
        long label = 0L;
        long p = 0L;
        String s = "";
        long q = 0L;
        long r = 0L;
        machine:
        while (true) {
            switch ((int) label) {
                case 0: {
                    if (n == 0L) {
                        p = n;
                        s = "p";
                        label = 1L;
                        continue machine;
                    } else {
                        q = n;
                        s = "q";
                        label = 2L;
                        continue machine;
                    }
                }
                case 1: {
                    if (p > 4L) {
                        r = p;
                        label = 3L;
                        continue machine;
                    } else {
                        q = p + 1L;
                        s = s + "a";
                        label = 2L;
                        continue machine;
                    }
                }
                case 2: {
                    if (q > 4L) {
                        r = q;
                        label = 3L;
                        continue machine;
                    } else {
                        p = q + 1L;
                        s = s + "b";
                        label = 1L;
                        continue machine;
                    }
                }
                case 3: {
                    return r * 100L + textLength(s);
                }
            }
        }
    }

    static long precedence(long a, long b) {
        final long x = (a + b) * (a - b) / 3L;
        final long y = a & b | a ^ b;
        final long z = a << 2L + b >> 1L;
        long s = x + y + z;
        if ((a & 1L) == 1L && b > 2L || a == 0L) {
            s = s + 1L;
        }
        final int w = (int) (long) a;
        if ((long) w < 100L && (int) (long) (a + b) < 1000) {
            s = s + 2L;
        }
        return s;
    }

    static long main() {
        long score = sumNodes(5L) + chain() + nestedClosures(2L) + strClosure("ab");
        score = score + holders() + globals() + findFirst(10L) + tryInLoop(6L);
        score = score + lineLen() + machineStrings(0L) + machineStrings(1L) + precedence(13L, 7L);
        return score % 251L;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }

    // The functions a closure can be, by how many arguments they take: `Fn1<A, R>` gives an R, `Act1<A>` gives nothing.
    interface Fn0<R> {
        R call();
    }

    interface Fn1<A, R> {
        R call(A a);
    }

    interface Fn2<A, B, R> {
        R call(A a, B b);
    }

    interface Fn3<A, B, C, R> {
        R call(A a, B b, C c);
    }

    interface Fn4<A, B, C, D, R> {
        R call(A a, B b, C c, D d);
    }

    interface Act0 {
        void call();
    }

    interface Act1<A> {
        void call(A a);
    }

    interface Act2<A, B> {
        void call(A a, B b);
    }

    interface Act3<A, B, C> {
        void call(A a, B b, C c);
    }

    interface Act4<A, B, C, D> {
        void call(A a, B b, C c, D d);
    }

    // A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
    // inside the string, and a search starts inside it.
    static byte[] utf8(String text) {
        return text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    }

    static long textLength(String text) {
        return utf8(text).length;
    }

    static byte textByte(String text, long at) {
        final byte[] bytes = utf8(text);
        return at >= 0 && at < bytes.length ? bytes[(int) at] : 0;
    }

    static String textSlice(String text, long from, long to) {
        final byte[] bytes = utf8(text);
        final int start = (int) Math.max(0, Math.min(from, bytes.length));
        final int end = (int) Math.max(start, Math.min(to, bytes.length));
        return new String(bytes, start, end - start, java.nio.charset.StandardCharsets.UTF_8);
    }

    static long textFind(String text, String part, long from) {
        final byte[] bytes = utf8(text);
        final byte[] wanted = utf8(part);
        for (int at = (int) Math.max(0, Math.min(from, bytes.length)); at + wanted.length <= bytes.length; at++) {
            if (Arrays.equals(bytes, at, at + wanted.length, wanted, 0, wanted.length)) {
                return at;
            }
        }
        return -1;
    }

    // A variant: the number of its case and the value it holds.
    record Variant(long tag, Object value) {}

    // A coroutine: its body runs on a thread of its own, only while a resume waits for it. `resume` gives case 0 with the value
    // the body gives, or case 1 when it has finished; `give` hands a value over and hears whether the coroutine was closed.
    static final class Coroutine<T> {
        interface Body<T> {
            void run(Coroutine<T> coroutine);
        }

        private final java.util.concurrent.SynchronousQueue<Object[]> toCaller = new java.util.concurrent.SynchronousQueue<>();
        private final java.util.concurrent.SynchronousQueue<Boolean> toBody = new java.util.concurrent.SynchronousQueue<>();
        private final Body<T> body;
        private Thread thread;
        private boolean finished;

        Coroutine(Body<T> body) {
            this.body = body;
        }

        Variant resume() {
            if (finished) {
                return new Variant(1L, null);
            }
            if (thread == null) {
                thread = Thread.ofVirtual().start(() -> {
                    body.run(this);
                    handOver(new Object[0]);
                });
            } else {
                send(false);
            }
            final Object[] step = take();
            finished = step.length == 0;
            return finished ? new Variant(1L, null) : new Variant(0L, step[0]);
        }

        boolean give(T value) {
            handOver(new Object[] {value});
            try {
                return toBody.take();
            } catch (InterruptedException stopped) {
                return true;
            }
        }

        boolean done() {
            return finished;
        }

        void close() {
            if (!finished && thread != null) {
                send(true);
            }
            finished = true;
        }

        private void handOver(Object[] step) {
            try {
                toCaller.put(step);
            } catch (InterruptedException stopped) {
                Thread.currentThread().interrupt();
            }
        }

        private void send(boolean closing) {
            try {
                toBody.put(closing);
            } catch (InterruptedException stopped) {
                Thread.currentThread().interrupt();
            }
        }

        private Object[] take() {
            try {
                return toCaller.take();
            } catch (InterruptedException stopped) {
                return new Object[0];
            }
        }
    }

    // Stops the program with a message, as polyir's `fail` does.
    static <T> T fail(String message) {
        System.err.println("trap: " + message);
        System.exit(1);
        return null;
    }

    // The value an optional holds; an empty one stops the program.
    static <T> T present(T value) {
        if (value == null) {
            fail("unwrap of an empty optional");
        }
        return value;
    }

    // What `throw` throws: the code thrown.
    static final class Thrown extends RuntimeException {
        final long value;

        Thrown(long value) {
            super("thrown " + value);
            this.value = value;
        }
    }
}
