import Foundation

func main() -> Int64 {
    let device: Int64 = metalDevice()
    if device == 0 {
        return 10
    }
    let source: String = metalReadFile("classics.metal")
    if source == "" {
        return 11
    }
    return runAll(device, source)
}

exit(Int32(truncatingIfNeeded: main()))
