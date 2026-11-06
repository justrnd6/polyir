# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def hist_test() -> int:
    hist: list[int] = [0 for _ in range(4)]
    hist[0] = 10
    hist[1] = 20
    hist[2] = 30
    hist[3] = 40
    return i64(hist[1] + hist[2])

def len_check() -> int:
    hist: list[int] = [0 for _ in range(4)]
    return len(hist)

def sum_fixed(arr: list[int]) -> int:
    return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + arr[3])

def len_on_param(arr: list[int]) -> int:
    return len(arr)

def call_with_local() -> int:
    hist: list[int] = [0 for _ in range(4)]
    hist[0] = 1
    hist[1] = 2
    hist[2] = 3
    hist[3] = 4
    return i64(i64(sum_fixed(hist) * 100) + len_on_param(hist))
