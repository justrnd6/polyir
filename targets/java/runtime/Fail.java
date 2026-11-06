// Stops the program with a message, as polyir's `fail` does.
static <T> T fail(String message) {
    System.err.println("trap: " + message);
    System.exit(1);
    return null;
}
