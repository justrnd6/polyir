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
