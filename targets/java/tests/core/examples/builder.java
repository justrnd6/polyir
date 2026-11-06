import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

final class builder {
    record Shape(long side, String name, ArrayList<String> tags) {
        public Size size() {
            return new Size(this.side * this.side);
        }

        static Shape square(long side) {
            return builder().side(side).name("square").build();
        }

        static Builder builder() {
            return new Builder(null, "shape", new ArrayList<>());
        }

        record Size(long area) {
        }

        static final class Builder {
            Long __side;
            String __name;
            ArrayList<String> __tags;

            Builder(Long __side, String __name, ArrayList<String> __tags) {
                this.__side = __side;
                this.__name = __name;
                this.__tags = __tags;
            }

            public Builder side(long value) {
                this.__side = value;
                return this;
            }

            public Builder name(String value) {
                this.__name = value;
                return this;
            }

            public Builder tags(ArrayList<String> value) {
                this.__tags = value;
                return this;
            }

            public Shape build() {
                if (this.__side == null) {
                    fail("Shape.build: side is not set");
                }
                return new Shape(present(this.__side), this.__name, this.__tags);
            }

            Builder copy() {
                return new Builder(__side, __name, __tags);
            }

            @Override
            public boolean equals(Object other) {
                return other instanceof Builder that && Arrays.equals(new Object[] {__side, __name, __tags}, new Object[] {that.__side, that.__name, that.__tags});
            }

            @Override
            public int hashCode() {
                return Objects.hash(__side, __name, __tags);
            }
        }
    }

    static Shape.Builder tagged(Shape.Builder b) {
        final ArrayList<String> tags = new ArrayList<>(List.of("red", "big"));
        return b.tags(tags);
    }

    static void main() {
        final Shape first = tagged(Shape.builder()).side(2L).build();
        final Shape second = Shape.square(3L);
        final Shape.Size size = second.size();
        System.out.println(first.name + " " + String.valueOf(first.side) + " " + String.valueOf(first.size().area) + " [" + first.tags.get(0) + ", " + first.tags.get(1) + "] " + second.name + " " + String.valueOf(second.side) + " " + String.valueOf(size.area));
    }

    public static void main(String[] args) {
        main();
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
}
