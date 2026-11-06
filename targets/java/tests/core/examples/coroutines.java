final class coroutines {
    static long putchar(long c) {
        System.out.write((int) c);
        System.out.flush();
        return c;
    }

    static Coroutine<Long> fibonacci(long count) {
        return new Coroutine<>(coroutine -> {
            long a = 0L;
            long b = 1L;
            for (long i = 0L; i < count; i = i + 1L) {
                if (coroutine.give(a)) {
                    return;
                }
                final long next = a + b;
                a = b;
                b = next;
            }
        });
    }

    static long main() {
        final Coroutine<Long> fib = fibonacci(7L);
        long sum = 0L;
        boolean done = false;
        for (long i = 0L; i < 8L && done == false; i = i + 1L) {
            final Variant step = fib.resume();
            if (step.tag() == 1L) {
                done = true;
            } else {
                final long value = (Long) step.value();
                putchar(48L + value);
                sum = sum + value;
            }
        }
        putchar(10L);
        if (fib.done() == false) {
            return -1L;
        }
        return sum;
    }

    public static void main(String[] args) {
        System.exit((int) main());
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
}
