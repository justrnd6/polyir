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

// A variant: the number of its case and the value it holds. Two compare by their cases and what their values say.
struct Variant: Equatable {
    let tag: Int64
    let value: Any?

    static func == (a: Variant, b: Variant) -> Bool {
        a.tag == b.tag && String(describing: a.value) == String(describing: b.value)
    }
}

// A coroutine: its body runs on a thread of its own, only while a resume waits for it. `resume` gives case 0 with the value
// the body gives, or case 1 when it has finished; `give` hands a value over and hears whether the coroutine was closed.
final class Coroutine<T> {
    private let body: (Coroutine<T>) -> Void
    private let toCaller = DispatchSemaphore(value: 0)
    private let toBody = DispatchSemaphore(value: 0)
    private var handed: T?
    private var closing = false
    private var started = false
    private var finished = false

    init(_ body: @escaping (Coroutine<T>) -> Void) {
        self.body = body
    }

    func resume() -> Variant {
        if finished {
            return Variant(tag: 1, value: nil)
        }
        handed = nil
        if started {
            toBody.signal()
        } else {
            started = true
            Thread {
                self.body(self)
                self.finished = true
                self.toCaller.signal()
            }.start()
        }
        toCaller.wait()
        return finished ? Variant(tag: 1, value: nil) : Variant(tag: 0, value: handed)
    }

    func give(_ value: T) -> Bool {
        handed = value
        toCaller.signal()
        toBody.wait()
        return closing
    }

    func done() -> Bool {
        finished
    }

    func close() {
        if started && !finished {
            closing = true
            toBody.signal()
        }
        finished = true
    }
}

// What `throw` throws: the code thrown.
struct Thrown: Error {
    let value: Int64
}

var flag: Bool = false
var ratio: Double = 1.5
var hits: Int64 = 0

final class Node: Hashable {
    var value: Int64
    var next: (Node)?

    init(_ value: Int64, _ next: (Node)?) {
        self.value = value
        self.next = next
    }

    func copy() -> Node {
        Node(value, next)
    }

    static func == (a: Node, b: Node) -> Bool {
        [true, a.value == b.value, a.next == b.next].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(value)
        hasher.combine(next)
    }
}

final class Holder: Hashable {
    let op: (Int64) -> Int64
    let name: String

    init(_ op: (Int64) -> Int64, _ name: String) {
        self.op = op
        self.name = name
    }

    func run(_ x: Int64) -> Int64 {
        let f: (Int64) -> Int64 = self.op
        return f(x)
    }

    func viaLater() -> (Int64) -> Int64 {
        return { (y: Int64) -> Int64 in
            let f: (Int64) -> Int64 = self.op
            return f(y) &+ textLength(self.name)
        }
    }

    static func == (a: Holder, b: Holder) -> Bool {
        [true, a.op == b.op, a.name == b.name].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(op)
        hasher.combine(name)
    }
}

func pairs(_ n: Int64) -> Coroutine<Node> {
    Coroutine { coroutine in
        do {
            var i: Int64 = 0
            while i < n {
                if coroutine.give(Node(i, nil)) {
                    return
                }
                i = i &+ 1
            }
        }
    }
}

func sumNodes(_ n: Int64) -> Int64 {
    let g: Coroutine<Node> = pairs(n)
    var s: Int64 = 0
    var done: Bool = false
    do {
        var k: Int64 = 0
        while k < 20 && done == false {
            let r: Variant = g.resume()
            if r.tag == 1 {
                done = true
            } else {
                s = s &+ (r.value as! Node).value
            }
            k = k &+ 1
        }
    }
    return s
}

func chain() -> Int64 {
    let a: Node = Node(1, nil)
    let b: Node = Node(2, a)
    let c: Node = Node(3, b)
    var sum: Int64 = 0
    var cur: (Node)? = c
    do {
        var i: Int64 = 0
        while i < 5 && cur != nil {
            let n: Node = cur!
            sum = sum &+ n.value
            cur = n.next
            i = i &+ 1
        }
    }
    a.value = 10
    return sum &* 100 &+ b.next!.value
}

