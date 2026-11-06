func classify(_ x: Int64) -> Int64 {
    return x < 0 ? 0 &- 1 : x == 0 ? 0 : 1
}

func maxVal(_ a: Int64, _ b: Int64) -> Int64 {
    var result: Int64 = 0
    if a < b {
        result = b
    } else {
        result = a
    }
    return result
}

func absViaIf(_ x: Int64) -> Int64 {
    var r: Int64 = x
    if r < 0 {
        r = 0 &- r
    }
    return r
}
