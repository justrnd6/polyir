#[path = "euclid.rs"]
mod euclid;
use euclid::{gcd, lcm};

pub fn main_() -> i64 {
    return gcd(48, 18) * 10 + lcm(4, 6);
}

fn main() {
    std::process::exit(main_() as i32);
}
