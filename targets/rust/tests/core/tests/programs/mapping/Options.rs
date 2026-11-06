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
pub struct Contact {
    pub name: String,
    pub nickname: Option<String>,
}

impl PartialEq for Contact {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.name == other.name
            && self.nickname == other.nickname
    }
}

impl Contact {
    pub fn new(name: String, nickname: Option<String>) -> Ref<Contact> {
        share(Contact { name, nickname })
    }
}

pub trait ContactMethods {
}

impl ContactMethods for RefCell<Contact> {
}

pub fn find(xs: Ref<Vec<i64>>, want: i64) -> Option<i64> {
    {
        let mut i: i64 = 0;
        while i < (xs.borrow().len() as i64) {
            if xs.borrow()[i as usize].clone() == want {
                return Some(i);
            }
            i = i + 1;
        }
    }
    return None;
}

pub fn firstPoint(ok: i64) -> Option<Ref<Pt>> {
    if ok == 1 {
        return Some(Pt::new(7, 8));
    }
    return None;
}

pub fn shown(c: Ref<Contact>) -> String {
    if c.borrow().nickname.clone().is_some() {
        return c.borrow().nickname.clone().clone().unwrap();
    }
    return c.borrow().name.clone();
}

pub fn main_() -> i64 {
    let xs: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = 10; xs.borrow_mut().push(pushed) };
    { let pushed = 20; xs.borrow_mut().push(pushed) };
    let mut score: i64 = 0;
    let hit: Option<i64> = find(xs.clone(), 20);
    if hit.is_some() {
        score = score + hit.clone().unwrap();
    }
    if find(xs.clone(), 99).is_none() {
        score = score + 10;
    }
    let p: Option<Ref<Pt>> = firstPoint(1);
    if p.is_some() {
        score = score + p.clone().unwrap().borrow().x;
    }
    if firstPoint(0).is_none() {
        score = score + 100;
    }
    let named: Ref<Contact> = Contact::new(String::from("ann"), Some(String::from("annie")));
    let plain: Ref<Contact> = Contact::new(String::from("bo"), None);
    if shown(named.clone()) == String::from("annie") {
        score = score + 1000;
    }
    if shown(plain.clone()) == String::from("bo") {
        score = score + 2000;
    }
    return score;
}

fn main() {
    std::process::exit(main_() as i32);
}
