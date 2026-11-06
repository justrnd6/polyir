"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function queens(t, prefixes, p, stack) {
  const n = p[0];
  const depth = p[1];
  const full = Number(BigInt.asIntN(32, i64(i64(1n << BigInt(n)) - 1n)));
  const base = Math.imul(Math.imul(t, (n + 1) | 0), 4);
  stack[base] = prefixes[Math.imul(t, 3)];
  stack[(base + 1) | 0] = prefixes[(Math.imul(t, 3) + 1) | 0];
  stack[(base + 2) | 0] = prefixes[(Math.imul(t, 3) + 2) | 0];
  stack[(base + 3) | 0] = full & ~(prefixes[Math.imul(t, 3)] | prefixes[(Math.imul(t, 3) + 1) | 0] | prefixes[(Math.imul(t, 3) + 2) | 0]);
  let level = 0;
  let count = 0;
  for (let step = 0n; level >= 0; step = i64(step + 1n)) {
    const at = (base + Math.imul(level, 4)) | 0;
    const free = stack[(at + 3) | 0];
    if (free === 0) {
      level = (level - 1) | 0;
    } else {
      const bit = free & Number(BigInt.asIntN(32, i64(0n - BigInt(free))));
      stack[(at + 3) | 0] = free ^ bit;
      if (((((depth + level) | 0) + 1) | 0) === n) {
        count = (count + 1) | 0;
      } else {
        const nextCols = stack[at] | bit;
        const nextLeft = (stack[(at + 1) | 0] | bit) << 1 & full;
        const nextRight = (stack[(at + 2) | 0] | bit) >> 1;
        level = (level + 1) | 0;
        const below = (base + Math.imul(level, 4)) | 0;
        stack[below] = nextCols;
        stack[(below + 1) | 0] = nextLeft;
        stack[(below + 2) | 0] = nextRight;
        stack[(below + 3) | 0] = full & ~(nextCols | nextLeft | nextRight);
      }
    }
  }
  return count;
}

function listPrefixes(n, depth, out) {
  let total = 1n;
  let r = 0n;
  for (; r < depth; r = i64(r + 1n)) {
    total = i64(total * n);
  }
  const full = Number(BigInt.asIntN(32, i64(i64(1n << n) - 1n)));
  let found = 0n;
  for (let code = 0n; code < total; code = i64(code + 1n)) {
    let left = 0;
    let right = 0;
    let cols = 0;
    let ok = 1n;
    let rest = code;
    for (let r = 0n; r < depth; r = i64(r + 1n)) {
      const column = rest % n;
      rest = rest / n;
      const bit = Number(BigInt.asIntN(32, i64(1n << column)));
      if ((bit & (cols | left | right)) !== 0) {
        ok = 0n;
      }
      cols = cols | bit;
      left = (left | bit) << 1 & full;
      right = (right | bit) >> 1;
    }
    if (ok === 1n) {
      out[Number(i64(found * 3n))] = cols;
      out[Number(i64(i64(found * 3n) + 1n))] = left;
      out[Number(i64(i64(found * 3n) + 2n))] = right;
      found = i64(found + 1n);
    }
  }
  return found;
}

export { queens, listPrefixes };
