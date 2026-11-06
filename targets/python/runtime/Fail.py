# Stops the program with a message, as polyir's `fail` does.
def fail(message: str):
    print("trap: " + message, file=sys.stderr)
    sys.exit(1)
