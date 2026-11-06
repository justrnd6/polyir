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

// The byte at a position of a string, as a signed 8-bit number; 0 outside the string.
function textByte(text: string, at: bigint): number {
  const bytes = utf8Bytes(text);
  const i = Number(at);
  return i >= 0 && i < bytes.length ? bytes[i] << 24 >> 24 : 0;
}

// Shortens a list to at most `count` items, and gives it back.
function truncate<T>(items: T[], count: bigint): T[] {
  items.length = Math.min(items.length, Number(count));
  return items;
}

function contains(s: string, part: string): boolean {
  return textFind(s, part, 0n) >= 0n;
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

function split(s: string, sep: string): Array<string> {
  const parts: Array<string> = [];
  if (textLength(sep) === 0n) {
    parts.push(s);
    return parts;
  }
  let from: bigint = 0n;
  for (let at = textFind(s, sep, 0n); at >= 0n; at = textFind(s, sep, from)) {
    parts.push(textSlice(s, from, at));
    from = i64(at + textLength(sep));
  }
  parts.push(textSlice(s, from, textLength(s)));
  return parts;
}

function lines(s: string): Array<string> {
  return split(s, "\n");
}

function join(parts: Array<string>, sep: string): string {
  let out: string = "";
  for (let i = 0n; i < BigInt(parts.length); i = i64(i + 1n)) {
    if (i > 0n) {
      out = out + sep;
    }
    out = out + parts[Number(i)];
  }
  return out;
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

function replace(s: string, old: string, by: string): string {
  return join(split(s, old), by);
}

function repeat(s: string, n: bigint): string {
  let out: string = "";
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    out = out + s;
  }
  return out;
}

function indent(s: string, n: bigint): string {
  const all: Array<string> = lines(s);
  const pad: string = repeat(" ", n);
  for (let i = 0n; i < BigInt(all.length); i = i64(i + 1n)) {
    if (textLength(all[Number(i)]) > 0n) {
      all[Number(i)] = pad + all[Number(i)];
    }
  }
  return join(all, "\n");
}

function indentRest(s: string, n: bigint): string {
  const first: bigint = textFind(s, "\n", 0n);
  if (first < 0n) {
    return s;
  }
  return textSlice(s, 0n, i64(first + 1n)) + indent(textSlice(s, i64(first + 1n), textLength(s)), n);
}

function fill(template: string, values: Map<string, string>): string {
  let out: string = "";
  let from: bigint = 0n;
  for (let at = textFind(template, "${", 0n); at >= 0n; at = textFind(template, "${", from)) {
    const close: bigint = textFind(template, "}", at);
    if (close < 0n) {
      return out + textSlice(template, from, textLength(template));
    }
    out = out + textSlice(template, from, at) + filledName(textSlice(template, at, i64(close + 1n)), values);
    from = i64(close + 1n);
  }
  return out + textSlice(template, from, textLength(template));
}

function filledName(placeholder: string, values: Map<string, string>): string {
  const name: string = trim(textSlice(placeholder, 2n, i64(textLength(placeholder) - 1n)));
  if (values.has(name)) {
    return values.get(name) ?? "";
  }
  return placeholder;
}

class StringBuilder {
  constructor(public pieces: Array<string>, public indents: Array<bigint>) {}

  add(s: string): StringBuilder {
    this.pieces.push(s);
    return this;
  }

  line(s: string): StringBuilder {
    if (textLength(s) > 0n) {
      this.pieces.push(repeat(" ", this.indentNow()));
    }
    this.pieces.push(s);
    this.pieces.push("\n");
    return this;
  }

  blank(): StringBuilder {
    this.pieces.push("\n");
    return this;
  }

  lines(all: Array<string>): StringBuilder {
    for (let i = 0n; i < BigInt(all.length); i = i64(i + 1n)) {
      this.line(all[Number(i)]);
    }
    return this;
  }

  indent(n: bigint): StringBuilder {
    this.indents.push(i64(this.indentNow() + n));
    return this;
  }

  dedent(): StringBuilder {
    if (BigInt(this.indents.length) > 0n) {
      truncate(this.indents, i64(BigInt(this.indents.length) - 1n));
    }
    return this;
  }

  indentNow(): bigint {
    if (BigInt(this.indents.length) === 0n) {
      return 0n;
    }
    return this.indents[Number(i64(BigInt(this.indents.length) - 1n))];
  }

  length(): bigint {
    return textLength(this.text());
  }

  isEmpty(): boolean {
    return this.length() === 0n;
  }

  text(): string {
    return join(this.pieces, "");
  }
}

function offText(got: string, want: string): number {
  if (got === want) {
    return 0.0;
  }
  return 1.0;
}

function offInt(got: bigint, want: bigint): number {
  return Number(i64(got - want));
}

function checkEscapes(): number {
  const s: string = "a\tb\n\"c\" \\ $";
  return offInt(textLength(s), 11n);
}

function checkCharEscapes(): number {
  return offInt(i64(i64(BigInt(10) + BigInt(39)) + BigInt(92)), i64(i64(10n + 39n) + 92n));
}

function checkInterpolation(): number {
  const name: string = "zlib";
  const n: bigint = 42n;
  return offText(`lib ${name} v${i64(n + 1n)} ok=${n > 1n}`, "lib zlib v43 ok=true");
}

function checkInterpolatedMethodCalls(): number {
  const words: Array<string> = ["a", "b", "c"];
  return offText(`[${join(words, ", ")}]`, "[a, b, c]");
}

function checkTextBlock(): number {
  const name: string = "demo";
  const text: string = `project(${name})` + "\n" + "  add(\"x\")" + "\n" + "\n" + "done" + "\n";
  return offText(text, "project(demo)\n  add(\"x\")\n\ndone\n");
}

function checkTextBlockWithoutFinalNewline(): number {
  const text: string = "one" + "\n" + "two";
  return offText(text, "one\ntwo");
}

function checkTextBlockKeepsIndentOfMultiLineValue(): number {
  const inner: string = "a\nb";
  const text: string = "list:" + "\n" + "  " + indentRest(inner, 2n) + "\n" + "end" + "\n";
  return offText(text, "list:\n  a\n  b\nend\n");
}

function checkStrOfValues(): number {
  const c: number = 120;
  return offText(`${i64(0n - 7n)}${true}${false}${String.fromCharCode(c & 255)}${"s"}${12n}`, "-7truefalsexs12");
}

function checkCharAt(): number {
  const s: string = "hello";
  return offInt(BigInt(textByte(s, 1n)), 101n);
}

function checkSliceAndFind(): number {
  const s: string = "key=value";
  const at: bigint = textFind(s, "=", 0n);
  return offText(textSlice(s, 0n, at) + "|" + textSlice(s, i64(at + 1n), textLength(s)), "key|value") + offInt(textFind(s, "z", 0n), i64(0n - 1n));
}

function checkSliceClamps(): number {
  const s: string = "abc";
  return offText(textSlice(s, i64(0n - 5n), 99n) + textSlice(s, 2n, 1n), "abc");
}

function checkFindFrom(): number {
  const s: string = "a.b.c";
  return offInt(textFind(s, ".", 2n), 3n);
}

function checkListLiteralAndForIn(): number {
  let total: bigint = 0n;
  for (const w of ["cmake", "-S", "src"]) {
    total = i64(total + textLength(w));
  }
  return offInt(total, 10n);
}

function checkSplitAndJoin(): number {
  const parts: Array<string> = split("a,b,,c", ",");
  return offInt(BigInt(parts.length), 4n) + offText(join(parts, "+"), "a+b++c");
}

function checkTrimAndContains(): number {
  const s: string = "  \tpadded \n";
  return offText(trim(s), "padded") + offInt((contains(s, "dd") ? 1n : 0n), 1n) + offInt((contains(s, "x") ? 1n : 0n), 0n);
}

function checkStartsAndEnds(): number {
  const s: string = "CMakeLists.txt";
  const both: boolean = startsWith(s, "CMake") && endsWith(s, ".txt") && !endsWith(s, "CMake");
  return offInt((both ? 1n : 0n), 1n);
}

function checkReplaceRepeatIndent(): number {
  const s: string = replace("a-b-c", "-", "::");
  return offText(s + "|" + repeat("ab", 3n) + "|" + indent("x\n\ny", 2n), "a::b::c|ababab|  x\n\n  y");
}

function checkLines(): number {
  return offInt(BigInt(lines("one\ntwo\nthree").length), 3n);
}

function checkFill(): number {
  const values: Map<string, string> = new Map();
  values.set("name", "gcd");
  values.set("lang", "C");
  return offText(fill("project(${name} LANGUAGES ${lang}) ${missing}", values), "project(gcd LANGUAGES C) ${missing}");
}

function writeDeps(out: StringBuilder, deps: Array<string>): void {
  out.indent(4n);
  for (const d of deps) {
    out.line(`implementation(\"${d}\")`);
  }
  out.dedent();
}

function checkStringBuilder(): number {
  const out: StringBuilder = new StringBuilder([], []);
  out.line("dependencies {");
  writeDeps(out, ["a:b:1", "c:d:2"]);
  out.line("}");
  const want: string = "dependencies {" + "\n" + "    implementation(\"a:b:1\")" + "\n" + "    implementation(\"c:d:2\")" + "\n" + "}" + "\n";
  return offText(out.text(), want) + offInt((out.isEmpty() ? 1n : 0n), 0n);
}

export { contains, startsWith, endsWith, split, lines, join, isSpaceChar, trim, replace, repeat, indent, indentRest, fill, filledName, StringBuilder, offText, offInt, checkEscapes, checkCharEscapes, checkInterpolation, checkInterpolatedMethodCalls, checkTextBlock, checkTextBlockWithoutFinalNewline, checkTextBlockKeepsIndentOfMultiLineValue, checkStrOfValues, checkCharAt, checkSliceAndFind, checkSliceClamps, checkFindFrom, checkListLiteralAndForIn, checkSplitAndJoin, checkTrimAndContains, checkStartsAndEnds, checkReplaceRepeatIndent, checkLines, checkFill, writeDeps, checkStringBuilder };
