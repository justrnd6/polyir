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
