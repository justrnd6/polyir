func gcd(_ a: Int64, _ b: Int64) -> Int64 {
    var x: Int64 = a
    var y: Int64 = b
    while y != 0 {
        let r: Int64 = x % y
        x = y
        y = r
    }
    return x
}

func lcm(_ a: Int64, _ b: Int64) -> Int64 {
    return a / gcd(a, b) &* b
}
