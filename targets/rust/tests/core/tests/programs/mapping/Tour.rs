use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

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

#[derive(Clone)]
pub struct Pt {
    pub x: i64,
    pub y: i64,
}

impl PartialEq for Pt {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Pt {
    pub fn new(x: i64, y: i64) -> Ref<Pt> {
        share(Pt { x, y })
    }
}

pub trait PtMethods {
}

impl PtMethods for RefCell<Pt> {
}

#[derive(Clone)]
pub struct Counter {
    pub n: i64,
    pub name: String,
}

impl PartialEq for Counter {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.n == other.n
            && self.name == other.name
    }
}

impl Counter {
    pub fn new(n: i64, name: String) -> Ref<Counter> {
        share(Counter { n, name })
    }
}

pub trait CounterMethods {
}

impl CounterMethods for RefCell<Counter> {
}

pub trait Shape {
    fn area(&self) -> i64;
}

#[derive(Clone)]
pub struct Sq {
    pub side: i64,
}

impl PartialEq for Sq {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.side == other.side
    }
}

impl Sq {
    pub fn new(side: i64) -> Ref<Sq> {
        share(Sq { side })
    }
}

pub trait SqMethods {
}

impl SqMethods for RefCell<Sq> {
}

impl Shape for RefCell<Sq> {
    fn area(&self) -> i64 {
        return self.borrow().side * self.borrow().side;
    }
}

#[derive(Clone)]
pub struct Rect {
    pub w: i64,
    pub h: i64,
}

impl PartialEq for Rect {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.w == other.w
            && self.h == other.h
    }
}

impl Rect {
    pub fn new(w: i64, h: i64) -> Ref<Rect> {
        share(Rect { w, h })
    }
}

pub trait RectMethods {
}

impl RectMethods for RefCell<Rect> {
}

impl Shape for RefCell<Rect> {
    fn area(&self) -> i64 {
        return self.borrow().w * self.borrow().h;
    }
}

pub fn totalArea(shapes: Ref<Vec<Rc<dyn Shape>>>) -> i64 {
    let mut sum: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < (shapes.borrow().len() as i64) {
            sum = sum + shapes.borrow()[i as usize].clone().area();
            i = i + 1;
        }
    }
    return sum;
}

pub fn risky(x: i64) -> i64 {
    if x < 0 {
        throw_code(7);
    }
    return x;
}

pub fn guarded(x: i64) -> i64 {
    let mut out: i64 = 0;
    let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        out = risky(x);
    }));
    if let Err(caught) = tried {
        let code: i64 = thrown_code(caught);
        out = code * 10;
    }
    return out;
}

pub fn main_() -> i64 {
    let mut score: i64 = 0;
    let p: Ref<Pt> = Pt::new(3, 4);
    let q: Ref<Pt> = Pt::new(3, 4);
    if p.clone() == q.clone() {
        score = score + 1;
    }
    let c: Ref<Counter> = Counter::new(5, String::from("hits"));
    let d: Ref<Counter> = c.clone();
    let assigned = d.borrow().n + 1;
    d.borrow_mut().n = assigned;
    if c.clone() == d.clone() {
        score = score + c.borrow().n;
    }
    let shapes: Ref<Vec<Rc<dyn Shape>>> = share(Vec::new());
    { let pushed = Sq::new(3); shapes.borrow_mut().push(pushed) };
    { let pushed = Rect::new(2, 5); shapes.borrow_mut().push(pushed) };
    score = score + totalArea(shapes.clone());
    let calls: Ref<i64> = share(0);
    let calls_moved = calls.clone();
    let tick: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        let assigned = calls_moved.borrow().clone() + 1;
        *calls_moved.borrow_mut() = assigned;
        return calls_moved.borrow().clone();
    }) as Rc<dyn Fn() -> i64>;
    let first: i64 = (tick)();
    let second: i64 = (tick)();
    score = score + first + second + calls.borrow().clone();
    let stock: Ref<HashMap<String, i64>> = share(HashMap::new());
    let stored = 3;
    stock.borrow_mut().insert(String::from("apple"), stored);
    let stored = 4;
    stock.borrow_mut().insert(String::from("pear"), stored);
    score = score + stock.borrow().get(&String::from("apple")).cloned().unwrap_or_else(|| 0) * stock.borrow().get(&String::from("pear")).cloned().unwrap_or_else(|| 0);
    let b: Ref<Box_i64> = Box_i64::new(7);
    score = score + b.borrow().value;
    score = score + guarded(5) + guarded(0 - 1);
    return score;
}

#[derive(Clone)]
pub struct Box_i64 {
    pub value: i64,
}

impl PartialEq for Box_i64 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Box_i64 {
    pub fn new(value: i64) -> Ref<Box_i64> {
        share(Box_i64 { value })
    }
}

pub trait Box_i64Methods {
}

impl Box_i64Methods for RefCell<Box_i64> {
}

fn main() {
    std::process::exit(main_() as i32);
}
