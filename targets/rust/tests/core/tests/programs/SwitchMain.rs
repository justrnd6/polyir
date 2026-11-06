#[path = "SwitchTests.rs"]
mod SwitchTests;
use SwitchTests::{checkBooleans, checkCallSubject, checkChars, checkConditions, checkConditionsAmongKeys, checkNumbers, checkStrings, checkSums};

pub fn failedIf(off: f64) -> i64 {
    if off > 0.001 || off < 0.0 - 0.001 {
        return 1;
    }
    return 0;
}

pub fn main_() -> i64 {
    let mut failed: i64 = 0;
    failed = failed + failedIf(checkNumbers());
    failed = failed + failedIf(checkChars());
    failed = failed + failedIf(checkStrings());
    failed = failed + failedIf(checkBooleans());
    failed = failed + failedIf(checkSums());
    failed = failed + failedIf(checkConditionsAmongKeys());
    failed = failed + failedIf(checkCallSubject());
    failed = failed + failedIf(checkConditions());
    return failed;
}

fn main() {
    std::process::exit(main_() as i32);
}
