"use strict";

import { metalBuffer, metalDispatch, metalEmptyBuffer } from "./Metal.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function fftStagesGpu(device, stage, first, second, total, batch, direction) {
  let source = first;
  let target = second;
  const scratch = metalEmptyBuffer(device, i64(i64(batch * (total / 2n)) * 4n));
  let n = total;
  let s = 1n;
  for (let pass = 0n; pass < 30n; pass = i64(pass + 1n)) {
    if (n > 1n) {
      const settings = Array.from({ length: 4 }, () => 0);
      settings[0] = Number(BigInt.asIntN(32, total));
      settings[1] = Number(BigInt.asIntN(32, n));
      settings[2] = Number(BigInt.asIntN(32, s));
      settings[3] = Number(BigInt.asIntN(32, direction));
      if (metalDispatch(device, stage, i64(batch * (total / 2n)), source, metalBuffer(device, settings, 16n), target, scratch) === 0n) {
        return 0n;
      }
      const swap = source;
      source = target;
      target = swap;
      n = n / 2n;
      s = i64(s * 2n);
    }
  }
  return source;
}

export { fftStagesGpu };
