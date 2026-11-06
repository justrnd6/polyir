use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn sumPushed(a: i64, b: i64, c: i64) -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = a; arr.borrow_mut().push(pushed) };
    { let pushed = b; arr.borrow_mut().push(pushed) };
    { let pushed = c; arr.borrow_mut().push(pushed) };
    { let pushed = a + b + c; arr.borrow_mut().push(pushed) };
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone() + arr.borrow()[2 as usize].clone() + arr.borrow()[3 as usize].clone();
}

pub fn lenCheck(a: i64, b: i64, c: i64) -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    { let pushed = a; arr.borrow_mut().push(pushed) };
    { let pushed = b; arr.borrow_mut().push(pushed) };
    { let pushed = c; arr.borrow_mut().push(pushed) };
    return (arr.borrow().len() as i64);
}

pub fn sumSquaresUpTo(n: i64) -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    {
        let mut i: i64 = 0;
        while i < n {
            { let pushed = i * i; arr.borrow_mut().push(pushed) };
            i = i + 1;
        }
    }
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < (arr.borrow().len() as i64) {
            total = total + arr.borrow()[i as usize].clone();
            i = i + 1;
        }
    }
    return total;
}

pub fn sumEvensBelow(n: i64) -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    {
        let mut i: i64 = 0;
        while i < n {
            if i % 2 == 0 {
                { let pushed = i; arr.borrow_mut().push(pushed) };
            }
            i = i + 1;
        }
    }
    let mut total: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < (arr.borrow().len() as i64) {
            total = total + arr.borrow()[i as usize].clone();
            i = i + 1;
        }
    }
    return total;
}

pub fn fillSquares(arr: Ref<Vec<i64>>, count: i64) -> i64 {
    {
        let mut i: i64 = 0;
        while i < count {
            { let pushed = i * i; arr.borrow_mut().push(pushed) };
            i = i + 1;
        }
    }
    return count;
}

pub fn squaresThroughAHelper(count: i64) -> i64 {
    let arr: Ref<Vec<i64>> = share(Vec::new());
    fillSquares(arr.clone(), count);
    return (arr.borrow().len() as i64) * 1000 + arr.borrow()[(count - 1) as usize].clone();
}

pub fn pushGivesTheArrayBack() -> i64 {
    let mut arr: Ref<Vec<i64>> = share(Vec::new());
    arr = { let pushed = 10; arr.borrow_mut().push(pushed); arr.clone() };
    arr = { let pushed = 20; arr.borrow_mut().push(pushed); arr.clone() };
    return arr.borrow()[0 as usize].clone() + arr.borrow()[1 as usize].clone();
}

pub fn sumEvensBelowExplicit(n: i64) -> i64 {
    let mut label: i64 = 0;
    let mut i: i64 = 0;
    let mut arr: Ref<Vec<i64>> = share(Vec::new());
    loop {
        match label {
            0 => {
                let empty: Ref<Vec<i64>> = share(Vec::new());
                i = 0;
                arr = empty.clone();
                label = 1;
                continue;
            }
            1 => {
                if i < n {
                    label = 2;
                    continue;
                } else {
                    label = 5;
                    continue;
                }
            }
            2 => {
                if i % 2 == 0 {
                    label = 3;
                    continue;
                } else {
                    label = 4;
                    continue;
                }
            }
            3 => {
                let mut next_i: i64 = i + 1;
                let mut next_arr: Ref<Vec<i64>> = { let pushed = i; arr.borrow_mut().push(pushed); arr.clone() };
                i = next_i;
                arr = next_arr;
                label = 1;
                continue;
            }
            4 => {
                i = i + 1;
                label = 1;
                continue;
            }
            5 => {
                let mut i: i64 = 0;
                let mut total: i64 = 0;
                while i < (arr.borrow().len() as i64) {
                    i = i + 1;
                    total = total + arr.borrow()[i as usize].clone();
                }
                return total;
            }
            _ => unreachable!(),
        }
    }
}
