func bitwisePrecedenceCheck() -> Int64 {
    let a: Int64 = 6
    let b: Int64 = 3
    let c: Int64 = 1
    let d: Int64 = 2
    return a | (b & c ^ d)
}

func shiftPrecedenceCheck() -> Int64 {
    let x: Int64 = 1
    return x << 2 & 3
}
