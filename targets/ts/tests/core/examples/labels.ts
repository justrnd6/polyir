function gcd(a0: bigint, b0: bigint): bigint {
  let label: bigint = 0n;
  let a: bigint = 0n;
  let b: bigint = 0n;
  let result: bigint = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        a = a0;
        b = b0;
        label = 1n;
        continue machine;
      }
      case 1n: {
        if (b === 0n) {
          result = a;
          label = 3n;
          continue machine;
        } else {
          label = 2n;
          continue machine;
        }
      }
      case 2n: {
        const r: bigint = a % b;
        a = b;
        b = r;
        label = 1n;
        continue machine;
      }
      case 3n: {
        return result;
      }
    }
  }
}

export { gcd };
