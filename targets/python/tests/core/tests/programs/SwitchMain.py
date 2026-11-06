from SwitchTests import check_booleans, check_call_subject, check_chars, check_conditions, check_conditions_among_keys, check_numbers, check_strings, check_sums

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def failed_if(off: float) -> int:
    if off > 0.001 or off < 0.0 - 0.001:
        return 1
    return 0

def main() -> int:
    failed: int = 0
    failed = i64(failed + failed_if(check_numbers()))
    failed = i64(failed + failed_if(check_chars()))
    failed = i64(failed + failed_if(check_strings()))
    failed = i64(failed + failed_if(check_booleans()))
    failed = i64(failed + failed_if(check_sums()))
    failed = i64(failed + failed_if(check_conditions_among_keys()))
    failed = i64(failed + failed_if(check_call_subject()))
    failed = i64(failed + failed_if(check_conditions()))
    return failed

if __name__ == "__main__":
    raise SystemExit(main())
