use std::rc::Rc;
use std::cell::RefCell;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn transform(p: [f32; 4], m: Ref<Vec<f32>>) -> [f32; 4] {
    return [m.borrow()[0 as usize].clone() * p[0 as usize] + m.borrow()[1 as usize].clone() * p[1 as usize] + m.borrow()[2 as usize].clone() * p[2 as usize] + m.borrow()[3 as usize].clone() * p[3 as usize], m.borrow()[4 as usize].clone() * p[0 as usize] + m.borrow()[5 as usize].clone() * p[1 as usize] + m.borrow()[6 as usize].clone() * p[2 as usize] + m.borrow()[7 as usize].clone() * p[3 as usize], m.borrow()[8 as usize].clone() * p[0 as usize] + m.borrow()[9 as usize].clone() * p[1 as usize] + m.borrow()[10 as usize].clone() * p[2 as usize] + m.borrow()[11 as usize].clone() * p[3 as usize], m.borrow()[12 as usize].clone() * p[0 as usize] + m.borrow()[13 as usize].clone() * p[1 as usize] + m.borrow()[14 as usize].clone() * p[2 as usize] + m.borrow()[15 as usize].clone() * p[3 as usize]];
}

pub fn identity(m: Ref<Vec<f32>>) -> i64 {
    {
        let mut k: i64 = 0;
        while k < 16 {
            let stored = 0.0f32;
            m.borrow_mut()[k as usize] = stored;
            k = k + 1;
        }
    }
    let stored = 1.0f32;
    m.borrow_mut()[0 as usize] = stored;
    let stored = 1.0f32;
    m.borrow_mut()[5 as usize] = stored;
    let stored = 1.0f32;
    m.borrow_mut()[10 as usize] = stored;
    let stored = 1.0f32;
    m.borrow_mut()[15 as usize] = stored;
    return 0;
}

pub fn multiply4(a: Ref<Vec<f32>>, b: Ref<Vec<f32>>, out: Ref<Vec<f32>>) -> i64 {
    {
        let mut row: i64 = 0;
        while row < 4 {
            {
                let mut col: i64 = 0;
                while col < 4 {
                    let mut sum: f32 = 0.0f32;
                    {
                        let mut k: i64 = 0;
                        while k < 4 {
                            sum = sum + a.borrow()[(row * 4 + k) as usize].clone() * b.borrow()[(k * 4 + col) as usize].clone();
                            k = k + 1;
                        }
                    }
                    let stored = sum;
                    out.borrow_mut()[(row * 4 + col) as usize] = stored;
                    col = col + 1;
                }
            }
            row = row + 1;
        }
    }
    return 0;
}
