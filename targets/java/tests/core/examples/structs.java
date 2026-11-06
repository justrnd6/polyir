final class structs {
    record Point(double x, double y) {
    }

    static double distanceSquared(Point a, Point b) {
        final double dx = a.x - b.x;
        final double dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    static double distFromOrigin(double bx, double by) {
        return distanceSquared(new Point(0.0, 0.0), new Point(bx, by));
    }
}
