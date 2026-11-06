import Foundation

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

func sizeOf(_ n: Int64) -> String {
    var matchValue_10_10: String = ""
    switch n {
        case 0:
            matchValue_10_10 = "none"
        case 1, 2, 3:
            matchValue_10_10 = "a few"
        default:
            matchValue_10_10 = "many"
    }
    return matchValue_10_10
}

func gradeOf(_ score: Int64) -> String {
    var matchValue_19_10: String = ""
    if score >= 90 {
        matchValue_19_10 = "A"
    } else if score >= 80 {
        matchValue_19_10 = "B"
    } else {
        matchValue_19_10 = "C"
    }
    return matchValue_19_10
}

func kindOf(_ path: String) -> String {
    var matchValue_28_10: String = ""
    if path == "Ninafile" || path == "Polyfile" {
        matchValue_28_10 = "build"
    } else if endsWith(path, ".poly") {
        matchValue_28_10 = "polyir"
    } else if startsWith(path, "README") {
        matchValue_28_10 = "docs"
    } else {
        matchValue_28_10 = "other"
    }
    return matchValue_28_10
}

func shown(_ found: Int64?) -> String {
    var matchValue_38_10: String = ""
    if found != nil && found! > 100 {
        let n: Int64 = found!
        matchValue_38_10 = "big \(n)"
    } else if found != nil {
        let n: Int64 = found!
        matchValue_38_10 = "found \(n)"
    } else {
        matchValue_38_10 = "nothing"
    }
    return matchValue_38_10
}

func safeDivide(_ a: Int64, _ b: Int64) -> Either<Int64, String> {
    if b == 0 {
        return Either<Int64, String>.err("divide by zero")
    }
    return Either<Int64, String>.ok(a / b)
}

func quotient(_ a: Int64, _ b: Int64) -> String {
    let match_54_10: Either<Int64, String> = safeDivide(a, b)
    var matchValue_54_10: String = ""
    if match_54_10.isOk {
        let q: Int64 = match_54_10.okValue
        matchValue_54_10 = "quotient \(q)"
    } else if !match_54_10.isOk {
        let why: String = match_54_10.errValue
        matchValue_54_10 = why
    }
    return matchValue_54_10
}

func said(_ xs: MutableList<Int64>) -> String {
    var matchValue_62_10: String = ""
    if xs.count == 0 {
        matchValue_62_10 = "empty"
    } else if xs.count == 1 {
        let x: Int64 = xs[0]
        matchValue_62_10 = "just \(x)"
    } else {
        let first: Int64 = xs[0]
        let rest: MutableList<Int64> = drop(xs, 1)
        matchValue_62_10 = "\(first), then \(rest.count) more"
    }
    return matchValue_62_10
}

func report(_ code: Int64) -> Void {
    if code == 200 || code == 204 {
        print("done")
    } else if code >= 500 {
        print("retry \(code)")
    } else if code >= 400 {
        let s: Int64 = code
        print("failed with \(s)")
    } else {
        print("? \(code)")
    }
}

func main() -> Int64 {
    print(sizeOf(2))
    print(gradeOf(85))
    print(kindOf("main.poly"))
    print(shown(500))
    print(quotient(7, 2))
    print(said(MutableList([3, 4, 5])))
    report(404)
    return 0
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

exit(Int32(truncatingIfNeeded: main()))
