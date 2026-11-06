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

class Shape {
  constructor(public side: bigint, public name: string = "shape", public tags: Array<string>) {}

  size(): Shape.Size {
    return new Shape.Size(i64(this.side * this.side));
  }

  static square(side: bigint): Shape {
    return Shape.builder().side(side).name("square").build();
  }

  static builder(): Shape.Builder {
    return new Shape.Builder(null, "shape", []);
  }
}

namespace Shape {
  export class Size {
    constructor(public area: bigint) {}
  }

  export class Builder {
    constructor(public __side: bigint | null, public __name: string, public __tags: Array<string>) {}

    side(value: bigint): Shape.Builder {
      this.__side = value;
      return this;
    }

    name(value: string): Shape.Builder {
      this.__name = value;
      return this;
    }

    tags(value: Array<string>): Shape.Builder {
      this.__tags = value;
      return this;
    }

    build(): Shape {
      if (this.__side === null) {
        fail("Shape.build: side is not set");
      }
      return new Shape(present(this.__side), this.__name, this.__tags);
    }
  }
}

function tagged(b: Shape.Builder): Shape.Builder {
  const tags: Array<string> = ["red", "big"];
  return b.tags(tags);
}

function main(): void {
  const first: Shape = tagged(Shape.builder()).side(2n).build();
  const second: Shape = Shape.square(3n);
  const size: Shape.Size = second.size();
  console.log(`${first.name} ${first.side} ${first.size().area} [${first.tags[0]}, ${first.tags[1]}] ${second.name} ${second.side} ${size.area}`);
}

main();
