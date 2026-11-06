# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A push used as a value gives the list, as in polyir (Python's append gives nothing).
def pushed(items: list, item) -> list:
    items.append(item)
    return items

def sum_pushed(a: int, b: int, c: int) -> int:
    arr: list[int] = []
    arr.append(a)
    arr.append(b)
    arr.append(c)
    arr.append(i64(i64(a + b) + c))
    return i64(i64(i64(arr[0] + arr[1]) + arr[2]) + arr[3])

def len_check(a: int, b: int, c: int) -> int:
    arr: list[int] = []
    arr.append(a)
    arr.append(b)
    arr.append(c)
    return len(arr)

def sum_squares_up_to(n: int) -> int:
    arr: list[int] = []
    i: int = 0
    while i < n:
        arr.append(i64(i * i))
        i = i64(i + 1)
    total: int = 0
    i: int = 0
    while i < len(arr):
        total = i64(total + arr[i])
        i = i64(i + 1)
    return total

def sum_evens_below(n: int) -> int:
    arr: list[int] = []
    i: int = 0
    while i < n:
        if rem(i, 2) == 0:
            arr.append(i)
        i = i64(i + 1)
    total: int = 0
    i: int = 0
    while i < len(arr):
        total = i64(total + arr[i])
        i = i64(i + 1)
    return total

def fill_squares(arr: list[int], count: int) -> int:
    i: int = 0
    while i < count:
        arr.append(i64(i * i))
        i = i64(i + 1)
    return count

def squares_through_a_helper(count: int) -> int:
    arr: list[int] = []
    fill_squares(arr, count)
    return i64(i64(len(arr) * 1000) + arr[i64(count - 1)])

def push_gives_the_array_back() -> int:
    arr: list[int] = []
    arr = pushed(arr, 10)
    arr = pushed(arr, 20)
    return i64(arr[0] + arr[1])

def sum_evens_below_explicit(n: int) -> int:
    label: int = 0
    i: int = 0
    arr: list[int] = []
    while True:
        if label == 0:
            empty: list[int] = []
            i = 0
            arr = empty
            label = 1
            continue
        if label == 1:
            if i < n:
                label = 2
                continue
            else:
                label = 5
                continue
        if label == 2:
            if rem(i, 2) == 0:
                label = 3
                continue
            else:
                label = 4
                continue
        if label == 3:
            next_i: int = i64(i + 1)
            next_arr: list[int] = pushed(arr, i)
            i = next_i
            arr = next_arr
            label = 1
            continue
        if label == 4:
            i = i64(i + 1)
            label = 1
            continue
        if label == 5:
            i: int = 0
            total: int = 0
            while i < len(arr):
                i = i64(i + 1)
                total = i64(total + arr[i])
            return total
