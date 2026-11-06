import Foundation

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

// A push used as a value gives the list; a list shortened; a list, set or map with one more; two lists joined.
fileprivate func pushed<T>(_ items: MutableList<T>, _ item: T) -> MutableList<T> {
    items.append(item)
    return items
}

fileprivate func truncate<T>(_ items: MutableList<T>, _ count: Int64) -> MutableList<T> {
    if Int(count) < items.items.count {
        items.items.removeLast(items.items.count - max(Int(count), 0))
    }
    return items
}

fileprivate func withItem<T>(_ items: MutableList<T>, _ item: T) -> MutableList<T> {
    MutableList(items.items + [item])
}

fileprivate func withItem<T>(_ items: MutableSet<T>, _ item: T) -> MutableSet<T> {
    MutableSet(items.items.union([item]))
}

fileprivate func withEntry<K, V>(_ table: MutableMap<K, V>, _ key: K, _ value: V) -> MutableMap<K, V> {
    let out = MutableMap(table.items)
    out[key] = value
    return out
}

fileprivate func concat<T>(_ a: MutableList<T>, _ b: MutableList<T>) -> MutableList<T> {
    MutableList(a.items + b.items)
}

func split(_ s: String, _ sep: String) -> MutableList<String> {
    let parts: MutableList<String> = MutableList()
    if textLength(sep) == 0 {
        parts.append(s)
        return parts
    }
    var from: Int64 = 0
    do {
        var at: Int64 = textFind(s, sep, 0)
        while at >= 0 {
            parts.append(textSlice(s, from, at))
            from = at &+ textLength(sep)
            at = textFind(s, sep, from)
        }
    }
    parts.append(textSlice(s, from, textLength(s)))
    return parts
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

func isSpaceChar(_ c: Int8) -> Bool {
    return c == 32 || c == 9 || c == 10 || c == 13
}

func trim(_ s: String) -> String {
    var from: Int64 = 0
    while from < textLength(s) && isSpaceChar(textByte(s, from)) {
        from = from &+ 1
    }
    var to: Int64 = textLength(s)
    while to > from && isSpaceChar(textByte(s, to &- 1)) {
        to = to &- 1
    }
    return textSlice(s, from, to)
}

func repeat_(_ s: String, _ n: Int64) -> String {
    var out: String = ""
    do {
        var i: Int64 = 0
        while i < n {
            out = out + s
            i = i &+ 1
        }
    }
    return out
}

func fill(_ template: String, _ values: MutableMap<String, String>) -> String {
    var out: String = ""
    var from: Int64 = 0
    do {
        var at: Int64 = textFind(template, "${", 0)
        while at >= 0 {
            let close: Int64 = textFind(template, "}", at)
            if close < 0 {
                return out + textSlice(template, from, textLength(template))
            }
            out = out + textSlice(template, from, at) + filledName(textSlice(template, at, close &+ 1), values)
            from = close &+ 1
            at = textFind(template, "${", from)
        }
    }
    return out + textSlice(template, from, textLength(template))
}

func filledName(_ placeholder: String, _ values: MutableMap<String, String>) -> String {
    let name: String = trim(textSlice(placeholder, 2, textLength(placeholder) &- 1))
    if values[name] != nil {
        return values[name] ?? ""
    }
    return placeholder
}

final class StringBuilder: Hashable {
    let pieces: MutableList<String>
    let indents: MutableList<Int64>

    init(_ pieces: MutableList<String>, _ indents: MutableList<Int64>) {
        self.pieces = pieces
        self.indents = indents
    }

    func add(_ s: String) -> StringBuilder {
        self.pieces.append(s)
        return self
    }

    func line(_ s: String) -> StringBuilder {
        if textLength(s) > 0 {
            self.pieces.append(repeat_(" ", self.indentNow()))
        }
        self.pieces.append(s)
        self.pieces.append("\n")
        return self
    }

    func blank() -> StringBuilder {
        self.pieces.append("\n")
        return self
    }

    func lines(_ all: MutableList<String>) -> StringBuilder {
        do {
            var i: Int64 = 0
            while i < all.count {
                self.line(all[i])
                i = i &+ 1
            }
        }
        return self
    }

    func indent(_ n: Int64) -> StringBuilder {
        self.indents.append(self.indentNow() &+ n)
        return self
    }

    func dedent() -> StringBuilder {
        if self.indents.count > 0 {
            truncate(self.indents, self.indents.count &- 1)
        }
        return self
    }

    func indentNow() -> Int64 {
        if self.indents.count == 0 {
            return 0
        }
        return self.indents[self.indents.count &- 1]
    }

    func length() -> Int64 {
        return textLength(self.text())
    }

    func isEmpty() -> Bool {
        return self.length() == 0
    }

    func text() -> String {
        return join(self.pieces, "")
    }

    static func == (a: StringBuilder, b: StringBuilder) -> Bool {
        [true, a.pieces == b.pieces, a.indents == b.indents].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(pieces)
        hasher.combine(indents)
    }
}

final class Dep: Hashable {
    let name: String
    let version: String

    init(_ name: String, _ version: String) {
        self.name = name
        self.version = version
    }

    static func == (a: Dep, b: Dep) -> Bool {
        [true, a.name == b.name, a.version == b.version].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(name)
        hasher.combine(version)
    }
}

func cmakeLists(_ name: String, _ sources: MutableList<String>, _ deps: MutableList<Dep>) -> String {
    let out: StringBuilder = StringBuilder(MutableList(), MutableList())
    for d in deps {
        out.line("pkg_check_modules(\(d.name) REQUIRED IMPORTED_TARGET \(d.name)>=\(d.version))")
    }
    return "cmake_minimum_required(VERSION 3.20)" + "\n" + "project(" + name + " LANGUAGES C)" + "\n" + "find_package(PkgConfig REQUIRED)" + "\n" + trim(out.text()) + "\n" + "add_executable(" + name + " " + join(sources, " ") + ")" + "\n"
}

func checksum(_ s: String) -> Int64 {
    var sum: Int64 = 0
    do {
        var i: Int64 = 0
        while i < textLength(s) {
            sum = (sum &* 31 &+ Int64(truncatingIfNeeded: textByte(s, i))) % 1000003
            i = i &+ 1
        }
    }
    return sum
}

func main() -> Int64 {
    let deps: MutableList<Dep> = MutableList([Dep("zlib", "1.2"), Dep("sdl2", "2.0")])
    let text: String = cmakeLists("demo", MutableList(["main.c", "util.c"]), deps)
    let values: MutableMap<String, String> = MutableMap()
    values["who"] = "polyir"
    let hello: String = fill("hello ${who}", values)
    return ((checksum(text) &+ split("a, b ,c", ",").count &* 7) &+ textLength(hello)) % 256
}

exit(Int32(truncatingIfNeeded: main()))
