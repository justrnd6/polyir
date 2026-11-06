use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
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
pub struct Seg {
    pub a: Ref<Pt>,
    pub b: Ref<Pt>,
    pub tag: i64,
}

impl PartialEq for Seg {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.a == other.a
            && self.b == other.b
            && self.tag == other.tag
    }
}

impl Seg {
    pub fn new(a: Ref<Pt>, b: Ref<Pt>, tag: i64) -> Ref<Seg> {
        share(Seg { a, b, tag })
    }
}

pub trait SegMethods {
}

impl SegMethods for RefCell<Seg> {
}

#[derive(Clone)]
pub struct Counter {
    pub hits: i64,
    pub name: String,
}

impl PartialEq for Counter {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.hits == other.hits
            && self.name == other.name
    }
}

impl Counter {
    pub fn new(hits: i64, name: String) -> Ref<Counter> {
        share(Counter { hits, name })
    }
}

pub trait CounterMethods {
}

impl CounterMethods for RefCell<Counter> {
}

pub fn measure(s: Ref<Seg>, c: Ref<Counter>, word: String) -> i64 {
    let dx: i64 = s.borrow().b.clone().borrow().x - s.borrow().a.clone().borrow().x;
    let xs: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = dx; xs.borrow_mut().push(pushed) };
    { let pushed = 7; xs.borrow_mut().push(pushed) };
    let cells: Ref<Vec<i64>> = share(vec![0; 3 as usize]);
    let assigned = c.borrow().hits + dx;
    c.borrow_mut().hits = assigned;
    return (xs.borrow().len() as i64) + c.borrow().hits;
}

pub fn main_() -> i64 {
    let seg: Ref<Seg> = Seg::new(Pt::new(1, 2), Pt::new(4, 6), 9);
    let c: Ref<Counter> = Counter::new(0, String::from("ctr"));
    return measure(seg.clone(), c.clone(), String::from("hello"));
}

fn main() {
    std::process::exit(main_() as i32);
}
