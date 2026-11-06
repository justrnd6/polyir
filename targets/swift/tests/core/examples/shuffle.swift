// An operator lane by lane on two vectors; the lanes of a vector in a new order.
fileprivate func lanes<T>(_ a: [T], _ b: [T], _ op: (T, T) -> T) -> [T] {
    zip(a, b).map { op($0, $1) }
}

fileprivate func shuffle<T>(_ v: [T], _ picked: Int...) -> [T] {
    picked.map { v[$0] }
}

func reverse4(_ a: Int64, _ b: Int64, _ c: Int64) -> Int64 {
    let v: [Int64] = [a, b, c, 40]
    let r: [Int64] = shuffle(v, 3, 2, 1, 0)
    return ((r[Int(0)] &* 1000 &+ r[Int(1)] &* 100) &+ r[Int(2)] &* 10) &+ r[Int(3)]
}

func duplicateFirstTwo(_ a: Int64, _ b: Int64, _ c: Int64) -> Int64 {
    let v: [Int64] = [a, b, c, 4]
    let r: [Int64] = shuffle(v, 0, 0, 1, 1)
    return ((r[Int(0)] &* 1000 &+ r[Int(1)] &* 100) &+ r[Int(2)] &* 10) &+ r[Int(3)]
}
