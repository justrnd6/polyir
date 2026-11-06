"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

class Rect {
  constructor(width, height) {
    this.width = width;
    this.height = height;
  }
}

function area(r) {
  return i64(r.width * r.height);
}

export { Rect, area };
