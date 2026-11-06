use std::rc::Rc;
use std::cell::RefCell;

// A string's positions count its bytes. A byte outside the string is 0, a slice keeps what lies inside the string, and a
// search starts inside it.
pub fn text_byte(text: &str, at: i64) -> i8 {
    if at >= 0 && (at as usize) < text.len() { text.as_bytes()[at as usize] as i8 } else { 0 }
}

pub fn text_slice(text: &str, from: i64, to: i64) -> String {
    let size = text.len() as i64;
    let start = from.clamp(0, size);
    let end = to.clamp(start, size);
    String::from_utf8_lossy(&text.as_bytes()[start as usize..end as usize]).into_owned()
}

pub fn text_find(text: &str, part: &str, from: i64) -> i64 {
    let start = from.clamp(0, text.len() as i64) as usize;
    text.get(start..).and_then(|rest| rest.find(part)).map(|at| (at + start) as i64).unwrap_or(-1)
}

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

pub fn startsWith(s: String, prefix: String) -> bool {
    if (prefix.len() as i64) > (s.len() as i64) {
        return false;
    }
    return text_slice(&s.clone(), 0, (prefix.len() as i64)) == prefix.clone();
}

pub fn endsWith(s: String, suffix: String) -> bool {
    if (suffix.len() as i64) > (s.len() as i64) {
        return false;
    }
    return text_slice(&s.clone(), (s.len() as i64) - (suffix.len() as i64), (s.len() as i64)) == suffix.clone();
}

pub fn sizeOf(n: i64) -> String {
    let mut matchValue_10_10: String = String::new();
    match n {
        0 => {
            matchValue_10_10 = String::from("none");
        }
        1 | 2 | 3 => {
            matchValue_10_10 = String::from("a few");
        }
        _ => {
            matchValue_10_10 = String::from("many");
        }
    }
    return matchValue_10_10.clone();
}

pub fn gradeOf(score: i64) -> String {
    let mut matchValue_19_10: String = String::new();
    if score >= 90 {
        matchValue_19_10 = String::from("A");
    } else if score >= 80 {
        matchValue_19_10 = String::from("B");
    } else {
        matchValue_19_10 = String::from("C");
    }
    return matchValue_19_10.clone();
}

pub fn kindOf(path: String) -> String {
    let mut matchValue_28_10: String = String::new();
    if path.clone() == String::from("Ninafile") || path.clone() == String::from("Polyfile") {
        matchValue_28_10 = String::from("build");
    } else if endsWith(path.clone(), String::from(".poly")) {
        matchValue_28_10 = String::from("polyir");
    } else if startsWith(path.clone(), String::from("README")) {
        matchValue_28_10 = String::from("docs");
    } else {
        matchValue_28_10 = String::from("other");
    }
    return matchValue_28_10.clone();
}

pub fn shown(found: Option<i64>) -> String {
    let mut matchValue_38_10: String = String::new();
    if found.is_some() && found.clone().unwrap() > 100 {
        let n: i64 = found.clone().unwrap();
        matchValue_38_10 = format!("{}{}", String::from("big "), n.to_string());
    } else if found.is_some() {
        let n: i64 = found.clone().unwrap();
        matchValue_38_10 = format!("{}{}", String::from("found "), n.to_string());
    } else {
        matchValue_38_10 = String::from("nothing");
    }
    return matchValue_38_10.clone();
}

pub fn safeDivide(a: i64, b: i64) -> Result<i64, String> {
    if b == 0 {
        return Err(String::from("divide by zero"));
    }
    return Ok(a / b);
}

pub fn quotient(a: i64, b: i64) -> String {
    let match_54_10: Result<i64, String> = safeDivide(a, b);
    let mut matchValue_54_10: String = String::new();
    if match_54_10.is_ok() {
        let q: i64 = match_54_10.clone().ok().unwrap();
        matchValue_54_10 = format!("{}{}", String::from("quotient "), q.to_string());
    } else if match_54_10.is_err() {
        let why: String = match_54_10.clone().err().unwrap();
        matchValue_54_10 = why.clone();
    }
    return matchValue_54_10.clone();
}

pub fn said(xs: Ref<Vec<i64>>) -> String {
    let mut matchValue_62_10: String = String::new();
    if (xs.borrow().len() as i64) == 0 {
        matchValue_62_10 = String::from("empty");
    } else if (xs.borrow().len() as i64) == 1 {
        let x: i64 = xs.borrow()[0 as usize].clone();
        matchValue_62_10 = format!("{}{}", String::from("just "), x.to_string());
    } else {
        let first: i64 = xs.borrow()[0 as usize].clone();
        let rest: Ref<Vec<i64>> = drop_i64(xs.clone(), 1);
        matchValue_62_10 = format!("{}{}", format!("{}{}", format!("{}{}", first.to_string(), String::from(", then ")), (rest.borrow().len() as i64).to_string()), String::from(" more"));
    }
    return matchValue_62_10.clone();
}

pub fn report(code: i64) -> () {
    if code == 200 || code == 204 {
        println!("{}", String::from("done"));
    } else if code >= 500 {
        println!("{}", format!("{}{}", String::from("retry "), code.to_string()));
    } else if code >= 400 {
        let s: i64 = code;
        println!("{}", format!("{}{}", String::from("failed with "), s.to_string()));
    } else {
        println!("{}", format!("{}{}", String::from("? "), code.to_string()));
    }
}

pub fn main_() -> i64 {
    println!("{}", sizeOf(2));
    println!("{}", gradeOf(85));
    println!("{}", kindOf(String::from("main.poly")));
    println!("{}", shown(Some(500)));
    println!("{}", quotient(7, 2));
    println!("{}", said(share(vec![3, 4, 5])));
    report(404);
    return 0;
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

fn main() {
    std::process::exit(main_() as i32);
}
