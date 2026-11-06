import Foundation

func main() -> Int64 {
    let gpu: Int64 = vulkanStart()
    if gpu == 0 {
        return 10
    }
    return runAll(gpu, "")
}

exit(Int32(truncatingIfNeeded: main()))
