// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

class Rect {
  constructor(public width: bigint, public height: bigint) {}
}

function area(r: Rect): bigint {
  return i64(r.width * r.height);
}

export { Rect, area };
