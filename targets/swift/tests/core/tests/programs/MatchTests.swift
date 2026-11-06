// A string's positions count its UTF-8 bytes, as on every target. A byte outside the string is 0, a slice keeps what lies
// inside the string, and a search starts inside it. A char (`Int8`) written is the character of its byte.
fileprivate func textLength(_ text: String) -> Int64 {
    Int64(text.utf8.count)
}

fileprivate func textByte(_ text: String, _ at: Int64) -> Int8 {
    let bytes = Array(text.utf8)
    return at >= 0 && at < Int64(bytes.count) ? Int8(bitPattern: bytes[Int(at)]) : 0
}

fileprivate func textSlice(_ text: String, _ from: Int64, _ to: Int64) -> String {
    let bytes = Array(text.utf8)
    let start = min(max(Int(from), 0), bytes.count)
    let end = min(max(Int(to), start), bytes.count)
    return String(decoding: bytes[start..<end], as: UTF8.self)
}

fileprivate func textFind(_ text: String, _ part: String, _ from: Int64) -> Int64 {
    let bytes = Array(text.utf8)
    let wanted = Array(part.utf8)
    var at = min(max(Int(from), 0), bytes.count)
    while at + wanted.count <= bytes.count {
        if Array(bytes[at..<(at + wanted.count)]) == wanted {
            return Int64(at)
        }
        at += 1
    }
    return -1
}

fileprivate func charText(_ c: Int8) -> String {
    String(UnicodeScalar(UInt8(bitPattern: c)))
}

// What polyir shares is a class: two names for one list, map or set see each other's changes. Sizes and positions are Int64.
final class MutableList<T>: Sequence {
    var items: [T]

    init(_ items: [T] = []) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    subscript(at: Int64) -> T {
        get { items[Int(at)] }
        set { items[Int(at)] = newValue }
    }

    func append(_ item: T) {
        items.append(item)
    }

    func makeIterator() -> IndexingIterator<[T]> {
        items.makeIterator()
    }
}

extension MutableList: Equatable where T: Equatable {
    static func == (a: MutableList, b: MutableList) -> Bool {
        a.items == b.items
    }
}

extension MutableList: Hashable where T: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class MutableMap<K: Hashable, V> {
    var items: [K: V]

    init(_ items: [K: V] = [:]) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    var keys: [K] {
        Array(items.keys)
    }

    subscript(key: K) -> V? {
        get { items[key] }
        set { items[key] = newValue }
    }
}

extension MutableMap: Equatable where V: Equatable {
    static func == (a: MutableMap, b: MutableMap) -> Bool {
        a.items == b.items
    }
}

extension MutableMap: Hashable where V: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

final class MutableSet<T: Hashable>: Sequence, Hashable {
    var items: Set<T>

    init(_ items: Set<T> = []) {
        self.items = items
    }

    var count: Int64 {
        Int64(items.count)
    }

    func makeIterator() -> Set<T>.Iterator {
        items.makeIterator()
    }

    static func == (a: MutableSet, b: MutableSet) -> Bool {
        a.items == b.items
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(items)
    }
}

// An `either`: an ok value or an error.
enum Either<T, E> {
    case ok(T)
    case err(E)

    var isOk: Bool {
        if case .ok = self { return true }
        return false
    }

    var okValue: T {
        if case .ok(let value) = self { return value }
        fatalError("not an ok value")
    }

    var errValue: E {
        if case .err(let value) = self { return value }
        fatalError("not an error")
    }
}

extension Either: Equatable where T: Equatable, E: Equatable {}

extension Either: Hashable where T: Hashable, E: Hashable {}

// A variant: the number of its case and the value it holds. Two compare by their cases and what their values say.
struct Variant: Equatable {
    let tag: Int64
    let value: Any?

    static func == (a: Variant, b: Variant) -> Bool {
        a.tag == b.tag && String(describing: a.value) == String(describing: b.value)
    }
}

func contains(_ s: String, _ part: String) -> Bool {
    return textFind(s, part, 0) >= 0
}

func startsWith(_ s: String, _ prefix: String) -> Bool {
    if textLength(prefix) > textLength(s) {
        return false
    }
    return textSlice(s, 0, textLength(prefix)) == prefix
}

func endsWith(_ s: String, _ suffix: String) -> Bool {
    if textLength(suffix) > textLength(s) {
        return false
    }
    return textSlice(s, textLength(s) &- textLength(suffix), textLength(s)) == suffix
}

func join(_ parts: MutableList<String>, _ sep: String) -> String {
    var out: String = ""
    do {
        var i: Int64 = 0
        while i < parts.count {
            if i > 0 {
                out = out + sep
            }
            out = out + parts[i]
            i = i &+ 1
        }
    }
    return out
}

