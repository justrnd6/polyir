#[path = "StringTests.rs"]
mod StringTests;
use StringTests::{StringBuilder, StringBuilder_line, checkCharAt, checkCharEscapes, checkEscapes, checkFill, checkFindFrom, checkInterpolatedMethodCalls, checkInterpolation, checkLines, checkListLiteralAndForIn, checkReplaceRepeatIndent, checkSliceAndFind, checkSliceClamps, checkSplitAndJoin, checkStartsAndEnds, checkStrOfValues, checkStringBuilder, checkTextBlock, checkTextBlockKeepsIndentOfMultiLineValue, checkTextBlockWithoutFinalNewline, checkTrimAndContains};

pub fn failedIf(off: f64) -> i64 {
    if off > 0.001 || off < 0.0 - 0.001 {
        return 1;
    }
    return 0;
}

pub fn main_() -> i64 {
    let mut failed: i64 = 0;
    failed = failed + failedIf(checkEscapes());
    failed = failed + failedIf(checkCharEscapes());
    failed = failed + failedIf(checkInterpolation());
    failed = failed + failedIf(checkInterpolatedMethodCalls());
    failed = failed + failedIf(checkTextBlock());
    failed = failed + failedIf(checkTextBlockWithoutFinalNewline());
    failed = failed + failedIf(checkTextBlockKeepsIndentOfMultiLineValue());
    failed = failed + failedIf(checkStrOfValues());
    failed = failed + failedIf(checkCharAt());
    failed = failed + failedIf(checkSliceAndFind());
    failed = failed + failedIf(checkSliceClamps());
    failed = failed + failedIf(checkFindFrom());
    failed = failed + failedIf(checkListLiteralAndForIn());
    failed = failed + failedIf(checkSplitAndJoin());
    failed = failed + failedIf(checkTrimAndContains());
    failed = failed + failedIf(checkStartsAndEnds());
    failed = failed + failedIf(checkReplaceRepeatIndent());
    failed = failed + failedIf(checkLines());
    failed = failed + failedIf(checkFill());
    failed = failed + failedIf(checkStringBuilder());
    return failed;
}

fn main() {
    std::process::exit(main_() as i32);
}
