// Stops the program with a message, as polyir's `fail` does.
fileprivate func fail(_ message: String) -> Never {
    FileHandle.standardError.write(("trap: " + message + "\n").data(using: .utf8)!)
    exit(1)
}