func offText(_ got: String, _ want: String) -> Double {
    if got == want {
        return 0.0
    }
    return 1.0
}

func sizeOf(_ n: Int64) -> String {
    var matchValue_16_12: String = ""
    switch n {
        case 0:
            matchValue_16_12 = "none"
        case 1, 2, 3:
            matchValue_16_12 = "a few"
        case -1:
            matchValue_16_12 = "minus one"
        default:
            matchValue_16_12 = "many"
    }
    return matchValue_16_12
}

func checkNumbers() -> Double {
    return offText(sizeOf(0) + sizeOf(2) + sizeOf(9) + sizeOf(0 &- 1), "nonea fewmanyminus one")
}

func signOf(_ n: Int64) -> String {
    var matchValue_30_16: String = ""
    if n < 0 {
        matchValue_30_16 = "negative"
    } else if n == 0 {
        matchValue_30_16 = "zero"
    } else if n >= 100 {
        matchValue_30_16 = "large"
    } else {
        matchValue_30_16 = "positive"
    }
    let sign: String = matchValue_30_16
    return sign
}

func checkComparisons() -> Double {
    return offText(signOf(0 &- 3) + " " + signOf(0) + " " + signOf(150) + " " + signOf(5), "negative zero large positive")
}

func gradeOf(_ score: Int64) -> String {
    var matchValue_45_12: String = ""
    if score == 100 {
        matchValue_45_12 = "perfect"
    } else if score >= 90 {
        let s: Int64 = score
        matchValue_45_12 = "A \(s)"
    } else if score >= 80 {
        matchValue_45_12 = "B"
    } else {
        matchValue_45_12 = "C"
    }
    return matchValue_45_12
}

func checkGuardsOnTheSubject() -> Double {
    return offText(gradeOf(100) + gradeOf(93) + gradeOf(85) + gradeOf(10), "perfectA 93BC")
}

func yes(_ reply: String) -> Bool {
    var matchValue_60_12: Bool = false
    switch reply {
        case "y", "yes":
            matchValue_60_12 = true
        case "n", "no":
            matchValue_60_12 = false
        default:
            matchValue_60_12 = false
    }
    return matchValue_60_12
}

func checkStrings() -> Double {
    return offText("\(yes("y")) \(yes("yes")) \(yes("no")) \(yes("maybe"))", "true true false false")
}

func kindOf(_ path: String) -> String {
    var matchValue_73_12: String = ""
    if path == "Ninafile" || path == "Polyfile" {
        matchValue_73_12 = "build"
    } else if endsWith(path, ".poly") {
        matchValue_73_12 = "polyir"
    } else if startsWith(path, "README") {
        matchValue_73_12 = "docs"
    } else if contains(path, "test") && !endsWith(path, ".md") {
        matchValue_73_12 = "a test"
    } else if endsWith(path, ".md") {
        let p: String = path
        matchValue_73_12 = "markdown \(p)"
    } else {
        matchValue_73_12 = "other"
    }
    return matchValue_73_12
}

func checkConditions() -> Double {
    let kinds: MutableList<String> = MutableList([kindOf("Polyfile"), kindOf("a.poly"), kindOf("README.md"), kindOf("my_test.c"), kindOf("notes.md"), kindOf("x.c")])
    return offText(join(kinds, ","), "build,polyir,docs,a test,markdown notes.md,other")
}

func word(_ ready: Bool) -> String {
    var matchValue_89_12: String = ""
    switch ready {
        case true:
            matchValue_89_12 = "go"
        case false:
            matchValue_89_12 = "wait"
        default:
            break
    }
    return matchValue_89_12
}

func checkBooleans() -> Double {
    return offText(word(true) + word(false), "gowait")
}

func shown(_ found: Int64?) -> String {
    var matchValue_102_12: String = ""
    if found != nil && found! > 100 {
        let n: Int64 = found!
        matchValue_102_12 = "big \(n)"
    } else if found != nil {
        let n: Int64 = found!
        matchValue_102_12 = "found \(n)"
    } else {
        matchValue_102_12 = "nothing"
    }
    return matchValue_102_12
}

func checkOptionals() -> Double {
    return offText(shown(500) + ", " + shown(5) + ", " + shown(nil), "big 500, found 5, nothing")
}

func safeDivide(_ a: Int64, _ b: Int64) -> Either<Int64, String> {
    if b == 0 {
        return Either<Int64, String>.err("divide by zero")
    }
    return Either<Int64, String>.ok(a / b)
}

func message(_ a: Int64, _ b: Int64) -> String {
    let match_122_12: Either<Int64, String> = safeDivide(a, b)
    var matchValue_122_12: String = ""
    if match_122_12.isOk {
        let q: Int64 = match_122_12.okValue
        matchValue_122_12 = "quotient \(q)"
    } else if !match_122_12.isOk {
        let why: String = match_122_12.errValue
        matchValue_122_12 = "error \(why)"
    }
    return matchValue_122_12
}

