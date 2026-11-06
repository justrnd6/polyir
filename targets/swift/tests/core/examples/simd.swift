// An operator lane by lane on two vectors; the lanes of a vector in a new order.
fileprivate func lanes<T>(_ a: [T], _ b: [T], _ op: (T, T) -> T) -> [T] {
    zip(a, b).map { op($0, $1) }
}

fileprivate func shuffle<T>(_ v: [T], _ picked: Int...) -> [T] {
    picked.map { v[$0] }
}

func vecMulSum(_ a: Double, _ b: Double) -> Double {
    let v1: [Double] = [a, a + 1.0, a + 2.0, a + 3.0]
    let v2: [Double] = [b, b, b, b]
    let p: [Double] = lanes(v1, v2) { $0 * $1 }
    return p[Int(0)] + p[Int(1)] + p[Int(2)] + p[Int(3)]
}

func sumI64Vec(_ a: Int64, _ b: Int64, _ c: Int64) -> Int64 {
    let v: [Int64] = [a, b, c]
    return (v[Int(0)] &+ v[Int(1)]) &+ v[Int(2)]
}

func vecMulSumViaVar(_ a: Double, _ b: Double) -> Double {
    let v1: [Double] = [a, a + 1.0, a + 2.0, a + 3.0]
    let v2: [Double] = [b, b, b, b]
    let p: [Double] = lanes(v1, v2) { $0 * $1 }
    var total: Double = 0.0
    do {
        var i: Int64 = 0
        while i < 4 {
            total = total + p[Int(i)]
            i = i &+ 1
        }
    }
    return total
}
