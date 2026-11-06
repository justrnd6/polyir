final class GeneratorTests {
    static long made = 0L;

    static Coroutine<Long> upTo(long limit) {
        return new Coroutine<>(coroutine -> {
            for (long i = 0L; i < limit; i = i + 1L) {
                if (coroutine.give(i)) {
                    return;
                }
            }
        });
    }

    static Coroutine<Long> counted(long limit) {
        made = made + 1L;
        return upTo(limit);
    }

    static double checkSumsEveryValue() {
        long total = 0L;
        final Coroutine<Long> forGen_21_10 = upTo(5L);
        Variant forStep_21_10 = forGen_21_10.resume();
        while (forStep_21_10.tag() == 0L) {
            final long n = (Long) forStep_21_10.value();
            total = total + n;
            forStep_21_10 = forGen_21_10.resume();
        }
        return Check.offInt(total, 10L);
    }

    static double checkEmptyCoroutineRunsNoTurns() {
        long turns = 0L;
        final Coroutine<Long> forGen_29_10 = upTo(0L);
        Variant forStep_29_10 = forGen_29_10.resume();
        while (forStep_29_10.tag() == 0L) {
            final long n = (Long) forStep_29_10.value();
            turns = turns + 1L;
            forStep_29_10 = forGen_29_10.resume();
        }
        return Check.offInt(turns, 0L);
    }

    static long firstAbove(long limit) {
        final Coroutine<Long> forGen_36_10 = upTo(100L);
        Variant forStep_36_10 = forGen_36_10.resume();
        while (forStep_36_10.tag() == 0L) {
            final long n = (Long) forStep_36_10.value();
            if (n > limit) {
                forGen_36_10.close();
                return n;
            }
            forStep_36_10 = forGen_36_10.resume();
        }
        return -1L;
    }

    static double checkReturnLeavesMidway() {
        return Check.offInt(firstAbove(6L), 7L);
    }

    static double checkLoopsNest() {
        long total = 0L;
        final Coroutine<Long> forGen_50_10 = upTo(3L);
        Variant forStep_50_10 = forGen_50_10.resume();
        while (forStep_50_10.tag() == 0L) {
            final long a = (Long) forStep_50_10.value();
            final Coroutine<Long> forGen_51_14 = upTo(2L);
            Variant forStep_51_14 = forGen_51_14.resume();
            while (forStep_51_14.tag() == 0L) {
                final long b = (Long) forStep_51_14.value();
                total = total + a * 10L + b;
                forStep_51_14 = forGen_51_14.resume();
            }
            forStep_50_10 = forGen_50_10.resume();
        }
        return Check.offInt(total, 63L);
    }

    static double checkSourceIsEvaluatedOnce() {
        made = 0L;
        long total = 0L;
        final Coroutine<Long> forGen_61_10 = counted(4L);
        Variant forStep_61_10 = forGen_61_10.resume();
        while (forStep_61_10.tag() == 0L) {
            final long n = (Long) forStep_61_10.value();
            total = total + n;
            forStep_61_10 = forGen_61_10.resume();
        }
        return Check.offInt(made * 100L + total, 106L);
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
