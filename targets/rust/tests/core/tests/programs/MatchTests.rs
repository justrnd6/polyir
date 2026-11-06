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

// A variant: the number of its case and the value it holds, of whatever type that case holds.
#[derive(Clone)]
pub struct Variant {
    pub tag: i64,
    pub value: Rc<dyn std::any::Any>,
}

impl Variant {
    pub fn new<T: 'static>(tag: i64, value: T) -> Variant {
        Variant { tag, value: Rc::new(value) }
    }

    pub fn get<T: Clone + 'static>(&self) -> T {
        self.value.downcast_ref::<T>().expect("the variant holds another case").clone()
    }
}

pub fn contains(s: String, part: String) -> bool {
    return text_find(&s.clone(), &part.clone(), 0) >= 0;
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

pub fn join(parts: Ref<Vec<String>>, sep: String) -> String {
    let mut out: String = String::from("");
    {
        let mut i: i64 = 0;
        while i < (parts.borrow().len() as i64) {
            if i > 0 {
                out = format!("{}{}", out.clone(), sep.clone());
            }
            out = format!("{}{}", out.clone(), parts.borrow()[i as usize].clone());
            i = i + 1;
        }
    }
    return out.clone();
}

pub fn offText(got: String, want: String) -> f64 {
    if got.clone() == want.clone() {
        return 0.0;
    }
    return 1.0;
}

pub fn sizeOf(n: i64) -> String {
    let mut matchValue_16_12: String = String::new();
    match n {
        0 => {
            matchValue_16_12 = String::from("none");
        }
        1 | 2 | 3 => {
            matchValue_16_12 = String::from("a few");
        }
        -1 => {
            matchValue_16_12 = String::from("minus one");
        }
        _ => {
            matchValue_16_12 = String::from("many");
        }
    }
    return matchValue_16_12.clone();
}

pub fn checkNumbers() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", sizeOf(0), sizeOf(2)), sizeOf(9)), sizeOf(-1)), String::from("nonea fewmanyminus one"));
}

pub fn signOf(n: i64) -> String {
    let mut matchValue_30_16: String = String::new();
    if n < 0 {
        matchValue_30_16 = String::from("negative");
    } else if n == 0 {
        matchValue_30_16 = String::from("zero");
    } else if n >= 100 {
        matchValue_30_16 = String::from("large");
    } else {
        matchValue_30_16 = String::from("positive");
    }
    let sign: String = matchValue_30_16.clone();
    return sign.clone();
}

pub fn checkComparisons() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", signOf(-3), String::from(" ")), signOf(0)), String::from(" ")), signOf(150)), String::from(" ")), signOf(5)), String::from("negative zero large positive"));
}

pub fn gradeOf(score: i64) -> String {
    let mut matchValue_45_12: String = String::new();
    if score == 100 {
        matchValue_45_12 = String::from("perfect");
    } else if score >= 90 {
        let s: i64 = score;
        matchValue_45_12 = format!("{}{}", String::from("A "), s.to_string());
    } else if score >= 80 {
        matchValue_45_12 = String::from("B");
    } else {
        matchValue_45_12 = String::from("C");
    }
    return matchValue_45_12.clone();
}

pub fn checkGuardsOnTheSubject() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", gradeOf(100), gradeOf(93)), gradeOf(85)), gradeOf(10)), String::from("perfectA 93BC"));
}

pub fn yes(reply: String) -> bool {
    let mut matchValue_60_12: bool = false;
    if reply.clone() == String::from("y") || reply.clone() == String::from("yes") {
        matchValue_60_12 = true;
    } else if reply.clone() == String::from("n") || reply.clone() == String::from("no") {
        matchValue_60_12 = false;
    } else {
        matchValue_60_12 = false;
    }
    return matchValue_60_12;
}

pub fn checkStrings() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", yes(String::from("y")).to_string(), String::from(" ")), yes(String::from("yes")).to_string()), String::from(" ")), yes(String::from("no")).to_string()), String::from(" ")), yes(String::from("maybe")).to_string()), String::from("true true false false"));
}

