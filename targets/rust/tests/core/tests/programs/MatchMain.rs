#[path = "MatchTests.rs"]
mod MatchTests;
use MatchTests::{checkBooleans, checkComparisons, checkConditions, checkEithers, checkGuardsOnTheSubject, checkListGuards, checkLists, checkNested, checkNumbers, checkOptionals, checkPayloadPatterns, checkStatements, checkStrings, checkVariants};

pub fn failedIf(off: f64) -> i64 {
    if off > 0.001 || off < 0.0 - 0.001 {
        return 1;
    }
    return 0;
}

pub fn main_() -> i64 {
    let mut failed: i64 = 0;
    failed = failed + failedIf(checkNumbers());
    failed = failed + failedIf(checkComparisons());
    failed = failed + failedIf(checkGuardsOnTheSubject());
    failed = failed + failedIf(checkStrings());
    failed = failed + failedIf(checkConditions());
    failed = failed + failedIf(checkBooleans());
    failed = failed + failedIf(checkOptionals());
    failed = failed + failedIf(checkEithers());
    failed = failed + failedIf(checkVariants());
    failed = failed + failedIf(checkPayloadPatterns());
    failed = failed + failedIf(checkLists());
    failed = failed + failedIf(checkListGuards());
    failed = failed + failedIf(checkStatements());
    failed = failed + failedIf(checkNested());
    return failed;
}

fn main() {
    std::process::exit(main_() as i32);
}
