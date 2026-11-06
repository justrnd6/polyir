use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn queens(t: i32, prefixes: Ref<Vec<i32>>, p: Ref<Vec<i32>>, stack: Ref<Vec<i32>>) -> i32 {
    let n: i32 = p.borrow()[0 as usize].clone();
    let depth: i32 = p.borrow()[1 as usize].clone();
    let full: i32 = (((1 << (n as i64)) - 1) as i32);
    let base: i32 = t * (n + 1) * 4;
    let stored = prefixes.borrow()[(t * 3) as usize].clone();
    stack.borrow_mut()[base as usize] = stored;
    let stored = prefixes.borrow()[(t * 3 + 1) as usize].clone();
    stack.borrow_mut()[(base + 1) as usize] = stored;
    let stored = prefixes.borrow()[(t * 3 + 2) as usize].clone();
    stack.borrow_mut()[(base + 2) as usize] = stored;
    let stored = full & !(prefixes.borrow()[(t * 3) as usize].clone() | prefixes.borrow()[(t * 3 + 1) as usize].clone() | prefixes.borrow()[(t * 3 + 2) as usize].clone());
    stack.borrow_mut()[(base + 3) as usize] = stored;
    let mut level: i32 = 0;
    let mut count: i32 = 0;
    {
        let mut step: i64 = 0;
        while level >= 0 {
            let at: i32 = base + level * 4;
            let free: i32 = stack.borrow()[(at + 3) as usize].clone();
            if free == 0 {
                level = level - 1;
            } else {
                let bit: i32 = free & ((0 - (free as i64)) as i32);
                let stored = free ^ bit;
                stack.borrow_mut()[(at + 3) as usize] = stored;
                if depth + level + 1 == n {
                    count = count + 1;
                } else {
                    let nextCols: i32 = stack.borrow()[at as usize].clone() | bit;
                    let nextLeft: i32 = (stack.borrow()[(at + 1) as usize].clone() | bit) << 1 & full;
                    let nextRight: i32 = (stack.borrow()[(at + 2) as usize].clone() | bit) >> 1;
                    level = level + 1;
                    let below: i32 = base + level * 4;
                    let stored = nextCols;
                    stack.borrow_mut()[below as usize] = stored;
                    let stored = nextLeft;
                    stack.borrow_mut()[(below + 1) as usize] = stored;
                    let stored = nextRight;
                    stack.borrow_mut()[(below + 2) as usize] = stored;
                    let stored = full & !(nextCols | nextLeft | nextRight);
                    stack.borrow_mut()[(below + 3) as usize] = stored;
                }
            }
            step = step + 1;
        }
    }
    return count;
}

pub fn listPrefixes(n: i64, depth: i64, out: Ref<Vec<i32>>) -> i64 {
    let mut total: i64 = 1;
    let mut r: i64 = 0;
    while r < depth {
        total = total * n;
        r = r + 1;
    }
    let full: i32 = (((1 << n) - 1) as i32);
    let mut found: i64 = 0;
    {
        let mut code: i64 = 0;
        while code < total {
            let mut left: i32 = 0;
            let mut right: i32 = 0;
            let mut cols: i32 = 0;
            let mut ok: i64 = 1;
            let mut rest: i64 = code;
            {
                let mut r: i64 = 0;
                while r < depth {
                    let column: i64 = rest % n;
                    rest = rest / n;
                    let bit: i32 = ((1 << column) as i32);
                    if bit & (cols | left | right) != 0 {
                        ok = 0;
                    }
                    cols = cols | bit;
                    left = (left | bit) << 1 & full;
                    right = (right | bit) >> 1;
                    r = r + 1;
                }
            }
            if ok == 1 {
                let stored = cols;
                out.borrow_mut()[(found * 3) as usize] = stored;
                let stored = left;
                out.borrow_mut()[(found * 3 + 1) as usize] = stored;
                let stored = right;
                out.borrow_mut()[(found * 3 + 2) as usize] = stored;
                found = found + 1;
            }
            code = code + 1;
        }
    }
    return found;
}
