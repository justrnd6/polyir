"use strict";

function gcd(a0, b0) {
  let label = 0n;
  let a = 0n;
  let b = 0n;
  let result = 0n;
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
        const r = a % b;
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
