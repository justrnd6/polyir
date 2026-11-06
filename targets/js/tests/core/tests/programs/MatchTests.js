"use strict";

// A string's UTF-8 bytes, and bytes back to a string, as polyir counts a string's positions in bytes. An unpaired
// surrogate is U+FFFD, and so is each byte sequence that is not UTF-8.
function utf8Bytes(text) {
  const bytes = [];
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

function utf8Text(bytes) {
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
function textFind(text, part, from) {
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
function textLength(text) {
  return BigInt(utf8Bytes(text).length);
}

// The bytes from `from` up to `to` of a string, both kept inside it.
function textSlice(text, from, to) {
  const bytes = utf8Bytes(text);
  const start = Math.max(0, Math.min(Number(from), bytes.length));
  const end = Math.max(start, Math.min(Number(to), bytes.length));
  return utf8Text(bytes.slice(start, end));
}

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

// An `either` holds its value as an `Ok` or an `Err`.
class Ok {
  constructor(value) {
    this.value = value;
  }
}

class Err {
  constructor(value) {
    this.value = value;
  }
}

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(tag, value) {
    this.tag = tag;
    this.value = value;
  }
}

function contains(s, part) {
  return textFind(s, part, 0n) >= 0n;
}

function startsWith(s, prefix) {
  if (textLength(prefix) > textLength(s)) {
    return false;
  }
  return textSlice(s, 0n, textLength(prefix)) === prefix;
}

function endsWith(s, suffix) {
  if (textLength(suffix) > textLength(s)) {
    return false;
  }
  return textSlice(s, i64(textLength(s) - textLength(suffix)), textLength(s)) === suffix;
}

function join(parts, sep) {
  let out = "";
  for (let i = 0n; i < BigInt(parts.length); i = i64(i + 1n)) {
    if (i > 0n) {
      out = out + sep;
    }
    out = out + parts[Number(i)];
  }
  return out;
}

function offText(got, want) {
  if (got === want) {
    return 0.0;
  }
  return 1.0;
}

function sizeOf(n) {
  let matchValue_16_12 = "";
  switch (n) {
    case 0n: {
      matchValue_16_12 = "none";
      break;
    }
    case 1n: case 2n: case 3n: {
      matchValue_16_12 = "a few";
      break;
    }
    case -1n: {
      matchValue_16_12 = "minus one";
      break;
    }
    default: {
      matchValue_16_12 = "many";
    }
  }
  return matchValue_16_12;
}

function checkNumbers() {
  return offText(sizeOf(0n) + sizeOf(2n) + sizeOf(9n) + sizeOf(-1n), "nonea fewmanyminus one");
}

function signOf(n) {
  let matchValue_30_16 = "";
  if (n < 0n) {
    matchValue_30_16 = "negative";
  } else if (n === 0n) {
    matchValue_30_16 = "zero";
  } else if (n >= 100n) {
    matchValue_30_16 = "large";
  } else {
    matchValue_30_16 = "positive";
  }
  const sign = matchValue_30_16;
  return sign;
}

function checkComparisons() {
  return offText(signOf(-3n) + " " + signOf(0n) + " " + signOf(150n) + " " + signOf(5n), "negative zero large positive");
}

function gradeOf(score) {
  let matchValue_45_12 = "";
  if (score === 100n) {
    matchValue_45_12 = "perfect";
  } else if (score >= 90n) {
    const s = score;
    matchValue_45_12 = `A ${s}`;
  } else if (score >= 80n) {
    matchValue_45_12 = "B";
  } else {
    matchValue_45_12 = "C";
  }
  return matchValue_45_12;
}

function checkGuardsOnTheSubject() {
  return offText(gradeOf(100n) + gradeOf(93n) + gradeOf(85n) + gradeOf(10n), "perfectA 93BC");
}

function yes(reply) {
  let matchValue_60_12 = false;
  switch (reply) {
    case "y": case "yes": {
      matchValue_60_12 = true;
      break;
    }
    case "n": case "no": {
      matchValue_60_12 = false;
      break;
    }
    default: {
      matchValue_60_12 = false;
    }
  }
  return matchValue_60_12;
}

function checkStrings() {
  return offText(`${yes("y")} ${yes("yes")} ${yes("no")} ${yes("maybe")}`, "true true false false");
}

function kindOf(path) {
  let matchValue_73_12 = "";
  if (path === "Ninafile" || path === "Polyfile") {
    matchValue_73_12 = "build";
  } else if (endsWith(path, ".poly")) {
    matchValue_73_12 = "polyir";
  } else if (startsWith(path, "README")) {
    matchValue_73_12 = "docs";
  } else if (contains(path, "test") && !endsWith(path, ".md")) {
    matchValue_73_12 = "a test";
  } else if (endsWith(path, ".md")) {
    const p = path;
    matchValue_73_12 = `markdown ${p}`;
  } else {
    matchValue_73_12 = "other";
  }
  return matchValue_73_12;
}

function checkConditions() {
  const kinds = [kindOf("Polyfile"), kindOf("a.poly"), kindOf("README.md"), kindOf("my_test.c"), kindOf("notes.md"), kindOf("x.c")];
  return offText(join(kinds, ","), "build,polyir,docs,a test,markdown notes.md,other");
}

function word(ready) {
  let matchValue_89_12 = "";
  switch (ready) {
    case true: {
      matchValue_89_12 = "go";
      break;
    }
    case false: {
      matchValue_89_12 = "wait";
      break;
    }
  }
  return matchValue_89_12;
}

function checkBooleans() {
  return offText(word(true) + word(false), "gowait");
}

function shown(found) {
  let matchValue_102_12 = "";
  if (found !== null && present(found) > 100n) {
    const n = present(found);
    matchValue_102_12 = `big ${n}`;
  } else if (found !== null) {
    const n = present(found);
    matchValue_102_12 = `found ${n}`;
  } else {
    matchValue_102_12 = "nothing";
  }
  return matchValue_102_12;
}

function checkOptionals() {
  return offText(shown(500n) + ", " + shown(5n) + ", " + shown(null), "big 500, found 5, nothing");
}

function safeDivide(a, b) {
  if (b === 0n) {
    return new Err("divide by zero");
  }
  return new Ok(a / b);
}

function message(a, b) {
  const match_122_12 = safeDivide(a, b);
  let matchValue_122_12 = "";
  if (match_122_12 instanceof Ok) {
    const q = match_122_12.value;
    matchValue_122_12 = `quotient ${q}`;
  } else if (match_122_12 instanceof Err) {
    const why = match_122_12.value;
    matchValue_122_12 = `error ${why}`;
  }
  return matchValue_122_12;
}

function checkEithers() {
  return offText(message(7n, 2n) + "; " + message(7n, 0n), "quotient 3; error divide by zero");
}

function textOf(shape) {
  let matchValue_133_12 = "";
  if (shape.tag === 0n) {
    const r = shape.value;
    matchValue_133_12 = `circle ${r}`;
  } else if (shape.tag === 1n) {
    const s = shape.value;
    matchValue_133_12 = `square ${s}`;
  } else if (shape.tag === 2n) {
    const name = shape.value;
    matchValue_133_12 = name;
  }
  return matchValue_133_12;
}

function checkVariants() {
  const circle = textOf(new Variant(0n, 2n));
  return offText(circle + ", " + textOf(new Variant(1n, 4n)) + ", " + textOf(new Variant(2n, "blob")), "circle 2, square 4, blob");
}

function zeroOr(found) {
  let matchValue_147_12 = "";
  if (found !== null && present(found) === 0n) {
    matchValue_147_12 = "zero";
  } else if (found !== null) {
    matchValue_147_12 = "other";
  } else {
    matchValue_147_12 = "none";
  }
  return matchValue_147_12;
}

function checkPayloadPatterns() {
  return offText(zeroOr(0n) + zeroOr(3n) + zeroOr(null), "zeroothernone");
}

function said(xs) {
  let matchValue_161_12 = "";
  if (BigInt(xs.length) === 0n) {
    matchValue_161_12 = "empty";
  } else if (BigInt(xs.length) === 1n) {
    const x = xs[0];
    matchValue_161_12 = `just ${x}`;
  } else if (xs[0] === 0n) {
    matchValue_161_12 = "starts at zero";
  } else {
    const first = xs[0];
    const rest = drop(xs, 1n);
    matchValue_161_12 = `${first}, then ${BigInt(rest.length)} more`;
  }
  return matchValue_161_12;
}

function checkLists() {
  const none = [];
  return offText(said(none) + "; " + said([7n]) + "; " + said([0n, 1n, 2n]) + "; " + said([3n, 4n, 5n]), "empty; just 7; starts at zero; 3, then 2 more");
}

function pairOf(xs) {
  let matchValue_176_12 = "";
  if (BigInt(xs.length) === 2n && xs[0] === xs[1]) {
    const a = xs[0];
    matchValue_176_12 = `a pair of ${a}`;
  } else if (BigInt(xs.length) === 2n) {
    const a = xs[0];
    const b = xs[1];
    matchValue_176_12 = `${a} and ${b}`;
  } else if (BigInt(xs.length) >= 1n && xs[0] < 0n) {
    matchValue_176_12 = "starts below zero";
  } else {
    matchValue_176_12 = "something else";
  }
  return matchValue_176_12;
}

function checkListGuards() {
  const same_ = [4n, 4n];
  const two = [1n, 2n];
  const low = [-1n, 2n, 3n];
  return offText(pairOf(same_) + "; " + pairOf(two) + "; " + pairOf(low) + "; " + pairOf([5n]), "a pair of 4; 1 and 2; starts below zero; something else");
}

function command(c) {
  let said = "";
  if (c === "quit" || c === "exit") {
    return "bye";
  } else if (startsWith(c, "say ")) {
    said = textSlice(c, 4n, textLength(c));
  } else if (c === "noop") {}
  return said;
}

function checkStatements() {
  return offText(command("say hi") + "|" + command("quit") + "|" + command("noop") + "|" + command("huh"), "hi|bye||");
}

function checkNested() {
  const a = 2n;
  const b = 5n;
  let matchValue_212_21 = "";
  switch (a) {
    case 1n: {
      matchValue_212_21 = "one";
      break;
    }
    case 2n: {
      let matchValue_212_49 = "";
      switch (b) {
        case 5n: {
          matchValue_212_49 = "two-five";
          break;
        }
        default: {
          matchValue_212_49 = "two";
        }
      }
      matchValue_212_21 = matchValue_212_49;
      break;
    }
    default: {
      matchValue_212_21 = "many";
    }
  }
  const text = matchValue_212_21;
  return offText(text, "two-five");
}

function drop(xs, n) {
  const out = [];
  for (let i = n; i < BigInt(xs.length); i = i64(i + 1n)) {
    out.push(xs[Number(i)]);
  }
  return out;
}

export { contains, startsWith, endsWith, join, offText, sizeOf, checkNumbers, signOf, checkComparisons, gradeOf, checkGuardsOnTheSubject, yes, checkStrings, kindOf, checkConditions, word, checkBooleans, shown, checkOptionals, safeDivide, message, checkEithers, textOf, checkVariants, zeroOr, checkPayloadPatterns, said, checkLists, pairOf, checkListGuards, command, checkStatements, checkNested, drop };
