declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

// Stops the program with a message, as polyir's `fail` does.
function fail(message: string): never {
  console.error("trap: " + message);
  if (typeof process !== "undefined") {
    process.exit(1);
  }
  throw new Error(message);
}

// The value an optional holds; an empty one stops the program.
function present<T>(value: T | null): T {
  if (value === null) {
    fail("unwrap of an empty optional");
  }
  return value;
}

class Pt {
  constructor(public x: bigint, public y: bigint) {}
}

class Contact {
  constructor(public name: string, public nickname: string | null) {}
}

function find(xs: Array<bigint>, want: bigint): bigint | null {
  for (let i = 0n; i < BigInt(xs.length); i = i64(i + 1n)) {
    if (xs[Number(i)] === want) {
      return i;
    }
  }
  return null;
}

function firstPoint(ok: bigint): Pt | null {
  if (ok === 1n) {
    return new Pt(7n, 8n);
  }
  return null;
}

function shown(c: Contact): string {
  if (c.nickname !== null) {
    return present(c.nickname);
  }
  return c.name;
}

function main(): bigint {
  const xs: Array<bigint> = [];
  xs.push(10n);
  xs.push(20n);
  let score: bigint = 0n;
  const hit: bigint | null = find(xs, 20n);
  if (hit !== null) {
    score = i64(score + present(hit));
  }
  if (find(xs, 99n) === null) {
    score = i64(score + 10n);
  }
  const p: Pt | null = firstPoint(1n);
  if (p !== null) {
    score = i64(score + present(p).x);
  }
  if (firstPoint(0n) === null) {
    score = i64(score + 100n);
  }
  const named: Contact = new Contact("ann", "annie");
  const plain: Contact = new Contact("bo", null);
  if (shown(named) === "annie") {
    score = i64(score + 1000n);
  }
  if (shown(plain) === "bo") {
    score = i64(score + 2000n);
  }
  return score;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
