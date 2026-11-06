from StringTests import StringBuilder, StringBuilder_line, check_char_at, check_char_escapes, check_escapes, check_fill, check_find_from, check_interpolated_method_calls, check_interpolation, check_lines, check_list_literal_and_for_in, check_replace_repeat_indent, check_slice_and_find, check_slice_clamps, check_split_and_join, check_starts_and_ends, check_str_of_values, check_string_builder, check_text_block, check_text_block_keeps_indent_of_multi_line_value, check_text_block_without_final_newline, check_trim_and_contains

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def failed_if(off: float) -> int:
    if off > 0.001 or off < 0.0 - 0.001:
        return 1
    return 0

def main() -> int:
    failed: int = 0
    failed = i64(failed + failed_if(check_escapes()))
    failed = i64(failed + failed_if(check_char_escapes()))
    failed = i64(failed + failed_if(check_interpolation()))
    failed = i64(failed + failed_if(check_interpolated_method_calls()))
    failed = i64(failed + failed_if(check_text_block()))
    failed = i64(failed + failed_if(check_text_block_without_final_newline()))
    failed = i64(failed + failed_if(check_text_block_keeps_indent_of_multi_line_value()))
    failed = i64(failed + failed_if(check_str_of_values()))
    failed = i64(failed + failed_if(check_char_at()))
    failed = i64(failed + failed_if(check_slice_and_find()))
    failed = i64(failed + failed_if(check_slice_clamps()))
    failed = i64(failed + failed_if(check_find_from()))
    failed = i64(failed + failed_if(check_list_literal_and_for_in()))
    failed = i64(failed + failed_if(check_split_and_join()))
    failed = i64(failed + failed_if(check_trim_and_contains()))
    failed = i64(failed + failed_if(check_starts_and_ends()))
    failed = i64(failed + failed_if(check_replace_repeat_indent()))
    failed = i64(failed + failed_if(check_lines()))
    failed = i64(failed + failed_if(check_fill()))
    failed = i64(failed + failed_if(check_string_builder()))
    return failed

if __name__ == "__main__":
    raise SystemExit(main())
