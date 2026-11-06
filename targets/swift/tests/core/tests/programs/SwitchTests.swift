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

func endsWith(_ s: String, _ suffix: String) -> Bool {
    if textLength(suffix) > textLength(s) {
        return false
    }
    return textSlice(s, textLength(s) &- textLength(suffix), textLength(s)) == suffix
}

func offText(_ got: String, _ want: String) -> Double {
    if got == want {
        return 0.0
    }
    return 1.0
}

func dayName(_ day: Int64) -> String {
    var name: String = "?"
    switch day {
        case 0, 6:
            name = "weekend"
        case 1:
            name = "monday"
        default:
            name = "weekday"
    }
    return name
}

func checkNumbers() -> Double {
    return offText(dayName(0) + dayName(6) + dayName(1) + dayName(3), "weekendweekendmondayweekday")
}

func classOf(_ c: Int8) -> String {
    var kind: String = "other"
    switch c {
        case 97, 101, 105, 111, 117:
            kind = "vowel"
        case 32:
            kind = "space"
        default:
            break
    }
    return kind
}

func checkChars() -> Double {
    return offText(classOf(101) + classOf(32) + classOf(122), "vowelspaceother")
}

func verb(_ command: String) -> String {
    var done: String = ""
    switch command {
        case "start", "go":
            done = "began"
        case "stop":
            done = "halted"
        default:
            done = "?"
    }
    return done
}

func checkStrings() -> Double {
    return offText(verb("go") + verb("start") + verb("stop") + verb("x"), "beganbeganhalted?")
}

func checkBooleans() -> Double {
    var said: String = ""
    do {
        var i: Int64 = 0
        while i < 2 {
            let match_57_9: Bool = i == 0
            switch match_57_9 {
                case true:
                    said = said + "yes"
                case false:
                    said = said + "no"
                default:
                    break
            }
            i = i &+ 1
        }
    }
    return offText(said, "yesno")
}

func outcome(_ r: Either<Int64, String>) -> String {
    var said: String = ""
    if r.isOk {
        let v: Int64 = r.okValue
        said = "ok \(v)"
    } else if !r.isOk {
        let e: String = r.errValue
        said = "err \(e)"
    }
    return said
}

func checkSums() -> Double {
    var count: Int64 = 0
    let found: Int64? = 4
    if found != nil {
        let n: Int64 = found!
        count = n
    } else if found == nil {
        count = 0 &- 1
    }
    return offText("\(outcome(Either<Int64, String>.ok(3))), \(outcome(Either<Int64, String>.err("no"))), \(count)", "ok 3, err no, 4")
}

func status(_ code: Int64) -> String {
    var said: String = ""
    if code == 200 || code == 204 {
        said = "done"
    } else if code >= 500 {
        said = "retry \(code)"
    } else if code >= 400 {
        let s: Int64 = code
        said = "fail \(s)"
    } else {
        said = "? \(code)"
    }
    return said
}

func checkConditionsAmongKeys() -> Double {
    return offText(status(204) + "; " + status(503) + "; " + status(404) + "; " + status(301), "done; retry 503; fail 404; ? 301")
}

func counter() -> Int64 {
    return 500
}

func checkCallSubject() -> Double {
    var said: String = ""
    let match_107_5: Int64 = counter()
    if match_107_5 == 200 {
        said = "ok"
    } else if match_107_5 >= 500 {
        let s: Int64 = match_107_5
        said = "server \(s)"
    } else {
        said = "other"
    }
    return offText(said, "server 500")
}

func step(_ name: String) -> String {
    var said: String = ""
    if endsWith(name, ".poly") {
        said = "compile " + name + ";"
    } else {
        said = "skip " + name + ";"
    }
    return said
}

func checkConditions() -> Double {
    return offText(step("a.poly") + step("b.md"), "compile a.poly;skip b.md;")
}
