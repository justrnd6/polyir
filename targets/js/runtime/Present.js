// The value an optional holds; an empty one stops the program.
function present(value) {
  if (value === null) {
    fail("unwrap of an empty optional");
  }
  return value;
}
