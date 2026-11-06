use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;
use std::collections::HashSet;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

thread_local! { static visits: RefCell<i64> = RefCell::new(0); }

#[derive(Clone)]
pub struct Account {
    pub balance: i64,
    pub owner: String,
}

impl PartialEq for Account {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.balance == other.balance
            && self.owner == other.owner
    }
}

impl Account {
    pub fn new(balance: i64, owner: String) -> Ref<Account> {
        share(Account { balance, owner })
    }
}

pub trait AccountMethods {
    fn deposit(&self, amount: i64) -> i64;
    fn depositTwice(&self, amount: i64) -> i64;
}

impl AccountMethods for RefCell<Account> {
    fn deposit(&self, amount: i64) -> i64 {
        let assigned = self.borrow().balance + amount;
        self.borrow_mut().balance = assigned;
        return self.borrow().balance;
    }

    fn depositTwice(&self, amount: i64) -> i64 {
        self.deposit(amount);
        return self.deposit(amount);
    }
}

#[derive(Clone)]
pub struct Vec2 {
    pub x: f64,
    pub y: f64,
}

impl PartialEq for Vec2 {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.x == other.x
            && self.y == other.y
    }
}

impl Vec2 {
    pub fn new(x: f64, y: f64) -> Ref<Vec2> {
        share(Vec2 { x, y })
    }
}

pub trait Vec2Methods {
    fn length2(&self) -> f64;
    fn scaled(&self, k: f64) -> Ref<Vec2>;
}

impl Vec2Methods for RefCell<Vec2> {
    fn length2(&self) -> f64 {
        return self.borrow().x * self.borrow().x + self.borrow().y * self.borrow().y;
    }

    fn scaled(&self, k: f64) -> Ref<Vec2> {
        return Vec2::new(self.borrow().x * k, self.borrow().y * k);
    }
}

pub fn fib(n: i64) -> i64 {
    return if n < 2 { n } else { fib(n - 1) + fib(n - 2) };
}

pub fn classify(n: i64) -> i64 {
    return if n < 0 { 1 } else { if n == 0 { 2 } else { if n < 10 { 3 } else { 4 } } };
}

pub fn bump() -> i64 {
    visits.with(|cell| *cell.borrow_mut() = visits.with(|cell| cell.borrow().clone()) + 1);
    return visits.with(|cell| cell.borrow().clone());
}

pub fn greeting(name: String) -> String {
    return format!("{}{}", String::from("hello, "), name.clone());
}

pub fn narrow(a: i32, b: i32) -> i32 {
    return a * b + 7;
}

pub fn apply(f: Rc<dyn Fn(i64) -> i64>, x: i64) -> i64 {
    return (f)(x) + (f)(x + 1);
}

pub fn main_() -> i64 {
    let mut score: i64 = 0;
    score = score + fib(10);
    let acct: Ref<Account> = Account::new(100, String::from("ann"));
    score = score + acct.depositTwice(5);
    let v: Ref<Vec2> = Vec2::new(3.0, 4.0);
    let w: Ref<Vec2> = v.scaled(2.0);
    score = score + (w.length2() as i64);
    if greeting(String::from("bo")) == String::from("hello, bo") {
        score = score + 1;
    }
    score = score + (narrow(6, 7) as i64);
    let seen: Ref<HashSet<i64>> = share(HashSet::new());
    {
        let mut i: i64 = 0;
        while i < 20 {
            seen.borrow_mut().insert(i % 7);
            i = i + 1;
        }
    }
    score = score + (seen.borrow().len() as i64);
    let nums: Ref<Vec<i64>> = share(Vec::new());
    {
        let mut i: i64 = 0;
        let mut j: i64 = 10;
        while i < 5 {
            { let pushed = i * j; nums.borrow_mut().push(pushed) };
            i = i + 1;
            j = j - 1;
        }
    }
    let stored = 100;
    nums.borrow_mut()[0 as usize] = stored;
    score = score + nums.borrow()[0 as usize].clone() + nums.borrow()[4 as usize].clone() + (nums.borrow().len() as i64);
    let ages: Ref<HashMap<String, i64>> = share(HashMap::new());
    let stored = 31;
    ages.borrow_mut().insert(String::from("ann"), stored);
    if ages.borrow().contains_key(&String::from("ann")) {
        score = score + ages.borrow().get(&String::from("ann")).cloned().unwrap_or_else(|| 0);
    }
    if ages.borrow().contains_key(&String::from("zed")) == false {
        score = score + 1;
    }
    let factor: i64 = 3;
    let factor_moved = factor.clone();
    score = score + apply(Rc::new(move |x: i64| -> i64 {
        return x * factor_moved;
    }) as Rc<dyn Fn(i64) -> i64>, 4);
    let b: Ref<Box_double> = Box_double::new(2.5);
    score = score + ((b.borrow().value * 4.0) as i64);
    score = score + classify(0 - 5) + classify(0) + classify(7) + classify(70);
    score = score + bump() + bump();
    return score;
}

#[derive(Clone)]
pub struct Box_double {
    pub value: f64,
}

impl PartialEq for Box_double {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.value == other.value
    }
}

impl Box_double {
    pub fn new(value: f64) -> Ref<Box_double> {
        share(Box_double { value })
    }
}

pub trait Box_doubleMethods {
}

impl Box_doubleMethods for RefCell<Box_double> {
}

fn main() {
    std::process::exit(main_() as i32);
}
