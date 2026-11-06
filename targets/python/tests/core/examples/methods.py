# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def square(x: int) -> int:
    return i64(x * x)

def sub(a: int, b: int) -> int:
    return i64(a - b)

def equivalence_check(x: int) -> int:
    return i64(square(x) - square(x))

def ordering_check(a: int, b: int) -> int:
    return i64(sub(a, b) - i64(a - b))

def push_via_method_persists() -> int:
    arr: list[int] = []
    arr.append(10)
    arr.append(20)
    return len(arr)

def push_via_method_sum() -> int:
    arr: list[int] = []
    arr.append(1)
    arr.append(2)
    arr.append(3)
    return i64(i64(arr[0] + arr[1]) + arr[2])