pub fn kindOf(path: String) -> String {
    let mut matchValue_73_12: String = String::new();
    if path.clone() == String::from("Ninafile") || path.clone() == String::from("Polyfile") {
        matchValue_73_12 = String::from("build");
    } else if endsWith(path.clone(), String::from(".poly")) {
        matchValue_73_12 = String::from("polyir");
    } else if startsWith(path.clone(), String::from("README")) {
        matchValue_73_12 = String::from("docs");
    } else if contains(path.clone(), String::from("test")) && !endsWith(path.clone(), String::from(".md")) {
        matchValue_73_12 = String::from("a test");
    } else if endsWith(path.clone(), String::from(".md")) {
        let p: String = path.clone();
        matchValue_73_12 = format!("{}{}", String::from("markdown "), p.clone());
    } else {
        matchValue_73_12 = String::from("other");
    }
    return matchValue_73_12.clone();
}

pub fn checkConditions() -> f64 {
    let kinds: Ref<Vec<String>> = share(vec![kindOf(String::from("Polyfile")), kindOf(String::from("a.poly")), kindOf(String::from("README.md")), kindOf(String::from("my_test.c")), kindOf(String::from("notes.md")), kindOf(String::from("x.c"))]);
    return offText(join(kinds.clone(), String::from(",")), String::from("build,polyir,docs,a test,markdown notes.md,other"));
}

pub fn word(ready: bool) -> String {
    let mut matchValue_89_12: String = String::new();
    match ready {
        true => {
            matchValue_89_12 = String::from("go");
        }
        false => {
            matchValue_89_12 = String::from("wait");
        }
        _ => {}
    }
    return matchValue_89_12.clone();
}

pub fn checkBooleans() -> f64 {
    return offText(format!("{}{}", word(true), word(false)), String::from("gowait"));
}

pub fn shown(found: Option<i64>) -> String {
    let mut matchValue_102_12: String = String::new();
    if found.is_some() && found.clone().unwrap() > 100 {
        let n: i64 = found.clone().unwrap();
        matchValue_102_12 = format!("{}{}", String::from("big "), n.to_string());
    } else if found.is_some() {
        let n: i64 = found.clone().unwrap();
        matchValue_102_12 = format!("{}{}", String::from("found "), n.to_string());
    } else {
        matchValue_102_12 = String::from("nothing");
    }
    return matchValue_102_12.clone();
}

pub fn checkOptionals() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", shown(Some(500)), String::from(", ")), shown(Some(5))), String::from(", ")), shown(None)), String::from("big 500, found 5, nothing"));
}

pub fn safeDivide(a: i64, b: i64) -> Result<i64, String> {
    if b == 0 {
        return Err(String::from("divide by zero"));
    }
    return Ok(a / b);
}

pub fn message(a: i64, b: i64) -> String {
    let match_122_12: Result<i64, String> = safeDivide(a, b);
    let mut matchValue_122_12: String = String::new();
    if match_122_12.is_ok() {
        let q: i64 = match_122_12.clone().ok().unwrap();
        matchValue_122_12 = format!("{}{}", String::from("quotient "), q.to_string());
    } else if match_122_12.is_err() {
        let why: String = match_122_12.clone().err().unwrap();
        matchValue_122_12 = format!("{}{}", String::from("error "), why.clone());
    }
    return matchValue_122_12.clone();
}

pub fn checkEithers() -> f64 {
    return offText(format!("{}{}", format!("{}{}", message(7, 2), String::from("; ")), message(7, 0)), String::from("quotient 3; error divide by zero"));
}

pub fn textOf(shape: Variant) -> String {
    let mut matchValue_133_12: String = String::new();
    if (shape.tag == 0) {
        let r: i64 = shape.get::<i64>();
        matchValue_133_12 = format!("{}{}", String::from("circle "), r.to_string());
    } else if (shape.tag == 1) {
        let s: i64 = shape.get::<i64>();
        matchValue_133_12 = format!("{}{}", String::from("square "), s.to_string());
    } else if (shape.tag == 2) {
        let name: String = shape.get::<String>();
        matchValue_133_12 = name.clone();
    }
    return matchValue_133_12.clone();
}

pub fn checkVariants() -> f64 {
    let circle: String = textOf(Variant::new(0, 2));
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", circle.clone(), String::from(", ")), textOf(Variant::new(1, 4))), String::from(", ")), textOf(Variant::new(2, String::from("blob")))), String::from("circle 2, square 4, blob"));
}

pub fn zeroOr(found: Option<i64>) -> String {
    let mut matchValue_147_12: String = String::new();
    if found.is_some() && found.clone().unwrap() == 0 {
        matchValue_147_12 = String::from("zero");
    } else if found.is_some() {
        matchValue_147_12 = String::from("other");
    } else {
        matchValue_147_12 = String::from("none");
    }
    return matchValue_147_12.clone();
}

