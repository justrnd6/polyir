// The bytes from `from` up to `to` of a string, both kept inside it.
function textSlice(text: string, from: bigint, to: bigint): string {
  const bytes = utf8Bytes(text);
  const start = Math.max(0, Math.min(Number(from), bytes.length));
  const end = Math.max(start, Math.min(Number(to), bytes.length));
  return utf8Text(bytes.slice(start, end));
}
