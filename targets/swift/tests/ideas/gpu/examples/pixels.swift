import Foundation

func main() -> Int64 {
    let device: Int64 = metalDevice()
    if device == 0 {
        return 10
    }
    let source: String = metalReadFile("pixels.metal")
    if source == "" {
        return 11
    }
    printTable(facePicture())
    var code: Int64 = scanDemo(device, source)
    if code != 0 {
        return code
    }
    code = channelsDemo(device, source)
    if code != 0 {
        return code
    }
    code = bitsDemo(device, source)
    if code != 0 {
        return code
    }
    code = resizeDemo(device, source)
    if code != 0 {
        return code
    }
    return enlargeDemo(device, source)
}

exit(Int32(truncatingIfNeeded: main()))
