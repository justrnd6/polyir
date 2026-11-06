use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub trait Runnable {
    fn run(&self) -> i64;
}

#[derive(Clone)]
pub struct Job {
    pub id: i64,
}

impl PartialEq for Job {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.id == other.id
    }
}

impl Job {
    pub fn new(id: i64) -> Ref<Job> {
        share(Job { id })
    }
}

pub trait JobMethods {
}

impl JobMethods for RefCell<Job> {
}

impl Runnable for RefCell<Job> {
    fn run(&self) -> i64 {
        return self.borrow().id;
    }
}

pub fn main_() -> i64 {
    return 0;
}

fn main() {
    std::process::exit(main_() as i32);
}
