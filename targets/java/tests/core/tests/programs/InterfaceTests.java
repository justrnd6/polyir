import java.util.ArrayList;
import java.util.Objects;

final class InterfaceTests {
    static double checkConversionAtACall() {
        return Check.offInt(interfaces.directCall(), 920L);
    }

    static double checkConversionAtAReturn() {
        return Check.offInt(interfaces.chosen(), 36034L);
    }

    static double checkDarrayOfInterfaceValues() {
        return Check.offInt(interfaces.sumAll(), 41L);
    }

    static double checkInterfaceAsAStructField() {
        return Check.offInt(interfaces.viaField(), 307L);
    }

    static double checkOneStructTwoInterfaces() {
        return Check.offInt(interfaces.twoInterfaces(), 1016L);
    }

    static double checkInterfaceMethodTakingAnInterface() {
        return Check.offInt(interfaces.interfaceArgument(), 109L);
    }

    static double checkDispatchFollowsTheValue() {
        final interfaces.Shape a = new interfaces.Square(5L);
        final interfaces.Shape b = new interfaces.Rect(5L, 2L);
        return Check.offInt(a.area(), 25L) + Check.offInt(b.area(), 10L);
    }

    static double checkSameMethodNameOnTwoStructs() {
        final interfaces.Square s = new interfaces.Square(3L);
        final interfaces.Rect r = new interfaces.Rect(3L, 4L);
        return Check.offInt(s.area() + r.area(), 21L);
    }

    static double checkConvertedValueIsACopy() {
        final interfaces.Square s = new interfaces.Square(4L);
        final interfaces.Shape a = s;
        final interfaces.Shape b = s;
        return Check.offInt(a.area() + b.area(), 32L);
    }

    interface Scaler {
        float half(float p0);
        long offset(int p0);
    }

    record Halver(long base) implements Scaler {
        public float half(float x) {
            return x / 2.0f;
        }

        public long offset(int by) {
            return this.base + (long) by;
        }
    }

    static double checkArgumentWidthsFollowTheInterface() {
        final Scaler s = new Halver(40L);
        return Check.off(s.half(9.0f), 4.5f) + Check.offInt(s.offset(2), 42L);
    }

    static double checkInterfaceInAnEither() {
        final Either<interfaces.Shape, Long> r = Either.<interfaces.Shape, Long>ok(new interfaces.Rect(4L, 4L));
        if (!r.isOk) {
            return 1.0;
        }
        final interfaces.Shape s = r.okValue();
        return Check.offInt(s.area(), 16L);
    }

    static double checkInterfaceCapturedByAClosure() {
        final interfaces.Shape s = new interfaces.Square(7L);
        final Fn0<Long> f = () -> s.area();
        return Check.offInt(f.call(), 49L);
    }

    static double checkInterfaceThroughAVar() {
        interfaces.Shape s = new interfaces.Square(2L);
        final long first = s.area();
        s = new interfaces.Rect(3L, 5L);
        return Check.offInt(first * 100L + s.area(), 415L);
    }

    interface Weighted {
        long weight();
    }

    static long totalWeight(Weighted a, Weighted b) {
        return a.weight() + b.weight();
    }

    static double checkGenericStructConvertsToAnInterface() {
        final Cell<Long> a = new Cell<>(5L);
        final Cell<Double> b = new Cell<>(2.5);
        return Check.offInt(totalWeight(a, b), 14L) + Check.offInt(a.get(), 5L) + Check.off((float) (double) b.get(), 2.5f);
    }

    static double checkGenericInterfaceValuesInADarray() {
        final ArrayList<Weighted> all = new ArrayList<>();
        final Cell<Long> a = new Cell<>(1L);
        all.add(a);
        all.add(a);
        return Check.offInt(all.get(0).weight() + all.get(1).weight(), 14L);
    }

    record Cell<T>(T value) implements Weighted {
        public long weight() {
            return 7L;
        }

        public T get() {
            return this.value;
        }
    }

    // An `either`: an ok value or an error, and which one it holds.
    static final class Either<T, E> {
        final boolean isOk;
        private final Object held;

        private Either(boolean isOk, Object held) {
            this.isOk = isOk;
            this.held = held;
        }

        static <T, E> Either<T, E> ok(T value) {
            return new Either<>(true, value);
        }

        static <T, E> Either<T, E> err(E value) {
            return new Either<>(false, value);
        }

        @SuppressWarnings("unchecked")
        T okValue() {
            return (T) held;
        }

        @SuppressWarnings("unchecked")
        E errValue() {
            return (E) held;
        }

        @Override
        public boolean equals(Object other) {
            return other instanceof Either<?, ?> that && isOk == that.isOk && Objects.equals(held, that.held);
        }

        @Override
        public int hashCode() {
            return Objects.hash(isOk, held);
        }
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
