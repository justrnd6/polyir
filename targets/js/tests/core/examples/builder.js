"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

// Stops the program with a message, as polyir's `fail` does.
function fail(message) {
  console.error("trap: " + message);
  if (typeof process !== "undefined") {
    process.exit(1);
  }
  throw new Error(message);
}

// The value an optional holds; an empty one stops the program.
function present(value) {
  if (value === null) {
    fail("unwrap of an empty optional");
  }
  return value;
}

class Shape {
  constructor(side, name = "shape", tags) {
    this.side = side;
    this.name = name;
    this.tags = tags;
  }

  size() {
    return new Shape.Size(i64(this.side * this.side));
  }

  static square(side) {
    return Shape.builder().side(side).name("square").build();
  }

  static builder() {
    return new Shape.Builder(null, "shape", []);
  }

  static Size = class {
    constructor(area) {
      this.area = area;
    }
  };

  static Builder = class {
    constructor(__side, __name, __tags) {
      this.__side = __side;
      this.__name = __name;
      this.__tags = __tags;
    }

    side(value) {
      this.__side = value;
      return this;
    }

    name(value) {
      this.__name = value;
      return this;
    }

    tags(value) {
      this.__tags = value;
      return this;
    }

    build() {
      if (this.__side === null) {
        fail("Shape.build: side is not set");
      }
      return new Shape(present(this.__side), this.__name, this.__tags);
    }
  };
}

function tagged(b) {
  const tags = ["red", "big"];
  return b.tags(tags);
}

function main() {
  const first = tagged(Shape.builder()).side(2n).build();
  const second = Shape.square(3n);
  const size = second.size();
  console.log(`${first.name} ${first.side} ${first.size().area} [${first.tags[0]}, ${first.tags[1]}] ${second.name} ${second.side} ${size.area}`);
}

main();
