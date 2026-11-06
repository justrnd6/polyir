# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

def queens(t: int, prefixes: list[int], p: list[int], stack: list[int]) -> int:
    n: int = p[0]
    depth: int = p[1]
    full: int = i32(i64(i64(1 << n) - 1))
    base: int = i32(i32(t * i32(n + 1)) * 4)
    stack[base] = prefixes[i32(t * 3)]
    stack[i32(base + 1)] = prefixes[i32(i32(t * 3) + 1)]
    stack[i32(base + 2)] = prefixes[i32(i32(t * 3) + 2)]
    stack[i32(base + 3)] = full & ~(prefixes[i32(t * 3)] | prefixes[i32(i32(t * 3) + 1)] | prefixes[i32(i32(t * 3) + 2)])
    level: int = 0
    count: int = 0
    step: int = 0
    while level >= 0:
        at: int = i32(base + i32(level * 4))
        free: int = stack[i32(at + 3)]
        if free == 0:
            level = i32(level - 1)
        else:
            bit: int = free & i32(i64(0 - free))
            stack[i32(at + 3)] = free ^ bit
            if i32(i32(depth + level) + 1) == n:
                count = i32(count + 1)
            else:
                next_cols: int = stack[at] | bit
                next_left: int = i32((stack[i32(at + 1)] | bit) << 1) & full
                next_right: int = (stack[i32(at + 2)] | bit) >> 1
                level = i32(level + 1)
                below: int = i32(base + i32(level * 4))
                stack[below] = next_cols
                stack[i32(below + 1)] = next_left
                stack[i32(below + 2)] = next_right
                stack[i32(below + 3)] = full & ~(next_cols | next_left | next_right)
        step = i64(step + 1)
    return count

def list_prefixes(n: int, depth: int, out: list[int]) -> int:
    total: int = 1
    r: int = 0
    while r < depth:
        total = i64(total * n)
        r = i64(r + 1)
    full: int = i32(i64(i64(1 << n) - 1))
    found: int = 0
    code: int = 0
    while code < total:
        left: int = 0
        right: int = 0
        cols: int = 0
        ok: int = 1
        rest: int = code
        r: int = 0
        while r < depth:
            column: int = rem(rest, n)
            rest = div(rest, n)
            bit: int = i32(i64(1 << column))
            if bit & (cols | left | right) != 0:
                ok = 0
            cols = cols | bit
            left = i32((left | bit) << 1) & full
            right = (right | bit) >> 1
            r = i64(r + 1)
        if ok == 1:
            out[i64(found * 3)] = cols
            out[i64(i64(found * 3) + 1)] = left
            out[i64(i64(found * 3) + 2)] = right
            found = i64(found + 1)
        code = i64(code + 1)
    return found
