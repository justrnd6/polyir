// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function collisionAndOverwrite(): bigint {
  const hm: Map<bigint, bigint> = new Map();
  hm.set(0n, 100n);
  hm.set(5n, 500n);
  hm.set(1n, 10n);
  hm.set(1n, 20n);
  return i64(i64(i64((hm.get(0n) ?? 0n) * 10000n) + i64((hm.get(5n) ?? 0n) * 10n)) + (hm.get(1n) ?? 0n));
}

function missingKeyIsZero(): bigint {
  const hm: Map<bigint, bigint> = new Map();
  hm.set(0n, 7n);
  return hm.get(42n) ?? 0n;
}

function sizeCheck(): bigint {
  const hm: Map<bigint, bigint> = new Map();
  hm.set(0n, 1n);
  hm.set(1n, 2n);
  hm.set(1n, 99n);
  return i64(BigInt(hm.size) * 100n);
}

function stringKeyedWriteAndReadBack(): bigint {
  const hm: Map<string, bigint> = new Map();
  hm.set("one", 1n);
  hm.set("two", 2n);
  hm.set("one", 11n);
  return i64(i64((hm.get("one") ?? 0n) * 100n) + (hm.get("two") ?? 0n));
}

function stringKeyMissingIsZero(): bigint {
  const hm: Map<string, bigint> = new Map();
  hm.set("present", 5n);
  return hm.get("absent") ?? 0n;
}

function hashmapPtrValueMissIsSafe(): bigint {
  const hm: Map<bigint, string> = new Map();
  hm.set(1n, "present");
  const missing: string = hm.get(99n) ?? "";
  return 1n;
}

export { collisionAndOverwrite, missingKeyIsZero, sizeCheck, stringKeyedWriteAndReadBack, stringKeyMissingIsZero, hashmapPtrValueMissIsSafe };
