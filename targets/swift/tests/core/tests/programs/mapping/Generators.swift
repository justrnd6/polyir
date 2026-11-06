import Foundation

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

func fibonacci(_ count: Int64) -> Coroutine<Int64> {
    Coroutine { coroutine in
        var a: Int64 = 0
        var b: Int64 = 1
        do {
            var i: Int64 = 0
            while i < count {
                if coroutine.give(a) {
                    return
                }
                let next: Int64 = a &+ b
                a = b
                b = next
                i = i &+ 1
            }
        }
    }
}

func countdown(_ from: Int64) -> Coroutine<Int64> {
    Coroutine { coroutine in
        var n: Int64 = from
        do {
            var i: Int64 = 0
            while i < from {
                if coroutine.give(n) {
                    return
                }
                n = n &- 1
                i = i &+ 1
            }
        }
    }
}

func total(_ source: Coroutine<Int64>) -> Int64 {
    var sum: Int64 = 0
    var finished: Bool = false
    do {
        var i: Int64 = 0
        while i < 100 && finished == false {
            let step: Variant = source.resume()
            if step.tag == 1 {
                finished = true
            } else {
                sum = sum &+ (step.value as! Int64)
            }
            i = i &+ 1
        }
    }
    return sum
}

func main() -> Int64 {
    var score: Int64 = total(fibonacci(7))
    score = score &+ total(countdown(4))
    let two: Coroutine<Int64> = countdown(3)
    let first: Variant = two.resume()
    let second: Variant = two.resume()
    score = (score &+ (first.value as! Int64) &* 10) &+ (second.value as! Int64)
    if two.done() == false {
        score = score &+ 100
    }
    return score
}

exit(Int32(truncatingIfNeeded: main()))
