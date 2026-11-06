// An operator lane by lane: on two vectors, or on a vector and one number.
function lanes<T>(a: T[], b: T[] | T, op: (x: T, y: T) => T): T[] {
  return a.map((x, i) => op(x, Array.isArray(b) ? b[i] : b));
}
