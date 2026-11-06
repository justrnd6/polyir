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

func lines(_ s: String) -> MutableList<String> {
    return split(s, "\n")
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

func replace(_ s: String, _ old: String, _ by: String) -> String {
    return join(split(s, old), by)
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

func indent(_ s: String, _ n: Int64) -> String {
    let all: MutableList<String> = lines(s)
    let pad: String = repeat_(" ", n)
    do {
        var i: Int64 = 0
        while i < all.count {
            if textLength(all[i]) > 0 {
                all[i] = pad + all[i]
            }
            i = i &+ 1
        }
    }
    return join(all, "\n")
}

func indentRest(_ s: String, _ n: Int64) -> String {
    let first: Int64 = textFind(s, "\n", 0)
    if first < 0 {
        return s
    }
    return textSlice(s, 0, first &+ 1) + indent(textSlice(s, first &+ 1, textLength(s)), n)
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

func offText(_ got: String, _ want: String) -> Double {
    if got == want {
        return 0.0
    }
    return 1.0
}

func offInt(_ got: Int64, _ want: Int64) -> Double {
    return Double(got &- want)
}

func checkEscapes() -> Double {
    let s: String = "a\tb\n\"c\" \\ $"
    return offInt(textLength(s), 11)
}

func checkCharEscapes() -> Double {
    return offInt((Int64(truncatingIfNeeded: 10) &+ Int64(truncatingIfNeeded: 39)) &+ Int64(truncatingIfNeeded: 92), (10 &+ 39) &+ 92)
}

func checkInterpolation() -> Double {
    let name: String = "zlib"
    let n: Int64 = 42
    return offText("lib \(name) v\(n &+ 1) ok=\(n > 1)", "lib zlib v43 ok=true")
}

func checkInterpolatedMethodCalls() -> Double {
    let words: MutableList<String> = MutableList(["a", "b", "c"])
    return offText("[\(join(words, ", "))]", "[a, b, c]")
}

func checkTextBlock() -> Double {
    let name: String = "demo"
    let text: String = "project(\(name))" + "\n" + "  add(\"x\")" + "\n" + "\n" + "done" + "\n"
    return offText(text, "project(demo)\n  add(\"x\")\n\ndone\n")
}

func checkTextBlockWithoutFinalNewline() -> Double {
    let text: String = "one" + "\n" + "two"
    return offText(text, "one\ntwo")
}

func checkTextBlockKeepsIndentOfMultiLineValue() -> Double {
    let inner: String = "a\nb"
    let text: String = "list:" + "\n" + "  " + indentRest(inner, 2) + "\n" + "end" + "\n"
    return offText(text, "list:\n  a\n  b\nend\n")
}

func checkStrOfValues() -> Double {
    let c: Int8 = 120
    return offText("\(0 &- 7)\(true)\(false)\(charText(c))\("s")\(12)", "-7truefalsexs12")
}

func checkCharAt() -> Double {
    let s: String = "hello"
    return offInt(Int64(truncatingIfNeeded: textByte(s, 1)), 101)
}

func checkSliceAndFind() -> Double {
    let s: String = "key=value"
    let at: Int64 = textFind(s, "=", 0)
    return offText(textSlice(s, 0, at) + "|" + textSlice(s, at &+ 1, textLength(s)), "key|value") + offInt(textFind(s, "z", 0), 0 &- 1)
}

func checkSliceClamps() -> Double {
    let s: String = "abc"
    return offText(textSlice(s, 0 &- 5, 99) + textSlice(s, 2, 1), "abc")
}

func checkFindFrom() -> Double {
    let s: String = "a.b.c"
    return offInt(textFind(s, ".", 2), 3)
}

func checkListLiteralAndForIn() -> Double {
    var total: Int64 = 0
    for w in MutableList(["cmake", "-S", "src"]) {
        total = total &+ textLength(w)
    }
    return offInt(total, 10)
}

func checkSplitAndJoin() -> Double {
    let parts: MutableList<String> = split("a,b,,c", ",")
    return offInt(parts.count, 4) + offText(join(parts, "+"), "a+b++c")
}

func checkTrimAndContains() -> Double {
    let s: String = "  \tpadded \n"
    return offText(trim(s), "padded") + offInt(Int64(contains(s, "dd") ? 1 : 0), 1) + offInt(Int64(contains(s, "x") ? 1 : 0), 0)
}

func checkStartsAndEnds() -> Double {
    let s: String = "CMakeLists.txt"
    let both: Bool = startsWith(s, "CMake") && endsWith(s, ".txt") && !endsWith(s, "CMake")
    return offInt(Int64(both ? 1 : 0), 1)
}

func checkReplaceRepeatIndent() -> Double {
    let s: String = replace("a-b-c", "-", "::")
    return offText(s + "|" + repeat_("ab", 3) + "|" + indent("x\n\ny", 2), "a::b::c|ababab|  x\n\n  y")
}

func checkLines() -> Double {
    return offInt(lines("one\ntwo\nthree").count, 3)
}

func checkFill() -> Double {
    let values: MutableMap<String, String> = MutableMap()
    values["name"] = "gcd"
    values["lang"] = "C"
    return offText(fill("project(${name} LANGUAGES ${lang}) ${missing}", values), "project(gcd LANGUAGES C) ${missing}")
}

func writeDeps(_ out: StringBuilder, _ deps: MutableList<String>) -> Void {
    out.indent(4)
    for d in deps {
        out.line("implementation(\"\(d)\")")
    }
    out.dedent()
}

func checkStringBuilder() -> Double {
    let out: StringBuilder = StringBuilder(MutableList(), MutableList())
    out.line("dependencies {")
    writeDeps(out, MutableList(["a:b:1", "c:d:2"]))
    out.line("}")
    let want: String = "dependencies {" + "\n" + "    implementation(\"a:b:1\")" + "\n" + "    implementation(\"c:d:2\")" + "\n" + "}" + "\n"
    return offText(out.text(), want) + offInt(Int64(out.isEmpty() ? 1 : 0), 0)
}
