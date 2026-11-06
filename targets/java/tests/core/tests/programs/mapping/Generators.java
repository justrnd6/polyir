final class Generators {
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

    static Coroutine<Long> countdown(long from) {
        return new Coroutine<>(coroutine -> {
            long n = from;
            for (long i = 0L; i < from; i = i + 1L) {
                if (coroutine.give(n)) {
                    return;
                }
                n = n - 1L;
            }
        });
    }

    static long total(Coroutine<Long> source) {
        long sum = 0L;
        boolean finished = false;
        for (long i = 0L; i < 100L && finished == false; i = i + 1L) {
            final Variant step = source.resume();
            if (step.tag() == 1L) {
                finished = true;
            } else {
                sum = sum + (Long) step.value();
            }
        }
        return sum;
    }

    static long main() {
        long score = total(fibonacci(7L));
        score = score + total(countdown(4L));
        final Coroutine<Long> two = countdown(3L);
        final Variant first = two.resume();
        final Variant second = two.resume();
        score = score + (Long) first.value() * 10L + (Long) second.value();
        if (two.done() == false) {
            score = score + 100L;
        }
        return score;
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
