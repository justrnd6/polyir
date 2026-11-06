// How many UTF-8 bytes a string has.
function textLength(text: string): bigint {
  return BigInt(utf8Bytes(text).length);
}
