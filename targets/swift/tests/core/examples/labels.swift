func gcd(_ a0: Int64, _ b0: Int64) -> Int64 {
    var label: Int64 = 0
    var a: Int64 = 0
    var b: Int64 = 0
    var result: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            a = a0
            b = b0
            label = 1
            continue machine
        case 1:
            if b == 0 {
                result = a
                label = 3
                continue machine
            } else {
                label = 2
                continue machine
            }
        case 2:
            let r: Int64 = a % b
            a = b
            b = r
            label = 1
            continue machine
        case 3:
            return result
        default:
            fatalError("no block \(label)")
        }
    }
}
