import Foundation

func main() -> Int64 {
    let gpu: Int64 = vulkanStart()
    if gpu == 0 {
        return 10
    }
    printTable(facePicture())
    var code: Int64 = scanDemo(gpu, "")
    if code != 0 {
        return code
    }
    code = channelsDemo(gpu, "")
    if code != 0 {
        return code
    }
    code = bitsDemo(gpu, "")
    if code != 0 {
        return code
    }
    code = resizeDemo(gpu, "")
    if code != 0 {
        return code
    }
    return enlargeDemo(gpu, "")
}

exit(Int32(truncatingIfNeeded: main()))
