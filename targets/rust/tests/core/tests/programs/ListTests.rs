use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn offText(got: String, want: String) -> f64 {
    if got.clone() == want.clone() {
        return 0.0;
    }
    return 1.0;
}

pub fn checkFirstAndLast() -> f64 {
    let xs: Ref<Vec<i64>> = share(vec![3, 1, 4]);
    let words: Ref<Vec<String>> = share(vec![String::from("a"), String::from("b")]);
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", first_i64(xs.clone()).to_string(), String::from(" ")), last_i64(xs.clone()).to_string()), String::from(" ")), first_string(words.clone())), last_string(words.clone())), String::from("3 4 ab"));
}

pub fn checkDrop() -> f64 {
    let xs: Ref<Vec<i64>> = share(vec![1, 2, 3]);
    let rest: Ref<Vec<i64>> = drop_i64(xs.clone(), 1);
    let none: Ref<Vec<i64>> = drop_i64(xs.clone(), 5);
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", (rest.borrow().len() as i64).to_string(), String::from(" ")), rest.borrow()[0 as usize].clone().to_string()), String::from(" ")), rest.borrow()[1 as usize].clone().to_string()), String::from(" ")), (none.borrow().len() as i64).to_string()), String::from("2 2 3 0"));
}

pub fn first_i64(xs: Ref<Vec<i64>>) -> i64 {
    return xs.borrow()[0 as usize].clone();
}

pub fn last_i64(xs: Ref<Vec<i64>>) -> i64 {
    return xs.borrow()[((xs.borrow().len() as i64) - 1) as usize].clone();
}

pub fn first_string(xs: Ref<Vec<String>>) -> String {
    return xs.borrow()[0 as usize].clone();
}

pub fn last_string(xs: Ref<Vec<String>>) -> String {
    return xs.borrow()[((xs.borrow().len() as i64) - 1) as usize].clone();
}

pub fn drop_i64(xs: Ref<Vec<i64>>, n: i64) -> Ref<Vec<i64>> {
    let out: Ref<Vec<i64>> = share(Vec::new());
    {
        let mut i: i64 = n;
        while i < (xs.borrow().len() as i64) {
            { let pushed = xs.borrow()[i as usize].clone(); out.borrow_mut().push(pushed) };
            i = i + 1;
        }
    }
    return out.clone();
}
