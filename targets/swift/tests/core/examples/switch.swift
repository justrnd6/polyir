func dayKind(_ day: Int64) -> Int64 {
    var label: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            if day == 0 {
                label = 1
                continue machine
            } else if day == 6 {
                label = 1
                continue machine
            } else if day == 1 {
                label = 2
                continue machine
            } else if day == 2 {
                label = 2
                continue machine
            } else if day == 3 {
                label = 2
                continue machine
            } else if day == 4 {
                label = 2
                continue machine
            } else if day == 5 {
                label = 2
                continue machine
            } else {
                label = 3
                continue machine
            }
        case 1:
            return 0
        case 2:
            return 1
        case 3:
            return 0 &- 1
        default:
            fatalError("no block \(label)")
        }
    }
}