func nestedClosures(_ a: Int64) -> Int64 {
    var total: Int64 = 0
    let outer: (Int64) -> Int64 = { (x: Int64) -> Int64 in
        let inner: (Int64) -> Int64 = { (y: Int64) -> Int64 in
            total = total &+ y
            return x &* y &+ a
        }
        return inner(x &+ 1) &+ inner(2)
    }
    let r: Int64 = outer(3)
    return r &* 1000 &+ total
}

func strClosure(_ prefix: String) -> Int64 {
    let tail: String = "!"
    let f: (String) -> String = { (s: String) in prefix + s + tail }
    return textLength(f("abc"))
}

func holders() -> Int64 {
    let h: Holder = Holder({ (x: Int64) in x &* x }, "hh")
    let later: (Int64) -> Int64 = h.viaLater()
    return h.run(5) &+ later(2)
}

func globals() -> Int64 {
    hits = hits &+ 5
    flag = true
    ratio = ratio * 2.0
    var r: Int64 = hits
    if flag {
        r = r &+ 100
    }
    if ratio > 2.5 {
        r = r &+ 1000
    }
    return r
}

func findFirst(_ limit: Int64) -> Int64 {
    do {
        var i: Int64 = 0
        while i < limit {
            do {
                var j: Int64 = 0
                while j < limit {
                    if i &* j == 12 {
                        return i &* 100 &+ j
                    }
                    j = j &+ 1
                }
            }
            i = i &+ 1
        }
    }
    return 0 &- 1
}

func tryInLoop(_ n: Int64) -> Int64 {
    var s: Int64 = 0
    do {
        var i: Int64 = 0
        while i < n {
            do {
                if i % 2 == 1 {
                    throw Thrown(value: i)
                }
                s = s &+ 10
            } catch {
                let code = (error as! Thrown).value
                s = s &+ code
            }
            i = i &+ 1
        }
    }
    return s
}

func lines(_ a: String, _ b: String) -> String {
    var label: Int64 = 0
    var acc: String = ""
    var n: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            acc = a
            n = 0
            label = 1
            continue machine
        case 1:
            if n < 3 {
                label = 2
                continue machine
            } else {
                label = 3
                continue machine
            }
        case 2:
            acc = acc + b
            n = n &+ 1
            label = 1
            continue machine
        case 3:
            return acc
        default:
            fatalError("no block \(label)")
        }
    }
}

func lineLen() -> Int64 {
    return textLength(lines("x", "yz"))
}

func machineStrings(_ n: Int64) -> Int64 {
    var label: Int64 = 0
    var p: Int64 = 0
    var s: String = ""
    var q: Int64 = 0
    var r: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            if n == 0 {
                p = n
                s = "p"
                label = 1
                continue machine
            } else {
                q = n
                s = "q"
                label = 2
                continue machine
            }
        case 1:
            if p > 4 {
                r = p
                label = 3
                continue machine
            } else {
                q = p &+ 1
                s = s + "a"
                label = 2
                continue machine
            }
        case 2:
            if q > 4 {
                r = q
                label = 3
                continue machine
            } else {
                p = q &+ 1
                s = s + "b"
                label = 1
                continue machine
            }
        case 3:
            return r &* 100 &+ textLength(s)
        default:
            fatalError("no block \(label)")
        }
    }
}

func precedence(_ a: Int64, _ b: Int64) -> Int64 {
    let x: Int64 = (a &+ b) &* (a &- b) / 3
    let y: Int64 = a & b | (a ^ b)
    let z: Int64 = a << (2 &+ b) >> 1
    var s: Int64 = (x &+ y) &+ z
    if a & 1 == 1 && b > 2 || a == 0 {
        s = s &+ 1
    }
    let w: Int32 = Int32(truncatingIfNeeded: a)
    if Int64(truncatingIfNeeded: w) < 100 && Int32(truncatingIfNeeded: a &+ b) < 1000 {
        s = s &+ 2
    }
    return s
}

func main() -> Int64 {
    var score: Int64 = ((sumNodes(5) &+ chain()) &+ nestedClosures(2)) &+ strClosure("ab")
    score = (((score &+ holders()) &+ globals()) &+ findFirst(10)) &+ tryInLoop(6)
    score = (((score &+ lineLen()) &+ machineStrings(0)) &+ machineStrings(1)) &+ precedence(13, 7)
    return score % 251
}

exit(Int32(truncatingIfNeeded: main()))
