import Foundation

protocol Runnable: AnyObject {
    func run() -> Int64
}

func main() -> Int64 {
    return 0
}

exit(Int32(truncatingIfNeeded: main()))
