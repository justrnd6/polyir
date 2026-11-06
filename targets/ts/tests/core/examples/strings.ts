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

// The byte at a position of a string, as a signed 8-bit number; 0 outside the string.
function textByte(text: string, at: bigint): number {
  const bytes = utf8Bytes(text);
  const i = Number(at);
  return i >= 0 && i < bytes.length ? bytes[i] << 24 >> 24 : 0;
}

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

// The bytes from `from` up to `to` of a string, both kept inside it.
function textSlice(text: string, from: bigint, to: bigint): string {
  const bytes = utf8Bytes(text);
  const start = Math.max(0, Math.min(Number(from), bytes.length));
  const end = Math.max(start, Math.min(Number(to), bytes.length));
  return utf8Text(bytes.slice(start, end));
}

// Where `part` first appears in a string at or after `from`, in bytes; -1 when it does not.
function textFind(text: string, part: string, from: bigint): bigint {
  const bytes = utf8Bytes(text);
  const wanted = utf8Bytes(part);
  for (let at = Math.max(0, Math.min(Number(from), bytes.length)); at + wanted.length <= bytes.length; at++) {
    if (wanted.every((byte, i) => bytes[at + i] === byte)) {
      return BigInt(at);
    }
  }
  return -1n;
}

function isSpaceChar(c: number): boolean {
  return c === 32 || c === 9 || c === 10 || c === 13;
}

function trim(s: string): string {
  let from: bigint = 0n;
  while (from < textLength(s) && isSpaceChar(textByte(s, from))) {
    from = i64(from + 1n);
  }
  let to: bigint = textLength(s);
  while (to > from && isSpaceChar(textByte(s, i64(to - 1n)))) {
    to = i64(to - 1n);
  }
  return textSlice(s, from, to);
}

function main(): bigint {
  console.log("hello from polyir");
  const name: string = "café";
  const n: bigint = 3n;
  console.log(`${name} has ${String(textLength(name))} bytes; ${String(n)} + 1 = ${String(i64(n + 1n))}`);
  console.log("a tab:\tand a quote: \"done\"");
  console.log(trim("a text block" + "\n" + "  keeps its inner indent" + "\n"));
  console.log(textSlice(name, 0n, 3n));
  console.log(String(textFind(name, "é", 0n)));
  console.log(String(textFind(name, "tea", 0n)));
  console.log(String(i64(textLength(name) * 10n)));
  console.log(String(name === "café"));
  const first: number = textByte(name, 0n);
  console.log(String.fromCharCode(first & 255));
  console.log(String.fromCharCode(first & 255));
  return i64(textLength(name) + i64(textFind(name, "é", 0n) * 10n));
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
