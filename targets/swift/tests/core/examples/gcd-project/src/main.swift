import Foundation

func main() -> Int64 {
    return gcd(48, 18) &* 10 &+ lcm(4, 6)
}

exit(Int32(truncatingIfNeeded: main()))
