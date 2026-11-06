// An operator lane by lane: on two vectors, or on a vector and one number.
function lanes(a, b, op) {
  return a.map((x, i) => op(x, Array.isArray(b) ? b[i] : b));
}
