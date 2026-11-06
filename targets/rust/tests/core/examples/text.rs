use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

// What polyir shares (a struct, a list, a map) is a `Ref`: one value that every name holding it reads and changes.
pub type Ref<T> = Rc<RefCell<T>>;

pub fn share<T>(value: T) -> Ref<T> {
    Rc::new(RefCell::new(value))
}

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

pub fn split(s: String, sep: String) -> Ref<Vec<String>> {
    let parts: Ref<Vec<String>> = share(Vec::new());
    if (sep.len() as i64) == 0 {
        { let pushed = s.clone(); parts.borrow_mut().push(pushed) };
        return parts.clone();
    }
    let mut from: i64 = 0;
    {
        let mut at: i64 = text_find(&s.clone(), &sep.clone(), 0);
        while at >= 0 {
            { let pushed = text_slice(&s.clone(), from, at); parts.borrow_mut().push(pushed) };
            from = at + (sep.len() as i64);
            at = text_find(&s.clone(), &sep.clone(), from);
        }
    }
    { let pushed = text_slice(&s.clone(), from, (s.len() as i64)); parts.borrow_mut().push(pushed) };
    return parts.clone();
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

pub fn isSpaceChar(c: i8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 13;
}

pub fn trim(s: String) -> String {
    let mut from: i64 = 0;
    while from < (s.len() as i64) && isSpaceChar(text_byte(&s.clone(), from)) {
        from = from + 1;
    }
    let mut to: i64 = (s.len() as i64);
    while to > from && isSpaceChar(text_byte(&s.clone(), to - 1)) {
        to = to - 1;
    }
    return text_slice(&s.clone(), from, to);
}

pub fn repeat(s: String, n: i64) -> String {
    let mut out: String = String::from("");
    {
        let mut i: i64 = 0;
        while i < n {
            out = format!("{}{}", out.clone(), s.clone());
            i = i + 1;
        }
    }
    return out.clone();
}

pub fn fill(template: String, values: Ref<HashMap<String, String>>) -> String {
    let mut out: String = String::from("");
    let mut from: i64 = 0;
    {
        let mut at: i64 = text_find(&template.clone(), &String::from("${"), 0);
        while at >= 0 {
            let close: i64 = text_find(&template.clone(), &String::from("}"), at);
            if close < 0 {
                return format!("{}{}", out.clone(), text_slice(&template.clone(), from, (template.len() as i64)));
            }
            out = format!("{}{}", format!("{}{}", out.clone(), text_slice(&template.clone(), from, at)), filledName(text_slice(&template.clone(), at, close + 1), values.clone()));
            from = close + 1;
            at = text_find(&template.clone(), &String::from("${"), from);
        }
    }
    return format!("{}{}", out.clone(), text_slice(&template.clone(), from, (template.len() as i64)));
}

pub fn filledName(placeholder: String, values: Ref<HashMap<String, String>>) -> String {
    let name: String = trim(text_slice(&placeholder.clone(), 2, (placeholder.len() as i64) - 1));
    if values.borrow().contains_key(&name.clone()) {
        return values.borrow().get(&name.clone()).cloned().unwrap_or_else(|| String::new());
    }
    return placeholder.clone();
}

#[derive(Clone)]
pub struct StringBuilder {
    pub pieces: Ref<Vec<String>>,
    pub indents: Ref<Vec<i64>>,
}

impl PartialEq for StringBuilder {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.pieces == other.pieces
            && self.indents == other.indents
    }
}

impl StringBuilder {
    pub fn new(pieces: Ref<Vec<String>>, indents: Ref<Vec<i64>>) -> Ref<StringBuilder> {
        share(StringBuilder { pieces, indents })
    }
}

pub trait StringBuilderMethods {
    fn add(&self, s: String) -> Ref<StringBuilder>;
    fn line(&self, s: String) -> Ref<StringBuilder>;
    fn blank(&self) -> Ref<StringBuilder>;
    fn lines(&self, all: Ref<Vec<String>>) -> Ref<StringBuilder>;
    fn indent(&self, n: i64) -> Ref<StringBuilder>;
    fn dedent(&self) -> Ref<StringBuilder>;
    fn indentNow(&self) -> i64;
    fn length(&self) -> i64;
    fn isEmpty(&self) -> bool;
    fn text(&self) -> String;
}

impl StringBuilderMethods for RefCell<StringBuilder> {
    fn add(&self, s: String) -> Ref<StringBuilder> {
        { let pushed = s.clone(); self.borrow().pieces.clone().borrow_mut().push(pushed) };
        return self;
    }

