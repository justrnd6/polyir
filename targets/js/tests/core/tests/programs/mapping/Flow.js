"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function sumTo(n) {
  let label = 0n;
  let total = 0n;
  let i = 0n;
  let result = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        total = 0n;
        i = 1n;
        label = 1n;
        continue machine;
      }
      case 1n: {
        if (i > n) {
          result = total;
          label = 3n;
          continue machine;
        } else {
          label = 2n;
          continue machine;
        }
      }
      case 2n: {
        total = i64(total + i);
        i = i64(i + 1n);
        label = 1n;
        continue machine;
      }
      case 3n: {
        return result;
      }
    }
  }
}

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

function collatz(n) {
  let label = 0n;
  let x = 0n;
  let steps = 0n;
  let count = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        x = n;
        steps = 0n;
        label = 1n;
        continue machine;
      }
      case 1n: {
        if (x === 1n) {
          count = steps;
          label = 5n;
          continue machine;
        } else {
          label = 2n;
          continue machine;
        }
      }
      case 2n: {
        if (x % 2n === 0n) {
          label = 3n;
          continue machine;
        } else {
          label = 4n;
          continue machine;
        }
      }
      case 3n: {
        x = x / 2n;
        steps = i64(steps + 1n);
        label = 1n;
        continue machine;
      }
      case 4n: {
        x = i64(i64(3n * x) + 1n);
        steps = i64(steps + 1n);
        label = 1n;
        continue machine;
      }
      case 5n: {
        return count;
      }
    }
  }
}

function swapped(a, b) {
  let label = 0n;
  let x = 0n;
  let y = 0n;
  let k = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        x = a;
        y = b;
        k = 0n;
        label = 1n;
        continue machine;
      }
      case 1n: {
        if (k === 3n) {
          label = 3n;
          continue machine;
        } else {
          label = 2n;
          continue machine;
        }
      }
      case 2n: {
        let next_x = y;
        let next_y = x;
        let next_k = i64(k + 1n);
        x = next_x;
        y = next_y;
        k = next_k;
        label = 1n;
        continue machine;
      }
      case 3n: {
        return i64(i64(x * 10n) + y);
      }
    }
  }
}

function weight(day) {
  let label = 0n;
  let w = 0n;
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
        } else {
          label = 3n;
          continue machine;
        }
      }
      case 1n: {
        w = 1n;
        label = 4n;
        continue machine;
      }
      case 2n: {
        w = 2n;
        label = 4n;
        continue machine;
      }
      case 3n: {
        w = 3n;
        label = 4n;
        continue machine;
      }
      case 4n: {
        return i64(w * 100n);
      }
    }
  }
}

function zigzag(a) {
  let label = 0n;
  let p = 0n;
  let q = 0n;
  let r = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        if (a === 0n) {
          p = a;
          label = 1n;
          continue machine;
        } else {
          q = a;
          label = 2n;
          continue machine;
        }
      }
      case 1n: {
        const next = i64(p + 1n);
        if (next > 5n) {
          r = next;
          label = 3n;
          continue machine;
        } else {
          q = next;
          label = 2n;
          continue machine;
        }
      }
      case 2n: {
        if (q > 5n) {
          r = q;
          label = 3n;
          continue machine;
        } else {
          p = i64(q + 1n);
          label = 1n;
          continue machine;
        }
      }
      case 3n: {
        return i64(r * 2n);
      }
    }
  }
}

function main() {
  let score = sumTo(10n);
  score = i64(score + gcd(48n, 18n));
  score = i64(score + collatz(6n));
  score = i64(score + swapped(4n, 7n));
  score = i64(i64(i64(score + weight(0n)) + weight(2n)) + weight(9n));
  score = i64(i64(score + zigzag(0n)) + zigzag(3n));
  return score;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
