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

pub fn offText(got: String, want: String) -> f64 {
    if got.clone() == want.clone() {
        return 0.0;
    }
    return 1.0;
}

pub fn offInt(got: i64, want: i64) -> f64 {
    return ((got - want) as f64);
}

pub fn checkEscapes() -> f64 {
    let s: String = String::from("a\tb\n\"c\" \\ $");
    return offInt((s.len() as i64), 11);
}

pub fn checkCharEscapes() -> f64 {
    return offInt((10 as i64) + (39 as i64) + (92 as i64), 10 + 39 + 92);
}

pub fn checkInterpolation() -> f64 {
    let name: String = String::from("zlib");
    let n: i64 = 42;
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", String::from("lib "), name.clone()), String::from(" v")), (n + 1).to_string()), String::from(" ok=")), (n > 1).to_string()), String::from("lib zlib v43 ok=true"));
}

pub fn checkInterpolatedMethodCalls() -> f64 {
    let words: Ref<Vec<String>> = share(vec![String::from("a"), String::from("b"), String::from("c")]);
    return offText(format!("{}{}", format!("{}{}", String::from("["), join(words.clone(), String::from(", "))), String::from("]")), String::from("[a, b, c]"));
}

pub fn checkTextBlock() -> f64 {
    let name: String = String::from("demo");
    let text: String = format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", String::from("project("), name.clone()), String::from(")")), String::from("\n")), String::from("  add(\"x\")")), String::from("\n")), String::from("\n")), String::from("done")), String::from("\n"));
    return offText(text.clone(), String::from("project(demo)\n  add(\"x\")\n\ndone\n"));
}

pub fn checkTextBlockWithoutFinalNewline() -> f64 {
    let text: String = format!("{}{}", format!("{}{}", String::from("one"), String::from("\n")), String::from("two"));
    return offText(text.clone(), String::from("one\ntwo"));
}

pub fn checkTextBlockKeepsIndentOfMultiLineValue() -> f64 {
    let inner: String = String::from("a\nb");
    let text: String = format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", String::from("list:"), String::from("\n")), String::from("  ")), indentRest(inner.clone(), 2)), String::from("\n")), String::from("end")), String::from("\n"));
    return offText(text.clone(), String::from("list:\n  a\n  b\nend\n"));
}

pub fn checkStrOfValues() -> f64 {
    let c: i8 = 120;
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", (0 - 7).to_string(), true.to_string()), false.to_string()), ((c as u8) as char).to_string()), String::from("s")), 12.to_string()), String::from("-7truefalsexs12"));
}

pub fn checkCharAt() -> f64 {
    let s: String = String::from("hello");
    return offInt((text_byte(&s.clone(), 1) as i64), 101);
}

pub fn checkSliceAndFind() -> f64 {
    let s: String = String::from("key=value");
    let at: i64 = text_find(&s.clone(), &String::from("="), 0);
    return offText(format!("{}{}", format!("{}{}", text_slice(&s.clone(), 0, at), String::from("|")), text_slice(&s.clone(), at + 1, (s.len() as i64))), String::from("key|value")) + offInt(text_find(&s.clone(), &String::from("z"), 0), 0 - 1);
}

pub fn checkSliceClamps() -> f64 {
    let s: String = String::from("abc");
    return offText(format!("{}{}", text_slice(&s.clone(), 0 - 5, 99), text_slice(&s.clone(), 2, 1)), String::from("abc"));
}

pub fn checkFindFrom() -> f64 {
    let s: String = String::from("a.b.c");
    return offInt(text_find(&s.clone(), &String::from("."), 2), 3);
}

pub fn checkListLiteralAndForIn() -> f64 {
    let mut total: i64 = 0;
    let w_items = share(vec![String::from("cmake"), String::from("-S"), String::from("src")]).borrow().clone();
    for w in w_items {
        total = total + (w.len() as i64);
    }
    return offInt(total, 10);
}

pub fn checkSplitAndJoin() -> f64 {
    let parts: Ref<Vec<String>> = split(String::from("a,b,,c"), String::from(","));
    return offInt((parts.borrow().len() as i64), 4) + offText(join(parts.clone(), String::from("+")), String::from("a+b++c"));
}

pub fn checkTrimAndContains() -> f64 {
    let s: String = String::from("  \tpadded \n");
    return offText(trim(s.clone()), String::from("padded")) + offInt((contains(s.clone(), String::from("dd")) as i64), 1) + offInt((contains(s.clone(), String::from("x")) as i64), 0);
}

pub fn checkStartsAndEnds() -> f64 {
    let s: String = String::from("CMakeLists.txt");
    let both: bool = startsWith(s.clone(), String::from("CMake")) && endsWith(s.clone(), String::from(".txt")) && !endsWith(s.clone(), String::from("CMake"));
    return offInt((both as i64), 1);
}

pub fn checkReplaceRepeatIndent() -> f64 {
    let s: String = replace(String::from("a-b-c"), String::from("-"), String::from("::"));
    return offText(format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", s.clone(), String::from("|")), repeat(String::from("ab"), 3)), String::from("|")), indent(String::from("x\n\ny"), 2)), String::from("a::b::c|ababab|  x\n\n  y"));
}

pub fn checkLines() -> f64 {
    return offInt((lines(String::from("one\ntwo\nthree")).borrow().len() as i64), 3);
}

pub fn checkFill() -> f64 {
    let values: Ref<HashMap<String, String>> = share(HashMap::new());
    let stored = String::from("gcd");
    values.borrow_mut().insert(String::from("name"), stored);
    let stored = String::from("C");
    values.borrow_mut().insert(String::from("lang"), stored);
    return offText(fill(String::from("project(${name} LANGUAGES ${lang}) ${missing}"), values.clone()), String::from("project(gcd LANGUAGES C) ${missing}"));
}

pub fn writeDeps(out: Ref<StringBuilder>, deps: Ref<Vec<String>>) -> () {
    out.indent(4);
    let d_items = deps.borrow().clone();
    for d in d_items {
        out.line(format!("{}{}", format!("{}{}", String::from("implementation(\""), d.clone()), String::from("\")")));
    }
    out.dedent();
}

pub fn checkStringBuilder() -> f64 {
    let out: Ref<StringBuilder> = StringBuilder::new(share(Vec::new()), share(Vec::new()));
    out.line(String::from("dependencies {"));
    writeDeps(out.clone(), share(vec![String::from("a:b:1"), String::from("c:d:2")]));
    out.line(String::from("}"));
    let want: String = format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", format!("{}{}", String::from("dependencies {"), String::from("\n")), String::from("    implementation(\"a:b:1\")")), String::from("\n")), String::from("    implementation(\"c:d:2\")")), String::from("\n")), String::from("}")), String::from("\n"));
    return offText(out.text(), want.clone()) + offInt((out.isEmpty() as i64), 0);
}
