# An operator lane by lane: on two vectors, or on a vector and one number.
def lanes(a: list, b, op) -> list:
    return [op(x, b[i] if isinstance(b, list) else b) for i, x in enumerate(a)]
