import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;

final class Library {
    static long visits = 0L;

    static final class Account {
        long balance;
        final String owner;

        Account(long balance, String owner) {
            this.balance = balance;
            this.owner = owner;
        }

        public long deposit(long amount) {
            this.balance = this.balance + amount;
            return this.balance;
        }

        public long depositTwice(long amount) {
            this.deposit(amount);
            return this.deposit(amount);
        }

        Account copy() {
            return new Account(balance, owner);
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Account that && Arrays.equals(new Object[] {balance, owner}, new Object[] {that.balance, that.owner});
        }

        @Override
        public int hashCode() {
            return Objects.hash(balance, owner);
        }
    }

    record Vec2(double x, double y) {
        public double length2() {
            return this.x * this.x + this.y * this.y;
        }

        public Vec2 scaled(double k) {
            return new Vec2(this.x * k, this.y * k);
        }
    }

    static long fib(long n) {
        return n < 2L ? n : fib(n - 1L) + fib(n - 2L);
    }

    static long classify(long n) {
        return n < 0L ? 1L : n == 0L ? 2L : n < 10L ? 3L : 4L;
    }

    static long bump() {
        visits = visits + 1L;
        return visits;
    }

    static String greeting(String name) {
        return "hello, " + name;
    }

    static int narrow(int a, int b) {
        return a * b + 7;
    }

    static long apply(Fn1<Long, Long> f, long x) {
        return f.call(x) + f.call(x + 1L);
    }

    static long main() {
        long score = 0L;
        score = score + fib(10L);
        final Account acct = new Account(100L, "ann");
        score = score + acct.depositTwice(5L);
        final Vec2 v = new Vec2(3.0, 4.0);
        final Vec2 w = v.scaled(2.0);
        score = score + (long) (double) w.length2();
        if (Objects.equals(greeting("bo"), "hello, bo")) {
            score = score + 1L;
        }
        score = score + (long) narrow(6, 7);
        final HashSet<Long> seen = new HashSet<>();
        for (long i = 0L; i < 20L; i = i + 1L) {
            seen.add(i % 7L);
        }
        score = score + (long) seen.size();
        final ArrayList<Long> nums = new ArrayList<>();
        {
            long i = 0L;
            long j = 10L;
            while (i < 5L) {
                nums.add(i * j);
                i = i + 1L;
                j = j - 1L;
            }
        }
        nums.set(0, 100L);
        score = score + nums.get(0) + nums.get(4) + (long) nums.size();
        final HashMap<String, Long> ages = new HashMap<>();
        ages.put("ann", 31L);
        if (ages.containsKey("ann")) {
            score = score + ages.getOrDefault("ann", 0L);
        }
        if (ages.containsKey("zed") == false) {
            score = score + 1L;
        }
        final long factor = 3L;
        score = score + apply((x) -> x * factor, 4L);
        final Box<Double> b = new Box<>(2.5);
        score = score + (long) (double) (b.value * 4.0);
        score = score + classify(0L - 5L) + classify(0L) + classify(7L) + classify(70L);
        score = score + bump() + bump();
        return score;
    }

    record Box<T>(T value) {
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
}
