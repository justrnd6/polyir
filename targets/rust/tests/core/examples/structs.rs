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
}

impl PointMethods for RefCell<Point> {
}

pub fn distanceSquared(a: Ref<Point>, b: Ref<Point>) -> f64 {
    let dx: f64 = a.borrow().x - b.borrow().x;
    let dy: f64 = a.borrow().y - b.borrow().y;
    return dx * dx + dy * dy;
}

pub fn distFromOrigin(bx: f64, by: f64) -> f64 {
    return distanceSquared(Point::new(0.0, 0.0), Point::new(bx, by));
}
