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
