import Foundation

protocol Runnable: AnyObject {
    func run() -> Int64
}

final class Job: Runnable, Hashable {
    let id: Int64

    init(_ id: Int64) {
        self.id = id
    }

    func run() -> Int64 {
        return self.id
    }

    static func == (a: Job, b: Job) -> Bool {
        [true, a.id == b.id].allSatisfy { $0 }
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
}

func main() -> Int64 {
    return 0
}

exit(Int32(truncatingIfNeeded: main()))
