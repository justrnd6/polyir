final class yielded {
    static Coroutine<Long> count(long n) {
        return new Coroutine<>(coroutine -> {
            boolean gone = false;
            for (long i = 0L; i < n; i = i + 1L) {
                if (!gone) {
                    gone = coroutine.give(i);
                }
            }
        });
    }

    static long main() {
        final Coroutine<Long> c = count(5L);
        final Variant first = c.resume();
        c.close();
        if (c.done()) {
            return (Long) first.value() + 40L;
        }
        return 1L;
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
