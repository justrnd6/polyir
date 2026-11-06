import java.util.ArrayList;

final class interfaces {
    interface Shape {
        long area();
        long scaled(long p0);
    }

    interface Named {
        long id();
    }

    interface Combiner {
        long combine(Shape p0);
    }

    record Square(long side) implements Shape, Named {
        public long area() {
            return this.side * this.side;
        }

        public long scaled(long k) {
            return this.side * k;
        }

        public long id() {
            return 1L;
        }
    }

    record Rect(long w, long h) implements Shape {
        public long area() {
            return this.w * this.h;
        }

        public long scaled(long k) {
            return this.w * k;
        }
    }

    record Summer(long base) implements Combiner {
        public long combine(Shape other) {
            return this.base + other.area();
        }
    }

    record Holder(Shape shape, long tag) {
    }

    static long totalArea(Shape s) {
        return s.area();
    }

    static long directCall() {
        return totalArea(new Square(3L)) * 100L + totalArea(new Rect(4L, 5L));
    }

    static Shape pick(long which) {
        if (which == 0L) {
            return new Square(6L);
        }
        return new Rect(2L, 7L);
    }

    static long chosen() {
        final Shape a = pick(0L);
        final Shape b = pick(1L);
        return a.area() * 1000L + b.area() + b.scaled(10L);
    }

    static long sumAll() {
        final ArrayList<Shape> shapes = new ArrayList<>();
        shapes.add(new Square(2L));
        shapes.add(new Rect(3L, 4L));
        shapes.add(new Square(5L));
        long total = 0L;
        for (long i = 0L; i < (long) shapes.size(); i = i + 1L) {
            total = total + shapes.get((int) i).area();
        }
        return total;
    }

    static long viaField() {
        final Holder h = new Holder(new Rect(5L, 6L), 7L);
        return h.shape.area() * 10L + h.tag;
    }

    static long twoInterfaces() {
        final Square sq = new Square(4L);
        final Named n = sq;
        final Shape s = sq;
        return n.id() * 1000L + s.area();
    }

    static long interfaceArgument() {
        final Combiner c = new Summer(100L);
        return c.combine(new Rect(3L, 3L));
    }

    static long main() {
        return directCall() + chosen() + sumAll() + viaField() + twoInterfaces() + interfaceArgument();
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
