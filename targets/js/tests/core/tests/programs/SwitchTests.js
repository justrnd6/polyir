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

function endsWith(s, suffix) {
  if (textLength(suffix) > textLength(s)) {
    return false;
  }
  return textSlice(s, i64(textLength(s) - textLength(suffix)), textLength(s)) === suffix;
}

function offText(got, want) {
  if (got === want) {
    return 0.0;
  }
  return 1.0;
}

function dayName(day) {
  let name = "?";
  switch (day) {
    case 0n: case 6n: {
      name = "weekend";
      break;
    }
    case 1n: {
      name = "monday";
      break;
    }
    default: {
      name = "weekday";
    }
  }
  return name;
}

function checkNumbers() {
  return offText(dayName(0n) + dayName(6n) + dayName(1n) + dayName(3n), "weekendweekendmondayweekday");
}

function classOf(c) {
  let kind = "other";
  switch (c) {
    case 97: case 101: case 105: case 111: case 117: {
      kind = "vowel";
      break;
    }
    case 32: {
      kind = "space";
      break;
    }
  }
  return kind;
}

function checkChars() {
  return offText(classOf(101) + classOf(32) + classOf(122), "vowelspaceother");
}

function verb(command) {
  let done = "";
  switch (command) {
    case "start": case "go": {
      done = "began";
      break;
    }
    case "stop": {
      done = "halted";
      break;
    }
    default: {
      done = "?";
    }
  }
  return done;
}

function checkStrings() {
  return offText(verb("go") + verb("start") + verb("stop") + verb("x"), "beganbeganhalted?");
}

function checkBooleans() {
  let said = "";
  for (let i = 0n; i < 2n; i = i64(i + 1n)) {
    const match_57_9 = i === 0n;
    switch (match_57_9) {
      case true: {
        said = said + "yes";
        break;
      }
      case false: {
        said = said + "no";
        break;
      }
    }
  }
  return offText(said, "yesno");
}

function outcome(r) {
  let said = "";
  if (r instanceof Ok) {
    const v = r.value;
    said = `ok ${v}`;
  } else if (r instanceof Err) {
    const e = r.value;
    said = `err ${e}`;
  }
  return said;
}

function checkSums() {
  let count = 0n;
  const found = 4n;
  if (found !== null) {
    const n = present(found);
    count = n;
  } else if (found === null) {
    count = -1n;
  }
  return offText(`${outcome(new Ok(3n))}, ${outcome(new Err("no"))}, ${count}`, "ok 3, err no, 4");
}

function status(code) {
  let said = "";
  if (code === 200n || code === 204n) {
    said = "done";
  } else if (code >= 500n) {
    said = `retry ${code}`;
  } else if (code >= 400n) {
    const s = code;
    said = `fail ${s}`;
  } else {
    said = `? ${code}`;
  }
  return said;
}

function checkConditionsAmongKeys() {
  return offText(status(204n) + "; " + status(503n) + "; " + status(404n) + "; " + status(301n), "done; retry 503; fail 404; ? 301");
}

function counter() {
  return 500n;
}

function checkCallSubject() {
  let said = "";
  const match_107_5 = counter();
  if (match_107_5 === 200n) {
    said = "ok";
  } else if (match_107_5 >= 500n) {
    const s = match_107_5;
    said = `server ${s}`;
  } else {
    said = "other";
  }
  return offText(said, "server 500");
}

function step(name) {
  let said = "";
  if (endsWith(name, ".poly")) {
    said = "compile " + name + ";";
  } else {
    said = "skip " + name + ";";
  }
  return said;
}

function checkConditions() {
  return offText(step("a.poly") + step("b.md"), "compile a.poly;skip b.md;");
}

export { endsWith, offText, dayName, checkNumbers, classOf, checkChars, verb, checkStrings, checkBooleans, outcome, checkSums, status, checkConditionsAmongKeys, counter, checkCallSubject, step, checkConditions };