func checkEithers() -> Double {
    return offText(message(7, 2) + "; " + message(7, 0), "quotient 3; error divide by zero")
}

func textOf(_ shape: Variant) -> String {
    var matchValue_133_12: String = ""
    if shape.tag == 0 {
        let r: Int64 = shape.value as! Int64
        matchValue_133_12 = "circle \(r)"
    } else if shape.tag == 1 {
        let s: Int64 = shape.value as! Int64
        matchValue_133_12 = "square \(s)"
    } else if shape.tag == 2 {
        let name: String = shape.value as! String
        matchValue_133_12 = name
    }
    return matchValue_133_12
}

func checkVariants() -> Double {
    let circle: String = textOf(Variant(tag: 0, value: Int64(2)))
    return offText(circle + ", " + textOf(Variant(tag: 1, value: Int64(4))) + ", " + textOf(Variant(tag: 2, value: "blob")), "circle 2, square 4, blob")
}

func zeroOr(_ found: Int64?) -> String {
    var matchValue_147_12: String = ""
    if found != nil && found! == 0 {
        matchValue_147_12 = "zero"
    } else if found != nil {
        matchValue_147_12 = "other"
    } else {
        matchValue_147_12 = "none"
    }
    return matchValue_147_12
}

func checkPayloadPatterns() -> Double {
    return offText(zeroOr(0) + zeroOr(3) + zeroOr(nil), "zeroothernone")
}

func said(_ xs: MutableList<Int64>) -> String {
    var matchValue_161_12: String = ""
    if xs.count == 0 {
        matchValue_161_12 = "empty"
    } else if xs.count == 1 {
        let x: Int64 = xs[0]
        matchValue_161_12 = "just \(x)"
    } else if xs[0] == 0 {
        matchValue_161_12 = "starts at zero"
    } else {
        let first: Int64 = xs[0]
        let rest: MutableList<Int64> = drop(xs, 1)
        matchValue_161_12 = "\(first), then \(rest.count) more"
    }
    return matchValue_161_12
}

func checkLists() -> Double {
    let none: MutableList<Int64> = MutableList()
    return offText(said(none) + "; " + said(MutableList([7])) + "; " + said(MutableList([0, 1, 2])) + "; " + said(MutableList([3, 4, 5])), "empty; just 7; starts at zero; 3, then 2 more")
}

func pairOf(_ xs: MutableList<Int64>) -> String {
    var matchValue_176_12: String = ""
    if xs.count == 2 && xs[0] == xs[1] {
        let a: Int64 = xs[0]
        matchValue_176_12 = "a pair of \(a)"
    } else if xs.count == 2 {
        let a: Int64 = xs[0]
        let b: Int64 = xs[1]
        matchValue_176_12 = "\(a) and \(b)"
    } else if xs.count >= 1 && xs[0] < 0 {
        matchValue_176_12 = "starts below zero"
    } else {
        matchValue_176_12 = "something else"
    }
    return matchValue_176_12
}

func checkListGuards() -> Double {
    let same: MutableList<Int64> = MutableList([4, 4])
    let two: MutableList<Int64> = MutableList([1, 2])
    let low: MutableList<Int64> = MutableList([0 &- 1, 2, 3])
    return offText(pairOf(same) + "; " + pairOf(two) + "; " + pairOf(low) + "; " + pairOf(MutableList([5])), "a pair of 4; 1 and 2; starts below zero; something else")
}

func command(_ c: String) -> String {
    var said: String = ""
    if c == "quit" || c == "exit" {
        return "bye"
    } else if startsWith(c, "say ") {
        said = textSlice(c, 4, textLength(c))
    } else if c == "noop" {}
    return said
}

func checkStatements() -> Double {
    return offText(command("say hi") + "|" + command("quit") + "|" + command("noop") + "|" + command("huh"), "hi|bye||")
}

func checkNested() -> Double {
    let a: Int64 = 2
    let b: Int64 = 5
    var matchValue_212_21: String = ""
    switch a {
        case 1:
            matchValue_212_21 = "one"
        case 2:
            var matchValue_212_49: String = ""
            switch b {
                case 5:
                    matchValue_212_49 = "two-five"
                default:
                    matchValue_212_49 = "two"
            }
            matchValue_212_21 = matchValue_212_49
        default:
            matchValue_212_21 = "many"
    }
    let text: String = matchValue_212_21
    return offText(text, "two-five")
}

func drop<T>(_ xs: MutableList<T>, _ n: Int64) -> MutableList<T> {
    let out: MutableList<T> = MutableList()
    do {
        var i: Int64 = n
        while i < xs.count {
            out.append(xs[i])
            i = i &+ 1
        }
    }
    return out
}
