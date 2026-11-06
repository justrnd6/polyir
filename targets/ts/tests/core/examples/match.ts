declare const process: any;
export {};

// A string's UTF-8 bytes, and bytes back to a string, as polyir counts a string's positions in bytes. An unpaired
// surrogate is U+FFFD, and so is each byte sequence that is not UTF-8.
function utf8Bytes(text: string): number[] {
  const bytes: number[] = [];
  for (let i = 0; i < text.length; i++) {
    let c = text.charCodeAt(i);
    const low = i + 1 < text.length ? text.charCodeAt(i + 1) : 0;
    if (c >= 0xD800 && c < 0xDC00 && low >= 0xDC00 && low < 0xE000) {
      c = 0x10000 + (c - 0xD800) * 1024 + (low - 0xDC00);
      i++;
    } else if (c >= 0xD800 && c < 0xE000) {
      c = 0xFFFD;
    }
    if (c < 0x80) {
      bytes.push(c);
    } else if (c < 0x800) {
      bytes.push(0xC0 | c >> 6, 0x80 | c & 63);
    } else if (c < 0x10000) {
      bytes.push(0xE0 | c >> 12, 0x80 | c >> 6 & 63, 0x80 | c & 63);
    } else {
      bytes.push(0xF0 | c >> 18, 0x80 | c >> 12 & 63, 0x80 | c >> 6 & 63, 0x80 | c & 63);
    }
  }
  return bytes;
}

function utf8Text(bytes: number[]): string {
  let text = "";
  for (let i = 0; i < bytes.length;) {
    const lead = bytes[i];
    const size = lead < 0x80 ? 1 : lead >= 0xC2 && lead < 0xE0 ? 2 : lead >= 0xE0 && lead < 0xF0 ? 3 : lead >= 0xF0 && lead < 0xF5 ? 4 : 0;
    const low = lead === 0xE0 ? 0xA0 : lead === 0xF0 ? 0x90 : 0x80;
    const high = lead === 0xED ? 0x9F : lead === 0xF4 ? 0x8F : 0xBF;
    let c = size === 1 ? lead : lead & (0xFF >> (size + 1));
    let k = 1;
    while (k < size && i + k < bytes.length && bytes[i + k] >= (k === 1 ? low : 0x80) && bytes[i + k] <= (k === 1 ? high : 0xBF)) {
      c = c << 6 | bytes[i + k] & 63;
      k++;
    }
    text += size > 0 && k === size ? String.fromCodePoint(c) : "\uFFFD";
    i += size > 0 ? k : 1;
  }
  return text;
}

// How many UTF-8 bytes a string has.
function textLength(text: string): bigint {
  return BigInt(utf8Bytes(text).length);
}

// The bytes from `from` up to `to` of a string, both kept inside it.
function textSlice(text: string, from: bigint, to: bigint): string {
  const bytes = utf8Bytes(text);
  const start = Math.max(0, Math.min(Number(from), bytes.length));
  const end = Math.max(start, Math.min(Number(to), bytes.length));
  return utf8Text(bytes.slice(start, end));
}

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

// An `either`: an ok value or an error, and which one it holds.
class Either<T, E> {
  private constructor(readonly isOk: boolean, private readonly held: T | E) {}

  static ok<T, E>(value: T): Either<T, E> {
    return new Either<T, E>(true, value);
  }

  static err<T, E>(value: E): Either<T, E> {
    return new Either<T, E>(false, value);
  }

  get okValue(): T {
    return this.held as T;
  }

  get errValue(): E {
    return this.held as E;
  }
}

function startsWith(s: string, prefix: string): boolean {
  if (textLength(prefix) > textLength(s)) {
    return false;
  }
  return textSlice(s, 0n, textLength(prefix)) === prefix;
}

function endsWith(s: string, suffix: string): boolean {
  if (textLength(suffix) > textLength(s)) {
    return false;
  }
  return textSlice(s, i64(textLength(s) - textLength(suffix)), textLength(s)) === suffix;
}

function sizeOf(n: bigint): string {
  let matchValue_10_10: string = "";
  switch (n) {
    case 0n: {
      matchValue_10_10 = "none";
      break;
    }
    case 1n: case 2n: case 3n: {
      matchValue_10_10 = "a few";
      break;
    }
    default: {
      matchValue_10_10 = "many";
    }
  }
  return matchValue_10_10;
}

function gradeOf(score: bigint): string {
  let matchValue_19_10: string = "";
  if (score >= 90n) {
    matchValue_19_10 = "A";
  } else if (score >= 80n) {
    matchValue_19_10 = "B";
  } else {
    matchValue_19_10 = "C";
  }
  return matchValue_19_10;
}

function kindOf(path: string): string {
  let matchValue_28_10: string = "";
  if (path === "Ninafile" || path === "Polyfile") {
    matchValue_28_10 = "build";
  } else if (endsWith(path, ".poly")) {
    matchValue_28_10 = "polyir";
  } else if (startsWith(path, "README")) {
    matchValue_28_10 = "docs";
  } else {
    matchValue_28_10 = "other";
  }
  return matchValue_28_10;
}

function shown(found: bigint | null): string {
  let matchValue_38_10: string = "";
  if (found !== null && present(found) > 100n) {
    const n: bigint = present(found);
    matchValue_38_10 = `big ${n}`;
  } else if (found !== null) {
    const n: bigint = present(found);
    matchValue_38_10 = `found ${n}`;
  } else {
    matchValue_38_10 = "nothing";
  }
  return matchValue_38_10;
}

function safeDivide(a: bigint, b: bigint): Either<bigint, string> {
  if (b === 0n) {
    return Either.err<bigint, string>("divide by zero");
  }
  return Either.ok<bigint, string>(a / b);
}

function quotient(a: bigint, b: bigint): string {
  const match_54_10: Either<bigint, string> = safeDivide(a, b);
  let matchValue_54_10: string = "";
  if (match_54_10.isOk) {
    const q: bigint = match_54_10.okValue;
    matchValue_54_10 = `quotient ${q}`;
  } else if (!match_54_10.isOk) {
    const why: string = match_54_10.errValue;
    matchValue_54_10 = why;
  }
  return matchValue_54_10;
}

function said(xs: Array<bigint>): string {
  let matchValue_62_10: string = "";
  if (BigInt(xs.length) === 0n) {
    matchValue_62_10 = "empty";
  } else if (BigInt(xs.length) === 1n) {
    const x: bigint = xs[0];
    matchValue_62_10 = `just ${x}`;
  } else {
    const first: bigint = xs[0];
    const rest: Array<bigint> = drop(xs, 1n);
    matchValue_62_10 = `${first}, then ${BigInt(rest.length)} more`;
  }
  return matchValue_62_10;
}

function report(code: bigint): void {
  if (code === 200n || code === 204n) {
    console.log("done");
  } else if (code >= 500n) {
    console.log(`retry ${code}`);
  } else if (code >= 400n) {
    const s: bigint = code;
    console.log(`failed with ${s}`);
  } else {
    console.log(`? ${code}`);
  }
}

function main(): bigint {
  console.log(sizeOf(2n));
  console.log(gradeOf(85n));
  console.log(kindOf("main.poly"));
  console.log(shown(500n));
  console.log(quotient(7n, 2n));
  console.log(said([3n, 4n, 5n]));
  report(404n);
  return 0n;
}

function drop<T>(xs: Array<T>, n: bigint): Array<T> {
  const out: Array<T> = [];
  for (let i = n; i < BigInt(xs.length); i = i64(i + 1n)) {
    out.push(xs[Number(i)]);
  }
  return out;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
