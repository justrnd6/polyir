pub fn classify(x: i64) -> i64 {
    return if x < 0 { 0 - 1 } else { if x == 0 { 0 } else { 1 } };
}

pub fn maxVal(a: i64, b: i64) -> i64 {
    let mut result: i64 = 0;
    if a < b {
        result = b;
    } else {
        result = a;
    }
    return result;
}

pub fn absViaIf(x: i64) -> i64 {
    let mut r: i64 = x;
    if r < 0 {
        r = 0 - r;
    }
    return r;
}