    fn line(&self, s: String) -> Ref<StringBuilder> {
        if (s.len() as i64) > 0 {
            { let pushed = repeat(String::from(" "), self.indentNow()); self.borrow().pieces.clone().borrow_mut().push(pushed) };
        }
        { let pushed = s.clone(); self.borrow().pieces.clone().borrow_mut().push(pushed) };
        { let pushed = String::from("\n"); self.borrow().pieces.clone().borrow_mut().push(pushed) };
        return self;
    }

    fn blank(&self) -> Ref<StringBuilder> {
        { let pushed = String::from("\n"); self.borrow().pieces.clone().borrow_mut().push(pushed) };
        return self;
    }

    fn lines(&self, all: Ref<Vec<String>>) -> Ref<StringBuilder> {
        {
            let mut i: i64 = 0;
            while i < (all.borrow().len() as i64) {
                self.line(all.borrow()[i as usize].clone());
                i = i + 1;
            }
        }
        return self;
    }

    fn indent(&self, n: i64) -> Ref<StringBuilder> {
        { let pushed = self.indentNow() + n; self.borrow().indents.clone().borrow_mut().push(pushed) };
        return self;
    }

    fn dedent(&self) -> Ref<StringBuilder> {
        if (self.borrow().indents.clone().borrow().len() as i64) > 0 {
            { self.borrow().indents.clone().borrow_mut().truncate(((self.borrow().indents.clone().borrow().len() as i64) - 1) as usize); self.borrow().indents.clone().clone() };
        }
        return self;
    }

    fn indentNow(&self) -> i64 {
        if (self.borrow().indents.clone().borrow().len() as i64) == 0 {
            return 0;
        }
        return self.borrow().indents.clone().borrow()[((self.borrow().indents.clone().borrow().len() as i64) - 1) as usize].clone();
    }

    fn length(&self) -> i64 {
        return (self.text().len() as i64);
    }

    fn isEmpty(&self) -> bool {
        return self.length() == 0;
    }

    fn text(&self) -> String {
        return join(self.borrow().pieces.clone(), String::from(""));
    }
}

#[derive(Clone)]
pub struct Dep {
    pub name: String,
    pub version: String,
}

impl PartialEq for Dep {
    fn eq(&self, other: &Self) -> bool {
        true
            && self.name == other.name
            && self.version == other.version
    }
}

impl Dep {
    pub fn new(name: String, version: String) -> Ref<Dep> {
        share(Dep { name, version })
    }
}

pub trait DepMethods {
}

impl DepMethods for RefCell<Dep> {
}

pub fn cmakeLists(name: String, sources: Ref<Vec<String>>, deps: Ref<Vec<Ref<Dep>>>) -> String {
    let out: Ref<StringBuilder> = StringBuilder::new(share(Vec::new()), share(Vec::new()));
    let d_items = deps.borrow().clone();
    for d in d_items {
        out.line(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", String::from("pkg_check_modules("), d.borrow().name.clone()), String::from(" REQUIRED IMPORTED_TARGET ")), d.borrow().name.clone()), String::from(">=")), d.borrow().version.clone()), String::from(")")));
    }
    return format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", String::from("cmake_minimum_required(VERSION 3.20)"), String::from("\n")), String::from("project(")), name.clone()), String::from(" LANGUAGES C)")), String::from("\n")), String::from("find_package(PkgConfig REQUIRED)")), String::from("\n")), trim(out.text())), String::from("\n")), String::from("add_executable(")), name.clone()), String::from(" ")), join(sources.clone(), String::from(" "))), String::from(")")), String::from("\n"));
}

pub fn checksum(s: String) -> i64 {
    let mut sum: i64 = 0;
    {
        let mut i: i64 = 0;
        while i < (s.len() as i64) {
            sum = (sum * 31 + (text_byte(&s.clone(), i) as i64)) % 1000003;
            i = i + 1;
        }
    }
    return sum;
}

pub fn main_() -> i64 {
    let deps: Ref<Vec<Ref<Dep>>> = share(vec![Dep::new(String::from("zlib"), String::from("1.2")), Dep::new(String::from("sdl2"), String::from("2.0"))]);
    let text: String = cmakeLists(String::from("demo"), share(vec![String::from("main.c"), String::from("util.c")]), deps.clone());
    let values: Ref<HashMap<String, String>> = share(HashMap::new());
    let stored = String::from("polyir");
    values.borrow_mut().insert(String::from("who"), stored);
    let hello: String = fill(String::from("hello ${who}"), values.clone());
    return (checksum(text.clone()) + (split(String::from("a, b ,c"), String::from(",")).borrow().len() as i64) * 7 + (hello.len() as i64)) % 256;
}

fn main() {
    std::process::exit(main_() as i32);
}
