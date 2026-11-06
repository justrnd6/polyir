// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function dayKind(day: bigint): bigint {
  let label: bigint = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        if (day === 0n) {
          label = 1n;
          continue machine;
        } else if (day === 6n) {
          label = 1n;
          continue machine;
        } else if (day === 1n) {
          label = 2n;
          continue machine;
        } else if (day === 2n) {
          label = 2n;
          continue machine;
        } else if (day === 3n) {
          label = 2n;
          continue machine;
        } else if (day === 4n) {
          label = 2n;
          continue machine;
        } else if (day === 5n) {
          label = 2n;
          continue machine;
        } else {
          label = 3n;
          continue machine;
        }
      }
      case 1n: {
        return 0n;
      }
      case 2n: {
        return 1n;
      }
      case 3n: {
        return i64(0n - 1n);
      }
    }
  }
}

export { dayKind };
