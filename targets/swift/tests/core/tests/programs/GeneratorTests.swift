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

var made: Int64 = 0

func upTo(_ limit: Int64) -> Coroutine<Int64> {
    Coroutine { coroutine in
        do {
            var i: Int64 = 0
            while i < limit {
                if coroutine.give(i) {
                    return
                }
                i = i &+ 1
            }
        }
    }
}

func counted(_ limit: Int64) -> Coroutine<Int64> {
    made = made &+ 1
    return upTo(limit)
}

func checkSumsEveryValue() -> Double {
    var total: Int64 = 0
    let forGen_21_10: Coroutine<Int64> = upTo(5)
    var forStep_21_10: Variant = forGen_21_10.resume()
    while forStep_21_10.tag == 0 {
        let n: Int64 = forStep_21_10.value as! Int64
        total = total &+ n
        forStep_21_10 = forGen_21_10.resume()
    }
    return offInt(total, 10)
}

func checkEmptyCoroutineRunsNoTurns() -> Double {
    var turns: Int64 = 0
    let forGen_29_10: Coroutine<Int64> = upTo(0)
    var forStep_29_10: Variant = forGen_29_10.resume()
    while forStep_29_10.tag == 0 {
        let n: Int64 = forStep_29_10.value as! Int64
        turns = turns &+ 1
        forStep_29_10 = forGen_29_10.resume()
    }
    return offInt(turns, 0)
}

func firstAbove(_ limit: Int64) -> Int64 {
    let forGen_36_10: Coroutine<Int64> = upTo(100)
    var forStep_36_10: Variant = forGen_36_10.resume()
    while forStep_36_10.tag == 0 {
        let n: Int64 = forStep_36_10.value as! Int64
        if n > limit {
            forGen_36_10.close()
            return n
        }
        forStep_36_10 = forGen_36_10.resume()
    }
    return 0 &- 1
}

func checkReturnLeavesMidway() -> Double {
    return offInt(firstAbove(6), 7)
}

func checkLoopsNest() -> Double {
    var total: Int64 = 0
    let forGen_50_10: Coroutine<Int64> = upTo(3)
    var forStep_50_10: Variant = forGen_50_10.resume()
    while forStep_50_10.tag == 0 {
        let a: Int64 = forStep_50_10.value as! Int64
        let forGen_51_14: Coroutine<Int64> = upTo(2)
        var forStep_51_14: Variant = forGen_51_14.resume()
        while forStep_51_14.tag == 0 {
            let b: Int64 = forStep_51_14.value as! Int64
            total = (total &+ a &* 10) &+ b
            forStep_51_14 = forGen_51_14.resume()
        }
        forStep_50_10 = forGen_50_10.resume()
    }
    return offInt(total, 63)
}

func checkSourceIsEvaluatedOnce() -> Double {
    made = 0
    var total: Int64 = 0
    let forGen_61_10: Coroutine<Int64> = counted(4)
    var forStep_61_10: Variant = forGen_61_10.resume()
    while forStep_61_10.tag == 0 {
        let n: Int64 = forStep_61_10.value as! Int64
        total = total &+ n
        forStep_61_10 = forGen_61_10.resume()
    }
    return offInt(made &* 100 &+ total, 106)
}
