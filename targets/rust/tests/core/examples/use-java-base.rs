pub trait Runnable {
    fn run(&self) -> i64;
}

pub fn main_() -> i64 {
    return 0;
}

fn main() {
    std::process::exit(main_() as i32);
}
