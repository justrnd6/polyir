# What `throw` raises: the value thrown.
class Thrown(Exception):
    def __init__(self, value):
        super().__init__(value)
        self.value = value

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def risky_double(x: int) -> int:
    if x < 0:
        raise Thrown(99)
    return i64(x * 2)

def safe_double(x: int) -> int:
    result: int = 0
    try:
        result = risky_double(x)
    except Thrown as thrown:
        code = thrown.value
        result = code
    return result

def nested_try_check(x: int) -> int:
    result: int = 0
    try:
        try:
            result = risky_double(x)
        except Thrown as thrown:
            inner_code = thrown.value
            result = i64(inner_code + 1)
            raise Thrown(result)
    except Thrown as thrown:
        outer_code = thrown.value
        result = i64(outer_code * 10)
    return result
