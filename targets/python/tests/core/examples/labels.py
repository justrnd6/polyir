# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

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
