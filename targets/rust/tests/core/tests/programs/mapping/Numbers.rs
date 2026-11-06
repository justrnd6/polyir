pub fn mix(h: i64, v: i64) -> i64 {
    return (h * 31 + v) % 1000003;
}

pub fn wide() -> i64 {
    let big: i64 = 9223372036854775807;
    let wrapped: i64 = big + 1;
    let product: i64 = 3037000500 * 3037000500;
    let neg: i64 = 0 - big;
    let quotient: i64 = (0 - 7) / 2;
    let rest: i64 = (0 - 7) % 3;
    let mut h: i64 = 7;
    h = mix(h, (wrapped >> 40) % 1000);
    h = mix(h, (product >> 33) % 1000);
    h = mix(h, (neg >> 50) % 1000);
    h = mix(h, quotient + 100);
    h = mix(h, rest + 100);
    h = mix(h, big << 3 >> 60);
    h = mix(h, 0 - 1 >> 5 + 100);
    return h;
}

pub fn thirtyTwo() -> i64 {
    let big: i32 = 2147483647;
    let wrapped: i32 = big + 1;
    let product: i32 = ((65536 * 65536) as i32);
    let other_: i32 = ((123456789 * 987) as i32);
    let quotient: i32 = (((0 - 7) / 2) as i32);
    let rest: i32 = (((0 - 7) % 3) as i32);
    let mut h: i64 = 11;
    h = mix(h, (wrapped as i64) % 100003);
    h = mix(h, (product as i64) + 5);
    h = mix(h, (other_ as i64) % 100003);
    h = mix(h, (quotient as i64) + 100);
    h = mix(h, (rest as i64) + 100);
    h = mix(h, ((wrapped >> 7) as i64) % 100003);
    h = mix(h, ((big << 5) as i64) % 100003);
    return h;
}

pub fn unsigned32() -> i64 {
    let top: u32 = 4294967295;
    let wrapped: u32 = top + 2;
    let product: u32 = ((4000000000 * 7) as u32);
    let under: u32 = ((3 - 5) as u32);
    let shifted: u32 = top << 4;
    let mut h: i64 = 13;
    h = mix(h, (wrapped as i64));
    h = mix(h, (product as i64) % 100003);
    h = mix(h, (under as i64) % 100003);
    h = mix(h, (shifted as i64) % 100003);
    h = mix(h, ((top >> 3) as i64) % 100003);
    h = mix(h, ((top / 3) as i64) % 100003);
    h = mix(h, ((top % 1000) as i64));
    return h;
}

pub fn narrow() -> i64 {
    let a: i8 = 127;
    let b: i8 = a + 1;
    let c: i16 = 32767;
    let d: i16 = c + 1;
    let e: u8 = 250;
    let f: u8 = e + 10;
    let g: u16 = 65535;
    let hh: u16 = g + 3;
    let m: i8 = ((0 - 128) as i8);
    let n: i8 = m / ((0 - 1) as i8);
    let mut h: i64 = 17;
    h = mix(h, (b as i64) + 200);
    h = mix(h, (d as i64) + 40000);
    h = mix(h, (f as i64));
    h = mix(h, (hh as i64));
    h = mix(h, (n as i64) + 200);
    h = mix(h, ((b >> 2) as i64) + 200);
    h = mix(h, ((e << 3) as i64));
    return h;
}

pub fn casts() -> i64 {
    let big: i64 = 5000000000;
    let cut: i32 = (big as i32);
    let tiny: i8 = (300 as i8);
    let uns: u32 = ((0 - 5) as u32);
    let up: u64 = ((0 - 1) as u64);
    let back: i64 = (up as i64);
    let f: f64 = 3.99;
    let g: f64 = 0.0 - 3.99;
    let mut h: i64 = 19;
    h = mix(h, (cut as i64) % 100003);
    h = mix(h, (tiny as i64) + 200);
    h = mix(h, (uns as i64) % 100003);
    h = mix(h, ((up >> 60) as i64));
    h = mix(h, back + 5);
    h = mix(h, (f as i64));
    h = mix(h, (g as i64) + 10);
    h = mix(h, ((f * 2.5) as i64));
    return h;
}

pub fn main_() -> i64 {
    let mut h: i64 = 1;
    h = mix(h, wide());
    h = mix(h, thirtyTwo());
    h = mix(h, unsigned32());
    h = mix(h, narrow());
    h = mix(h, casts());
    return h % 251;
}

fn main() {
    std::process::exit(main_() as i32);
}
