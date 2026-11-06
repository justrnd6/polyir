# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def fill_to_capacity() -> int:
    arr: list[int] = [0 for _ in range(4)]
    arr[0] = 10
    arr[1] = 20
    arr[2] = 30
    arr[3] = 40
    return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + arr[3])

def capacity_check() -> int:
    arr: list[int] = [0 for _ in range(4)]
    arr[0] = 1
    arr[1] = 2
    return len(arr)

def let_bound_indexed_fill() -> int:
    arr: list[int] = [0 for _ in range(3)]
    arr[0] = 5
    arr[1] = 6
    arr[2] = 7
    return i64(i64(arr[0] + arr[1]) + arr[2])

def sum_via_loop(n: int) -> int:
    arr: list[int] = [0 for _ in range(10)]
    i: int = 0
    while i < n:
        arr[i] = i
        i = i64(i + 1)
    total: int = 0
    i: int = 0
    while i < n:
        total = i64(total + arr[i])
        i = i64(i + 1)
    return total

def string_elements() -> int:
    names: list[str] = ["" for _ in range(2)]
    names[0] = "first"
    names[1] = "second"
    if names[0] != "first":
        return 0
    if names[1] != "second":
        return 0
    return 1

def sum_param(arr: list[int]) -> int:
    return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + len(arr))

def via_param() -> int:
    arr: list[int] = [0 for _ in range(3)]
    arr[0] = 100
    arr[1] = 200
    arr[2] = 300
    return sum_param(arr)
