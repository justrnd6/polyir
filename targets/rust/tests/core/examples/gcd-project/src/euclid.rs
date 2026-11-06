pub fn gcd(a: i64, b: i64) -> i64 {
    let mut x: i64 = a;
    let mut y: i64 = b;
    while y != 0 {
        let r: i64 = x % y;
        x = y;
        y = r;
    }
    return x;
}

pub fn lcm(a: i64, b: i64) -> i64 {
    return a / gcd(a, b) * b;
}
