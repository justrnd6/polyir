"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

let visits = 0n;

class Account {
  constructor(balance, owner) {
    this.balance = balance;
    this.owner = owner;
  }

  deposit(amount) {
    this.balance = i64(this.balance + amount);
    return this.balance;
  }

  depositTwice(amount) {
    this.deposit(amount);
    return this.deposit(amount);
  }
}

class Vec2 {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }

  length2() {
    return this.x * this.x + this.y * this.y;
  }

  scaled(k) {
    return new Vec2(this.x * k, this.y * k);
  }
}

function fib(n) {
  return n < 2n ? n : i64(fib(i64(n - 1n)) + fib(i64(n - 2n)));
}

function classify(n) {
  return n < 0n ? 1n : n === 0n ? 2n : n < 10n ? 3n : 4n;
}

function bump() {
  visits = i64(visits + 1n);
  return visits;
}

function greeting(name) {
  return "hello, " + name;
}

function narrow(a, b) {
  return (Math.imul(a, b) + 7) | 0;
}

function apply(f, x) {
  return i64(f(x) + f(i64(x + 1n)));
}

function main() {
  let score = 0n;
  score = i64(score + fib(10n));
  const acct = new Account(100n, "ann");
  score = i64(score + acct.depositTwice(5n));
  const v = new Vec2(3.0, 4.0);
  const w = v.scaled(2.0);
  score = i64(score + BigInt(Math.trunc(w.length2())));
  if (greeting("bo") === "hello, bo") {
    score = i64(score + 1n);
  }
  score = i64(score + BigInt(narrow(6, 7)));
  const seen = new Set();
  for (let i = 0n; i < 20n; i = i64(i + 1n)) {
    seen.add(i % 7n);
  }
  score = i64(score + BigInt(seen.size));
  const nums = [];
  for (let i = 0n, j = 10n; i < 5n; i = i64(i + 1n), j = i64(j - 1n)) {
    nums.push(i64(i * j));
  }
  nums[0] = 100n;
  score = i64(i64(i64(score + nums[0]) + nums[4]) + BigInt(nums.length));
  const ages = new Map();
  ages.set("ann", 31n);
  if (ages.has("ann")) {
    score = i64(score + (ages.get("ann") ?? 0n));
  }
  if (ages.has("zed") === false) {
    score = i64(score + 1n);
  }
  const factor = 3n;
  score = i64(score + apply((x) => i64(x * factor), 4n));
  const b = new Box(2.5);
  score = i64(score + BigInt(Math.trunc(b.value * 4.0)));
  score = i64(i64(i64(i64(score + classify(i64(0n - 5n))) + classify(0n)) + classify(7n)) + classify(70n));
  score = i64(i64(score + bump()) + bump());
  return score;
}

class Box {
  constructor(value) {
    this.value = value;
  }
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
