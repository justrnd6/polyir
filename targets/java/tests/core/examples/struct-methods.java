final class struct_methods {
    record Point(double x, double y) {
        public double dist() {
            return this.x * this.x + this.y * this.y;
        }
    }

    record Circle(double r) {
        public double dist() {
            return this.r * this.r;
        }
    }

    static double distSum(double px, double py, double r) {
        final Point p = new Point(px, py);
        final Circle c = new Circle(r);
        return p.dist() + c.dist();
    }

    record Pair(long a, long b) {
        public long sum() {
            return this.a + this.b;
        }

        public long doubledSum() {
            return this.sum() * 2L;
        }
    }

    static long doubledSumCheck(long a, long b) {
        final Pair p = new Pair(a, b);
        return p.doubledSum();
    }
}
