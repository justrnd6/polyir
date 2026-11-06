import java.util.ArrayList;
import java.util.List;

final class modules {
    static final class Geometry {
        private Geometry() {}

        record Point(long x, long y) {
            public long manhattan() {
                return this.absolute(this.x) + this.absolute(this.y);
            }

            public long absolute(long n) {
                return n < 0L ? 0L - n : n;
            }
        }

        static long clamp(long n, long low, long high) {
            return n < low ? low : n > high ? high : n;
        }

        static long distance(Point a, Point b) {
            final Point step = new Point(a.x - b.x, a.y - b.y);
            return clamp(step.manhattan(), 0L, 100L);
        }

        static final class Shapes {
            private Shapes() {}

            interface Shape {
                long area();
            }

            record Square(long side) implements Shape {
                public long area() {
                    return this.side * this.side;
                }
            }

            record Rect(long w, long h) implements Shape {
                public long area() {
                    return this.w * this.h;
                }
            }

            static long total(ArrayList<Shape> shapes) {
                long sum = 0L;
                for (var s : shapes) {
                    sum = sum + s.area();
                }
                return sum;
            }
        }
    }

    static void main() {
        final Geometry.Point a = new Geometry.Point(1L, 2L);
        final Geometry.Point b = new Geometry.Point(4L, -2L);
        final ArrayList<Geometry.Shapes.Shape> shapes = new ArrayList<>(List.of(new Geometry.Shapes.Square(3L), new Geometry.Shapes.Rect(2L, 5L)));
        final String size = Geometry.Shapes.total(shapes) > 10L ? "big" : "small";
        System.out.println(String.valueOf(Geometry.distance(a, b)) + " " + size + " " + String.valueOf(larger(3L, 7L, false)) + " " + larger("x", "y", true) + " " + firstOf(new ArrayList<>(List.of("first", "second"))));
    }

    static <T> T larger(T a, T b, boolean aFirst) {
        return aFirst ? a : b;
    }

    static <T> T firstOf(ArrayList<T> xs) {
        return xs.get(0);
    }

    public static void main(String[] args) {
        main();
    }
}
