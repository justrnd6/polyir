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
