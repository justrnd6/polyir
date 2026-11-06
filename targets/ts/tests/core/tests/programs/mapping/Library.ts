declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

let visits: bigint = 0n;

class Account {
  constructor(public balance: bigint, public owner: string) {}

  deposit(amount: bigint): bigint {
    this.balance = i64(this.balance + amount);
    return this.balance;
  }

  depositTwice(amount: bigint): bigint {
    this.deposit(amount);
    return this.deposit(amount);
  }
}

class Vec2 {
  constructor(public x: number, public y: number) {}

  length2(): number {
    return this.x * this.x + this.y * this.y;
  }

  scaled(k: number): Vec2 {
    return new Vec2(this.x * k, this.y * k);
  }
}

function fib(n: bigint): bigint {
  return n < 2n ? n : i64(fib(i64(n - 1n)) + fib(i64(n - 2n)));
}

function classify(n: bigint): bigint {
  return n < 0n ? 1n : n === 0n ? 2n : n < 10n ? 3n : 4n;
}

function bump(): bigint {
  visits = i64(visits + 1n);
  return visits;
}

function greeting(name: string): string {
  return "hello, " + name;
}

function narrow(a: number, b: number): number {
  return (Math.imul(a, b) + 7) | 0;
}

function apply(f: (...args: [bigint]) => bigint, x: bigint): bigint {
  return i64(f(x) + f(i64(x + 1n)));
}

function main(): bigint {
  let score: bigint = 0n;
  score = i64(score + fib(10n));
  const acct: Account = new Account(100n, "ann");
  score = i64(score + acct.depositTwice(5n));
  const v: Vec2 = new Vec2(3.0, 4.0);
  const w: Vec2 = v.scaled(2.0);
  score = i64(score + BigInt(Math.trunc(w.length2())));
  if (greeting("bo") === "hello, bo") {
    score = i64(score + 1n);
  }
  score = i64(score + BigInt(narrow(6, 7)));
  const seen: Set<bigint> = new Set();
  for (let i = 0n; i < 20n; i = i64(i + 1n)) {
    seen.add(i % 7n);
  }
  score = i64(score + BigInt(seen.size));
  const nums: Array<bigint> = [];
  for (let i = 0n, j = 10n; i < 5n; i = i64(i + 1n), j = i64(j - 1n)) {
    nums.push(i64(i * j));
  }
  nums[0] = 100n;
  score = i64(i64(i64(score + nums[0]) + nums[4]) + BigInt(nums.length));
  const ages: Map<string, bigint> = new Map();
  ages.set("ann", 31n);
  if (ages.has("ann")) {
    score = i64(score + (ages.get("ann") ?? 0n));
  }
  if (ages.has("zed") === false) {
    score = i64(score + 1n);
  }
  const factor: bigint = 3n;
  score = i64(score + apply((x: bigint) => i64(x * factor), 4n));
  const b: Box<number> = new Box(2.5);
  score = i64(score + BigInt(Math.trunc(b.value * 4.0)));
  score = i64(i64(i64(i64(score + classify(i64(0n - 5n))) + classify(0n)) + classify(7n)) + classify(70n));
  score = i64(i64(score + bump()) + bump());
  return score;
}

class Box<T> {
  constructor(public value: T) {}
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
