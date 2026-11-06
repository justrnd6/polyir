import Foundation

func sumTo(_ n: Int64) -> Int64 {
    var label: Int64 = 0
    var total: Int64 = 0
    var i: Int64 = 0
    var result: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            total = 0
            i = 1
            label = 1
            continue machine
        case 1:
            if i > n {
                result = total
                label = 3
                continue machine
            } else {
                label = 2
                continue machine
            }
        case 2:
            total = total &+ i
            i = i &+ 1
            label = 1
            continue machine
        case 3:
            return result
        default:
            fatalError("no block \(label)")
        }
    }
}

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

func collatz(_ n: Int64) -> Int64 {
    var label: Int64 = 0
    var x: Int64 = 0
    var steps: Int64 = 0
    var count: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            x = n
            steps = 0
            label = 1
            continue machine
        case 1:
            if x == 1 {
                count = steps
                label = 5
                continue machine
            } else {
                label = 2
                continue machine
            }
        case 2:
            if x % 2 == 0 {
                label = 3
                continue machine
            } else {
                label = 4
                continue machine
            }
        case 3:
            x = x / 2
            steps = steps &+ 1
            label = 1
            continue machine
        case 4:
            x = 3 &* x &+ 1
            steps = steps &+ 1
            label = 1
            continue machine
        case 5:
            return count
        default:
            fatalError("no block \(label)")
        }
    }
}

func swapped(_ a: Int64, _ b: Int64) -> Int64 {
    var label: Int64 = 0
    var x: Int64 = 0
    var y: Int64 = 0
    var k: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            x = a
            y = b
            k = 0
            label = 1
            continue machine
        case 1:
            if k == 3 {
                label = 3
                continue machine
            } else {
                label = 2
                continue machine
            }
        case 2:
            var next_x: Int64 = y
            var next_y: Int64 = x
            var next_k: Int64 = k &+ 1
            x = next_x
            y = next_y
            k = next_k
            label = 1
            continue machine
        case 3:
            return x &* 10 &+ y
        default:
            fatalError("no block \(label)")
        }
    }
}

func weight(_ day: Int64) -> Int64 {
    var label: Int64 = 0
    var w: Int64 = 0
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
            } else {
                label = 3
                continue machine
            }
        case 1:
            w = 1
            label = 4
            continue machine
        case 2:
            w = 2
            label = 4
            continue machine
        case 3:
            w = 3
            label = 4
            continue machine
        case 4:
            return w &* 100
        default:
            fatalError("no block \(label)")
        }
    }
}

func zigzag(_ a: Int64) -> Int64 {
    var label: Int64 = 0
    var p: Int64 = 0
    var q: Int64 = 0
    var r: Int64 = 0
    machine: while true {
        switch label {
        case 0:
            if a == 0 {
                p = a
                label = 1
                continue machine
            } else {
                q = a
                label = 2
                continue machine
            }
        case 1:
            let next: Int64 = p &+ 1
            if next > 5 {
                r = next
                label = 3
                continue machine
            } else {
                q = next
                label = 2
                continue machine
            }
        case 2:
            if q > 5 {
                r = q
                label = 3
                continue machine
            } else {
                p = q &+ 1
                label = 1
                continue machine
            }
        case 3:
            return r &* 2
        default:
            fatalError("no block \(label)")
        }
    }
}

func main() -> Int64 {
    var score: Int64 = sumTo(10)
    score = score &+ gcd(48, 18)
    score = score &+ collatz(6)
    score = score &+ swapped(4, 7)
    score = ((score &+ weight(0)) &+ weight(2)) &+ weight(9)
    score = (score &+ zigzag(0)) &+ zigzag(3)
    return score
}

exit(Int32(truncatingIfNeeded: main()))
