use std::rc::Rc;
use std::cell::RefCell;

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
pub struct Acc {
    pub total: i64,
    pub tag: String,
    pub items_: Ref<Vec<i64>>,
}

impl PartialEq for Acc {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.total == other.total
            && self.tag == other.tag
            && self.items_ == other.items_
    }
}

impl Acc {
    pub fn new(total: i64, tag: String, items_: Ref<Vec<i64>>) -> Ref<Acc> {
        share(Acc { total, tag, items_ })
    }
}

pub trait AccMethods {
    fn add(&self, x: i64) -> i64;
    fn later(&self) -> Rc<dyn Fn() -> i64>;
    fn describe(&self) -> String;
    fn self_(&self) -> Ref<Acc>;
}

impl AccMethods for RefCell<Acc> {
    fn add(&self, x: i64) -> i64 {
        let assigned = self.borrow().total + x;
        self.borrow_mut().total = assigned;
        { let pushed = x; self.borrow().items_.clone().borrow_mut().push(pushed) };
        return self.borrow().total;
    }

    fn later(&self) -> Rc<dyn Fn() -> i64> {
        let this__moved = self.clone();
        return Rc::new(move || -> i64 {
            return self.borrow().total * 2;
        }) as Rc<dyn Fn() -> i64>;
    }

    fn describe(&self) -> String {
        return format!("{}{}", format!("{}{}", self.borrow().tag.clone(), String::from(":")), self.borrow().tag.clone());
    }

    fn self_(&self) -> Ref<Acc> {
        return self;
    }
}

pub trait Animal {
    fn legs(&self, p0: i64) -> i64;
    fn name(&self) -> String;
}

#[derive(Clone)]
pub struct Dog {
    pub age: i64,
}

impl PartialEq for Dog {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.age == other.age
    }
}

impl Dog {
    pub fn new(age: i64) -> Ref<Dog> {
        share(Dog { age })
    }
}

pub trait DogMethods {
}

impl DogMethods for RefCell<Dog> {
}

impl Animal for RefCell<Dog> {
    fn legs(&self, n: i64) -> i64 {
        return 4 * n + self.borrow().age;
    }

    fn name(&self) -> String {
        return String::from("dog");
    }
}

#[derive(Clone)]
pub struct Bird {
    pub age: i64,
}

impl PartialEq for Bird {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.age == other.age
    }
}

impl Bird {
    pub fn new(age: i64) -> Ref<Bird> {
        share(Bird { age })
    }
}

pub trait BirdMethods {
}

impl BirdMethods for RefCell<Bird> {
}

impl Animal for RefCell<Bird> {
    fn legs(&self, n: i64) -> i64 {
        return 2 * n;
    }

    fn name(&self) -> String {
        return String::from("bird");
    }
}

pub fn risky(x: i64) -> i64 {
    if x < 0 {
        throw_code(7);
    }
    return x;
}

pub fn tryReturn(x: i64) -> i64 {
    let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        return risky(x) + 1;
    }));
    if let Err(caught) = tried {
        let code: i64 = thrown_code(caught);
        return code * 100;
    }
    return 0;
}

pub fn nested(x: i64) -> i64 {
    let mut r: i64 = 0;
    let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let tried = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            r = risky(x);
        }));
        if let Err(caught) = tried {
            let a: i64 = thrown_code(caught);
            r = a + 1;
            throw_code(r);
        }
    }));
    if let Err(caught) = tried {
        let b: i64 = thrown_code(caught);
        r = b * 10;
    }
    return r;
}

pub fn makeAdder(n: i64) -> Rc<dyn Fn(i64) -> i64> {
    let n_moved = n.clone();
    return Rc::new(move |x: i64| -> i64 {
        return x + n_moved;
    }) as Rc<dyn Fn(i64) -> i64>;
}

pub fn twice(f: Rc<dyn Fn(i64) -> i64>, x: i64) -> i64 {
    return (f)((f)(x));
}

pub fn counterPair() -> i64 {
    let c: Ref<i64> = share(0);
    let c_moved = c.clone();
    let inc: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        let assigned = c_moved.borrow().clone() + 1;
        *c_moved.borrow_mut() = assigned;
        return c_moved.borrow().clone();
    }) as Rc<dyn Fn() -> i64>;
    let c_moved = c.clone();
    let get: Rc<dyn Fn() -> i64> = Rc::new(move || -> i64 {
        return c_moved.borrow().clone();
    }) as Rc<dyn Fn() -> i64>;
    (inc)();
    (inc)();
    return (get)() * 10 + c.borrow().clone();
}

pub fn main_() -> i64 {
    let mut score: i64 = 0;
    let a: Ref<Acc> = Acc::new(1, String::from("ab"), share(Vec::new()));
    score = score + a.add(5) + a.add(6);
    let f: Rc<dyn Fn() -> i64> = a.later();
    score = score + (f)();
    score = score + (a.borrow().items_.clone().borrow().len() as i64);
    if a.describe() == String::from("ab:ab") {
        score = score + 1000;
    }
    let s: Ref<Acc> = a.self_();
    if s.clone() == a.clone() {
        score = score + 2000;
    }
    let assigned = String::from("zz");
    a.borrow_mut().tag = assigned;
    if s.borrow().tag.clone() == String::from("zz") {
        score = score + 3000;
    }
    let zoo: Ref<Vec<Rc<dyn Animal>>> = share(Vec::new());
    { let pushed = Dog::new(3); zoo.borrow_mut().push(pushed) };
    { let pushed = Bird::new(1); zoo.borrow_mut().push(pushed) };
    {
        let mut i: i64 = 0;
        while i < (zoo.borrow().len() as i64) {
            score = score + zoo.borrow()[i as usize].clone().legs(2);
            if zoo.borrow()[i as usize].clone().name() == String::from("bird") {
                score = score + 7;
            }
            i = i + 1;
        }
    }
    score = score + tryReturn(3) + tryReturn(0 - 1);
    score = score + nested(4) + nested(0 - 2);
    let add3: Rc<dyn Fn(i64) -> i64> = makeAdder(3);
    score = score + twice(add3.clone(), 10);
    score = score + counterPair();
    return score % 251;
}

fn main() {
    std::process::exit(main_() as i32);
}
