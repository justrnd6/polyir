use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

#[derive(Clone)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

impl PartialEq for Point {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Point {
    pub fn new(x: f64, y: f64) -> Ref<Point> {
        share(Point { x, y })
    }
}

pub trait PointMethods {
    fn dist(&self) -> f64;
}

impl PointMethods for RefCell<Point> {
    fn dist(&self) -> f64 {
        return self.borrow().x * self.borrow().x + self.borrow().y * self.borrow().y;
    }
}

#[derive(Clone)]
pub struct Circle {
    pub r: f64,
}

impl PartialEq for Circle {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.r == other.r
    }
}

impl Circle {
    pub fn new(r: f64) -> Ref<Circle> {
        share(Circle { r })
    }
}

pub trait CircleMethods {
    fn dist(&self) -> f64;
}

impl CircleMethods for RefCell<Circle> {
    fn dist(&self) -> f64 {
        return self.borrow().r * self.borrow().r;
    }
}

pub fn distSum(px: f64, py: f64, r: f64) -> f64 {
    let p: Ref<Point> = Point::new(px, py);
    let c: Ref<Circle> = Circle::new(r);
    return p.dist() + c.dist();
}

#[derive(Clone)]
pub struct Pair {
    pub a: i64,
    pub b: i64,
}

impl PartialEq for Pair {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.a == other.a
            && self.b == other.b
    }
}

impl Pair {
    pub fn new(a: i64, b: i64) -> Ref<Pair> {
        share(Pair { a, b })
    }
}

pub trait PairMethods {
    fn sum(&self) -> i64;
    fn doubledSum(&self) -> i64;
}

impl PairMethods for RefCell<Pair> {
    fn sum(&self) -> i64 {
        return self.borrow().a + self.borrow().b;
    }

    fn doubledSum(&self) -> i64 {
        return self.sum() * 2;
    }
}

pub fn doubledSumCheck(a: i64, b: i64) -> i64 {
    let p: Ref<Pair> = Pair::new(a, b);
    return p.doubledSum();
}
