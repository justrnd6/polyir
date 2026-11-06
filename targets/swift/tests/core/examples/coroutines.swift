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

func main() -> Int64 {
    let fib: Coroutine<Int64> = fibonacci(7)
    var sum: Int64 = 0
    var done: Bool = false
    do {
        var i: Int64 = 0
        while i < 8 && done == false {
            let step: Variant = fib.resume()
            if step.tag == 1 {
                done = true
            } else {
                let value: Int64 = step.value as! Int64
                putchar(Int32(truncatingIfNeeded: 48 &+ value))
                sum = sum &+ value
            }
            i = i &+ 1
        }
    }
    putchar(Int32(truncatingIfNeeded: 10))
    if fib.done() == false {
        return 0 &- 1
    }
    return sum
}

exit(Int32(truncatingIfNeeded: main()))
