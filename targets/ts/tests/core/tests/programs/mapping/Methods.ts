declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

class Acc {
  constructor(public total: bigint, public tag: string, public items: Array<bigint>) {}

  add(x: bigint): bigint {
    this.total = i64(this.total + x);
    this.items.push(x);
    return this.total;
  }

  later(): () => bigint {
    return () => i64(this.total * 2n);
  }

  describe(): string {
    return this.tag + ":" + this.tag;
  }

  self(): Acc {
    return this;
  }
}

interface Animal {
  legs(p0: bigint): bigint;
  name(): string;
}

class Dog {
  constructor(public age: bigint) {}

  legs(n: bigint): bigint {
    return i64(i64(4n * n) + this.age);
  }

  name(): string {
    return "dog";
  }
}

class Bird {
  constructor(public age: bigint) {}

  legs(n: bigint): bigint {
    return i64(2n * n);
  }

  name(): string {
    return "bird";
  }
}

function risky(x: bigint): bigint {
  if (x < 0n) {
    throw 7n;
  }
  return x;
}

function tryReturn(x: bigint): bigint {
  try {
    return i64(risky(x) + 1n);
  } catch (code: any) {
    return i64(code * 100n);
  }
  return 0n;
}

function nested(x: bigint): bigint {
  let r: bigint = 0n;
  try {
    try {
      r = risky(x);
    } catch (a: any) {
      r = i64(a + 1n);
      throw r;
    }
  } catch (b: any) {
    r = i64(b * 10n);
  }
  return r;
}

function makeAdder(n: bigint): (...args: [bigint]) => bigint {
  return (x: bigint) => i64(x + n);
}

function twice(f: (...args: [bigint]) => bigint, x: bigint): bigint {
  return f(f(x));
}

function counterPair(): bigint {
  let c: bigint = 0n;
  const inc: () => bigint = (): bigint => {
    c = i64(c + 1n);
    return c;
  };
  const get: () => bigint = () => c;
  inc();
  inc();
  return i64(i64(get() * 10n) + c);
}

function main(): bigint {
  let score: bigint = 0n;
  const a: Acc = new Acc(1n, "ab", []);
  score = i64(i64(score + a.add(5n)) + a.add(6n));
  const f: () => bigint = a.later();
  score = i64(score + f());
  score = i64(score + BigInt(a.items.length));
  if (a.describe() === "ab:ab") {
    score = i64(score + 1000n);
  }
  const s: Acc = a.self();
  if (s === a) {
    score = i64(score + 2000n);
  }
  a.tag = "zz";
  if (s.tag === "zz") {
    score = i64(score + 3000n);
  }
  const zoo: Array<Animal> = [];
  zoo.push(new Dog(3n));
  zoo.push(new Bird(1n));
  for (let i = 0n; i < BigInt(zoo.length); i = i64(i + 1n)) {
    score = i64(score + zoo[Number(i)].legs(2n));
    if (zoo[Number(i)].name() === "bird") {
      score = i64(score + 7n);
    }
  }
  score = i64(i64(score + tryReturn(3n)) + tryReturn(i64(0n - 1n)));
  score = i64(i64(score + nested(4n)) + nested(i64(0n - 2n)));
  const add3: (...args: [bigint]) => bigint = makeAdder(3n);
  score = i64(score + twice(add3, 10n));
  score = i64(score + counterPair());
  return score % 251n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
