// What `throw` throws: the code, carried by a panic that a `try` catches.
pub struct Thrown(pub i64);

pub fn throw_code(code: i64) -> ! {
    std::panic::panic_any(Thrown(code))
}

// The code a caught panic carries; a panic polyir did not throw goes on.
pub fn thrown_code(caught: Box<dyn std::any::Any + Send>) -> i64 {
    match caught.downcast::<Thrown>() {
        Ok(thrown) => thrown.0,
        Err(other) => std::panic::resume_unwind(other),
    }
}

pub fn riskyDouble(x: i64) -> i64 {
    if x < 0 {
        throw_code(99);
    }
    return x * 2;
}

pub fn safeDouble(x: i64) -> i64 {
    let mut result: i64 = 0;
    let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        result = riskyDouble(x);
    }));
    if let Err(caught) = tried {
        let code: i64 = thrown_code(caught);
        result = code;
    }
    return result;
}

pub fn nestedTryCheck(x: i64) -> i64 {
    let mut result: i64 = 0;
    let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            result = riskyDouble(x);
        }));
        if let Err(caught) = tried {
            let innerCode: i64 = thrown_code(caught);
            result = innerCode + 1;
            throw_code(result);
        }
    }));
    if let Err(caught) = tried {
        let outerCode: i64 = thrown_code(caught);
        result = outerCode * 10;
    }
    return result;
}
