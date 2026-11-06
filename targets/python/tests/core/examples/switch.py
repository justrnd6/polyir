# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def day_kind(day: int) -> int:
    label: int = 0
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
            elif day == 3:
                label = 2
                continue
            elif day == 4:
                label = 2
                continue
            elif day == 5:
                label = 2
                continue
            else:
                label = 3
                continue
        if label == 1:
            return 0
        if label == 2:
            return 1
        if label == 3:
            return i64(0 - 1)
