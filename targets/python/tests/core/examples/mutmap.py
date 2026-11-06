# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def collision_and_overwrite() -> int:
    hm: dict[int, int] = {}
    hm[0] = 100
    hm[5] = 500
    hm[1] = 10
    hm[1] = 20
    return i64(i64(i64(hm.get(0, 0) * 10000) + i64(hm.get(5, 0) * 10)) + hm.get(1, 0))

def missing_key_is_zero() -> int:
    hm: dict[int, int] = {}
    hm[0] = 7
    return hm.get(42, 0)

def size_check() -> int:
    hm: dict[int, int] = {}
    hm[0] = 1
    hm[1] = 2
    hm[1] = 99
    return i64(len(hm) * 100)

def string_keyed_write_and_read_back() -> int:
    hm: dict[str, int] = {}
    hm["one"] = 1
    hm["two"] = 2
    hm["one"] = 11
    return i64(i64(hm.get("one", 0) * 100) + hm.get("two", 0))

def string_key_missing_is_zero() -> int:
    hm: dict[str, int] = {}
    hm["present"] = 5
    return hm.get("absent", 0)

def hashmap_ptr_value_miss_is_safe() -> int:
    hm: dict[int, str] = {}
    hm[1] = "present"
    missing: str = hm.get(99, "")
    return 1
