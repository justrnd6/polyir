from MatchTests import check_booleans, check_comparisons, check_conditions, check_eithers, check_guards_on_the_subject, check_list_guards, check_lists, check_nested, check_numbers, check_optionals, check_payload_patterns, check_statements, check_strings, check_variants

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
    failed = i64(failed + failed_if(check_comparisons()))
    failed = i64(failed + failed_if(check_guards_on_the_subject()))
    failed = i64(failed + failed_if(check_strings()))
    failed = i64(failed + failed_if(check_conditions()))
    failed = i64(failed + failed_if(check_booleans()))
    failed = i64(failed + failed_if(check_optionals()))
    failed = i64(failed + failed_if(check_eithers()))
    failed = i64(failed + failed_if(check_variants()))
    failed = i64(failed + failed_if(check_payload_patterns()))
    failed = i64(failed + failed_if(check_lists()))
    failed = i64(failed + failed_if(check_list_guards()))
    failed = i64(failed + failed_if(check_statements()))
    failed = i64(failed + failed_if(check_nested()))
    return failed

if __name__ == "__main__":
    raise SystemExit(main())