pub fn checkPayloadPatterns() -> f64 {
    return offText(format!("{}{}", format!("{}{}", zeroOr(Some(0)), zeroOr(Some(3))), zeroOr(None)), String::from("zeroothernone"));
}

pub fn said(xs: Ref<Vec<i64>>) -> String {
    let mut matchValue_161_12: String = String::new();
    if (xs.borrow().len() as i64) == 0 {
        matchValue_161_12 = String::from("empty");
    } else if (xs.borrow().len() as i64) == 1 {
        let x: i64 = xs.borrow()[0 as usize].clone();
        matchValue_161_12 = format!("{}{}", String::from("just "), x.to_string());
    } else if xs.borrow()[0 as usize].clone() == 0 {
        matchValue_161_12 = String::from("starts at zero");
    } else {
        let first: i64 = xs.borrow()[0 as usize].clone();
        let rest: Ref<Vec<i64>> = drop_i64(xs.clone(), 1);
        matchValue_161_12 = format!("{}{}", format!("{}{}", format!("{}{}", first.to_string(), String::from(", then ")), (rest.borrow().len() as i64).to_string()), String::from(" more"));
    }
    return matchValue_161_12.clone();
}

pub fn checkLists() -> f64 {
    let none: Ref<Vec<i64>> = share(Vec::new());
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", said(none.clone()), String::from("; ")), said(share(vec![7]))), String::from("; ")), said(share(vec![0, 1, 2]))), String::from("; ")), said(share(vec![3, 4, 5]))), String::from("empty; just 7; starts at zero; 3, then 2 more"));
}

pub fn pairOf(xs: Ref<Vec<i64>>) -> String {
    let mut matchValue_176_12: String = String::new();
    if (xs.borrow().len() as i64) == 2 && xs.borrow()[0 as usize].clone() == xs.borrow()[1 as usize].clone() {
        let a: i64 = xs.borrow()[0 as usize].clone();
        matchValue_176_12 = format!("{}{}", String::from("a pair of "), a.to_string());
    } else if (xs.borrow().len() as i64) == 2 {
        let a: i64 = xs.borrow()[0 as usize].clone();
        let b: i64 = xs.borrow()[1 as usize].clone();
        matchValue_176_12 = format!("{}{}", format!("{}{}", a.to_string(), String::from(" and ")), b.to_string());
    } else if (xs.borrow().len() as i64) >= 1 && xs.borrow()[0 as usize].clone() < 0 {
        matchValue_176_12 = String::from("starts below zero");
    } else {
        matchValue_176_12 = String::from("something else");
    }
    return matchValue_176_12.clone();
}

pub fn checkListGuards() -> f64 {
    let same: Ref<Vec<i64>> = share(vec![4, 4]);
    let two: Ref<Vec<i64>> = share(vec![1, 2]);
    let low: Ref<Vec<i64>> = share(vec![-1, 2, 3]);
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", pairOf(same.clone()), String::from("; ")), pairOf(two.clone())), String::from("; ")), pairOf(low.clone())), String::from("; ")), pairOf(share(vec![5]))), String::from("a pair of 4; 1 and 2; starts below zero; something else"));
}

pub fn command(c: String) -> String {
    let mut said: String = String::from("");
    if c.clone() == String::from("quit") || c.clone() == String::from("exit") {
        return String::from("bye");
    } else if startsWith(c.clone(), String::from("say ")) {
        said = text_slice(&c.clone(), 4, (c.len() as i64));
    } else if c.clone() == String::from("noop") {}
    return said.clone();
}

pub fn checkStatements() -> f64 {
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", command(String::from("say hi")), String::from("|")), command(String::from("quit"))), String::from("|")), command(String::from("noop"))), String::from("|")), command(String::from("huh"))), String::from("hi|bye||"));
}

pub fn checkNested() -> f64 {
    let a: i64 = 2;
    let b: i64 = 5;
    let mut matchValue_212_21: String = String::new();
    match a {
        1 => {
            matchValue_212_21 = String::from("one");
        }
        2 => {
            let mut matchValue_212_49: String = String::new();
            match b {
                5 => {
                    matchValue_212_49 = String::from("two-five");
                }
                _ => {
                    matchValue_212_49 = String::from("two");
                }
            }
            matchValue_212_21 = matchValue_212_49.clone();
        }
        _ => {
            matchValue_212_21 = String::from("many");
        }
    }
    let text: String = matchValue_212_21.clone();
    return offText(text.clone(), String::from("two-five"));
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
