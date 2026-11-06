pub fn sumTo(n: i64) -> i64 {
    let mut label: i64 = 0;
    let mut total: i64 = 0;
    let mut i: i64 = 0;
    let mut result: i64 = 0;
    loop {
        match label {
            0 => {
                total = 0;
                i = 1;
                label = 1;
                continue;
            }
            1 => {
                if i > n {
                    result = total;
                    label = 3;
                    continue;
                } else {
                    label = 2;
                    continue;
                }
            }
            2 => {
                total = total + i;
                i = i + 1;
                label = 1;
                continue;
            }
            3 => {
                return result;
            }
            _ => unreachable!(),
        }
    }
}

pub fn gcd(a0: i64, b0: i64) -> i64 {
    let mut label: i64 = 0;
    let mut a: i64 = 0;
    let mut b: i64 = 0;
    let mut result: i64 = 0;
    loop {
        match label {
            0 => {
                a = a0;
                b = b0;
                label = 1;
                continue;
            }
            1 => {
                if b == 0 {
                    result = a;
                    label = 3;
                    continue;
                } else {
                    label = 2;
                    continue;
                }
            }
            2 => {
                let r: i64 = a % b;
                a = b;
                b = r;
                label = 1;
                continue;
            }
            3 => {
                return result;
            }
            _ => unreachable!(),
        }
    }
}

pub fn collatz(n: i64) -> i64 {
    let mut label: i64 = 0;
    let mut x: i64 = 0;
    let mut steps: i64 = 0;
    let mut count: i64 = 0;
    loop {
        match label {
            0 => {
                x = n;
                steps = 0;
                label = 1;
                continue;
            }
            1 => {
                if x == 1 {
                    count = steps;
                    label = 5;
                    continue;
                } else {
                    label = 2;
                    continue;
                }
            }
            2 => {
                if x % 2 == 0 {
                    label = 3;
                    continue;
                } else {
                    label = 4;
                    continue;
                }
            }
            3 => {
                x = x / 2;
                steps = steps + 1;
                label = 1;
                continue;
            }
            4 => {
                x = 3 * x + 1;
                steps = steps + 1;
                label = 1;
                continue;
            }
            5 => {
                return count;
            }
            _ => unreachable!(),
        }
    }
}

pub fn swapped(a: i64, b: i64) -> i64 {
    let mut label: i64 = 0;
    let mut x: i64 = 0;
    let mut y: i64 = 0;
    let mut k: i64 = 0;
    loop {
        match label {
            0 => {
                x = a;
                y = b;
                k = 0;
                label = 1;
                continue;
            }
            1 => {
                if k == 3 {
                    label = 3;
                    continue;
                } else {
                    label = 2;
                    continue;
                }
            }
            2 => {
                let mut next_x: i64 = y;
                let mut next_y: i64 = x;
                let mut next_k: i64 = k + 1;
                x = next_x;
                y = next_y;
                k = next_k;
                label = 1;
                continue;
            }
            3 => {
                return x * 10 + y;
            }
            _ => unreachable!(),
        }
    }
}

pub fn weight(day: i64) -> i64 {
    let mut label: i64 = 0;
    let mut w: i64 = 0;
    loop {
        match label {
            0 => {
                if day == 0 {
                    label = 1;
                    continue;
                } else if day == 6 {
                    label = 1;
                    continue;
                } else if day == 1 {
                    label = 2;
                    continue;
                } else if day == 2 {
                    label = 2;
                    continue;
                } else {
                    label = 3;
                    continue;
                }
            }
            1 => {
                w = 1;
                label = 4;
                continue;
            }
            2 => {
                w = 2;
                label = 4;
                continue;
            }
            3 => {
                w = 3;
                label = 4;
                continue;
            }
            4 => {
                return w * 100;
            }
            _ => unreachable!(),
        }
    }
}

pub fn zigzag(a: i64) -> i64 {
    let mut label: i64 = 0;
    let mut p: i64 = 0;
    let mut q: i64 = 0;
    let mut r: i64 = 0;
    loop {
        match label {
            0 => {
                if a == 0 {
                    p = a;
                    label = 1;
                    continue;
                } else {
                    q = a;
                    label = 2;
                    continue;
                }
            }
            1 => {
                let next: i64 = p + 1;
                if next > 5 {
                    r = next;
                    label = 3;
                    continue;
                } else {
                    q = next;
                    label = 2;
                    continue;
                }
            }
            2 => {
                if q > 5 {
                    r = q;
                    label = 3;
                    continue;
                } else {
                    p = q + 1;
                    label = 1;
                    continue;
                }
            }
            3 => {
                return r * 2;
            }
            _ => unreachable!(),
        }
    }
}

pub fn main_() -> i64 {
    let mut score: i64 = sumTo(10);
    score = score + gcd(48, 18);
    score = score + collatz(6);
    score = score + swapped(4, 7);
    score = score + weight(0) + weight(2) + weight(9);
    score = score + zigzag(0) + zigzag(3);
    return score;
}

fn main() {
    std::process::exit(main_() as i32);
}
