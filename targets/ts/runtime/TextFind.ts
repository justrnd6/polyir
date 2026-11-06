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
