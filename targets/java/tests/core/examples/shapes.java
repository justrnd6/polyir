final class shapes {
    record Rect(long width, long height) {
    }

    static long area(Rect r) {
        return r.width * r.height;
    }
}
