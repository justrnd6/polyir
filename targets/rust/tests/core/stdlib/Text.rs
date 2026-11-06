use std::rc::Rc;
use std::cell::RefCell;
use std::collections::HashMap;

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

pub fn lines(s: String) -> Ref<Vec<String>> {
    return split(s.clone(), String::from("\n"));
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

pub fn replace(s: String, old: String, by: String) -> String {
    return join(split(s.clone(), old.clone()), by.clone());
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

pub fn indent(s: String, n: i64) -> String {
    let all: Ref<Vec<String>> = lines(s.clone());
    let pad: String = repeat(String::from(" "), n);
    {
        let mut i: i64 = 0;
        while i < (all.borrow().len() as i64) {
            if (all.borrow()[i as usize].clone().len() as i64) > 0 {
                let stored = format!("{}{}", pad.clone(), all.borrow()[i as usize].clone());
                all.borrow_mut()[i as usize] = stored;
            }
            i = i + 1;
        }
    }
    return join(all.clone(), String::from("\n"));
}

pub fn indentRest(s: String, n: i64) -> String {
    let first: i64 = text_find(&s.clone(), &String::from("\n"), 0);
    if first < 0 {
        return s.clone();
    }
    return format!("{}{}", text_slice(&s.clone(), 0, first + 1), indent(text_slice(&s.clone(), first + 1, (s.len() as i64)), n));
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
