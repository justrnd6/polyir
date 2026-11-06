// How many UTF-8 bytes a string has.
function textLength(text) {
  return BigInt(utf8Bytes(text).length);
}
