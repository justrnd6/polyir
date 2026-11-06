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

func evens(_ limit: Int64) -> Coroutine<Int64> {
    Coroutine { coroutine in
        do {
            var i: Int64 = 0
            while i < limit {
                if coroutine.give(i) {
                    return
                }
                i = i &+ 2
            }
        }
    }
}

func sumOfEvens() -> Int64 {
    var total: Int64 = 0
    let forGen_14_8: Coroutine<Int64> = evens(10)
    var forStep_14_8: Variant = forGen_14_8.resume()
    while forStep_14_8.tag == 0 {
        let n: Int64 = forStep_14_8.value as! Int64
        total = total &+ n
        forStep_14_8 = forGen_14_8.resume()
    }
    return total
}

func firstOver(_ limit: Int64) -> Int64 {
    let forGen_22_8: Coroutine<Int64> = evens(100)
    var forStep_22_8: Variant = forGen_22_8.resume()
    while forStep_22_8.tag == 0 {
        let n: Int64 = forStep_22_8.value as! Int64
        if n > limit {
            forGen_22_8.close()
            return n
        }
        forStep_22_8 = forGen_22_8.resume()
    }
    return 0 &- 1
}

func pairs() -> Int64 {
    var total: Int64 = 0
    let forGen_33_8: Coroutine<Int64> = evens(6)
    var forStep_33_8: Variant = forGen_33_8.resume()
    while forStep_33_8.tag == 0 {
        let a: Int64 = forStep_33_8.value as! Int64
        let forGen_34_10: Coroutine<Int64> = evens(4)
        var forStep_34_10: Variant = forGen_34_10.resume()
        while forStep_34_10.tag == 0 {
            let b: Int64 = forStep_34_10.value as! Int64
            total = (total &+ a &* 10) &+ b
            forStep_34_10 = forGen_34_10.resume()
        }
        forStep_33_8 = forGen_33_8.resume()
    }
    return total
}

func main() -> Int64 {
    return (sumOfEvens() &+ firstOver(5)) &+ pairs()
}

exit(Int32(truncatingIfNeeded: main()))
