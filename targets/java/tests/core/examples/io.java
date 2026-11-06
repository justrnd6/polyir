final class io {
    static int putchar(int c) {
        System.out.write((int) c);
        System.out.flush();
        return c;
    }

    static int getchar() {
        try {
            return (int) System.in.read();
        } catch (java.io.IOException failed) {
            return -1;
        }
    }

    static int writeLine() {
        putchar(104);
        putchar(108);
        putchar(108);
        putchar(118);
        putchar(109);
        putchar(32);
        putchar(105);
        putchar(111);
        return putchar(10);
    }

    static int echoOneByte() {
        final int c = getchar();
        return putchar(c);
    }

    static long main() {
        writeLine();
        echoOneByte();
        return 0L;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
