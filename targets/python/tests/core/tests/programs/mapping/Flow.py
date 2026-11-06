# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

def sum_to(n: int) -> int:
    label: int = 0
    total: int = 0
    i: int = 0
    result: int = 0
    while True:
        if label == 0:
            total = 0
            i = 1
            label = 1
            continue
        if label == 1:
            if i > n:
                result = total
                label = 3
                continue
            else:
                label = 2
                continue
        if label == 2:
            total = i64(total + i)
            i = i64(i + 1)
            label = 1
            continue
        if label == 3:
            return result

def gcd(a0: int, b0: int) -> int:
    label: int = 0
    a: int = 0
    b: int = 0
    result: int = 0
    while True:
        if label == 0:
            a = a0
            b = b0
            label = 1
            continue
        if label == 1:
            if b == 0:
                result = a
                label = 3
                continue
            else:
                label = 2
                continue
        if label == 2:
            r: int = rem(a, b)
            a = b
            b = r
            label = 1
            continue
        if label == 3:
            return result

def collatz(n: int) -> int:
    label: int = 0
    x: int = 0
    steps: int = 0
    count: int = 0
    while True:
        if label == 0:
            x = n
            steps = 0
            label = 1
            continue
        if label == 1:
            if x == 1:
                count = steps
                label = 5
                continue
            else:
                label = 2
                continue
        if label == 2:
            if rem(x, 2) == 0:
                label = 3
                continue
            else:
                label = 4
                continue
        if label == 3:
            x = div(x, 2)
            steps = i64(steps + 1)
            label = 1
            continue
        if label == 4:
            x = i64(i64(3 * x) + 1)
            steps = i64(steps + 1)
            label = 1
            continue
        if label == 5:
            return count

def swapped(a: int, b: int) -> int:
    label: int = 0
    x: int = 0
    y: int = 0
    k: int = 0
    while True:
        if label == 0:
            x = a
            y = b
            k = 0
            label = 1
            continue
        if label == 1:
            if k == 3:
                label = 3
                continue
            else:
                label = 2
                continue
        if label == 2:
            next_x: int = y
            next_y: int = x
            next_k: int = i64(k + 1)
            x = next_x
            y = next_y
            k = next_k
            label = 1
            continue
        if label == 3:
            return i64(i64(x * 10) + y)

def weight(day: int) -> int:
    label: int = 0
    w: int = 0
    while True:
        if label == 0:
            if day == 0:
                label = 1
                continue
            elif day == 6:
                label = 1
                continue
            elif day == 1:
                label = 2
                continue
            elif day == 2:
                label = 2
                continue
            else:
                label = 3
                continue
        if label == 1:
            w = 1
            label = 4
            continue
        if label == 2:
            w = 2
            label = 4
            continue
        if label == 3:
            w = 3
            label = 4
            continue
        if label == 4:
            return i64(w * 100)

def zigzag(a: int) -> int:
    label: int = 0
    p: int = 0
    q: int = 0
    r: int = 0
    while True:
        if label == 0:
            if a == 0:
                p = a
                label = 1
                continue
            else:
                q = a
                label = 2
                continue
        if label == 1:
            next_: int = i64(p + 1)
            if next_ > 5:
                r = next_
                label = 3
                continue
            else:
                q = next_
                label = 2
                continue
        if label == 2:
            if q > 5:
                r = q
                label = 3
                continue
            else:
                p = i64(q + 1)
                label = 1
                continue
        if label == 3:
            return i64(r * 2)

def main() -> int:
    score: int = sum_to(10)
    score = i64(score + gcd(48, 18))
    score = i64(score + collatz(6))
    score = i64(score + swapped(4, 7))
    score = i64(i64(i64(score + weight(0)) + weight(2)) + weight(9))
    score = i64(i64(score + zigzag(0)) + zigzag(3))
    return score

if __name__ == "__main__":
    raise SystemExit(main())
