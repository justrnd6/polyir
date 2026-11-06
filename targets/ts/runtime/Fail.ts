// Stops the program with a message, as polyir's `fail` does.
function fail(message: string): never {
  console.error("trap: " + message);
  if (typeof process !== "undefined") {
    process.exit(1);
  }
  throw new Error(message);
}
