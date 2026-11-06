final class generators {
    static Coroutine<Long> evens(long limit) {
        return new Coroutine<>(coroutine -> {
            for (long i = 0L; i < limit; i = i + 2L) {
                if (coroutine.give(i)) {
                    return;
                }
            }
        });
    }

    static long sumOfEvens() {
        long total = 0L;
        final Coroutine<Long> forGen_14_8 = evens(10L);
        Variant forStep_14_8 = forGen_14_8.resume();
        while (forStep_14_8.tag() == 0L) {
            final long n = (Long) forStep_14_8.value();
            total = total + n;
            forStep_14_8 = forGen_14_8.resume();
        }
        return total;
    }

    static long firstOver(long limit) {
        final Coroutine<Long> forGen_22_8 = evens(100L);
        Variant forStep_22_8 = forGen_22_8.resume();
        while (forStep_22_8.tag() == 0L) {
            final long n = (Long) forStep_22_8.value();
            if (n > limit) {
                forGen_22_8.close();
                return n;
            }
            forStep_22_8 = forGen_22_8.resume();
        }
        return -1L;
    }

    static long pairs() {
        long total = 0L;
        final Coroutine<Long> forGen_33_8 = evens(6L);
        Variant forStep_33_8 = forGen_33_8.resume();
        while (forStep_33_8.tag() == 0L) {
            final long a = (Long) forStep_33_8.value();
            final Coroutine<Long> forGen_34_10 = evens(4L);
            Variant forStep_34_10 = forGen_34_10.resume();
            while (forStep_34_10.tag() == 0L) {
                final long b = (Long) forStep_34_10.value();
                total = total + a * 10L + b;
                forStep_34_10 = forGen_34_10.resume();
            }
            forStep_33_8 = forGen_33_8.resume();
        }
        return total;
    }

    static long main() {
        return sumOfEvens() + firstOver(5L) + pairs();
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
