// A string's hash: djb2 over its UTF-8 bytes, wrapping at 64 bits, as the native code computes it.
function hashText(text: string): bigint {
  let hash = 5381n;
  for (const byte of utf8Bytes(text)) {
    hash = BigInt.asIntN(64, hash * 33n + BigInt(byte));
  }
  return hash;
}
