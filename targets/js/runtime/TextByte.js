// The byte at a position of a string, as a signed 8-bit number; 0 outside the string.
function textByte(text, at) {
  const bytes = utf8Bytes(text);
  const i = Number(at);
  return i >= 0 && i < bytes.length ? bytes[i] << 24 >> 24 : 0;
}
