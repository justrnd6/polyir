pub fn vecMulSum(a: f64, b: f64) -> f64 {
    let v1: [f64; 4] = [a, a + 1.0, a + 2.0, a + 3.0];
    let v2: [f64; 4] = [b, b, b, b];
    let p: [f64; 4] = v1.clone() * v2.clone();
    return p[0 as usize] + p[1 as usize] + p[2 as usize] + p[3 as usize];
}

pub fn sumI64Vec(a: i64, b: i64, c: i64) -> i64 {
    let v: [i64; 3] = [a, b, c];
    return v[0 as usize] + v[1 as usize] + v[2 as usize];
}

pub fn vecMulSumViaVar(a: f64, b: f64) -> f64 {
    let v1: [f64; 4] = [a, a + 1.0, a + 2.0, a + 3.0];
    let v2: [f64; 4] = [b, b, b, b];
    let p: [f64; 4] = v1.clone() * v2.clone();
    let mut total: f64 = 0.0;
    {
        let mut i: i64 = 0;
        while i < 4 {
            total = total + p[i as usize];
            i = i + 1;
        }
    }
    return total;
}
