pub fn doubleIt(x: i64) -> i64 {
    return x * 2;
}

pub fn applyTwice(f: fn(i64) -> i64, x: i64) -> i64 {
    let once: i64 = (f)(x);
    return (f)(once);
}

pub fn applyDoubleTwice(x: i64) -> i64 {
    return applyTwice(doubleIt, x);
}

pub fn directDoubleCheck(x: i64) -> i64 {
    return doubleIt(x);
}

pub fn addOne(x: i64) -> i64 {
    return x + 1;
}

pub fn pickAndApply(x: i64, useAddOne: i64) -> i64 {
    let mut fn_: fn(i64) -> i64 = doubleIt;
    if useAddOne != 0 {
        fn_ = addOne;
    }
    return (fn_)(x);
}

pub fn applyNTimes(f: fn(i64) -> i64, x: i64, n: i64) -> i64 {
    let mut acc: i64 = x;
    {
        let mut i: i64 = 0;
        while i < n {
            acc = (f)(acc);
            i = i + 1;
        }
    }
    return acc;
}

pub fn doubleFourTimes(x: i64) -> i64 {
    return applyNTimes(doubleIt, x, 4);
}
