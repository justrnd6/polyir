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
