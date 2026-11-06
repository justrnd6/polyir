// An operator lane by lane on two vectors; the lanes of a vector in a new order.
fileprivate func lanes<T>(_ a: [T], _ b: [T], _ op: (T, T) -> T) -> [T] {
    zip(a, b).map { op($0, $1) }
}

fileprivate func shuffle<T>(_ v: [T], _ picked: Int...) -> [T] {
    picked.map { v[$0] }
}
