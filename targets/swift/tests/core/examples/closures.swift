func counterTwice() -> Int64 {
    var count: Int64 = 0
    let inc: () -> Int64 = { () -> Int64 in
        count = count &+ 1
        return count
    }
    let a: Int64 = inc()
    let b: Int64 = inc()
    return a &* 100 &+ b
}

func makeMultiplier(_ factor: Int64) -> (Int64) -> Int64 {
    let mul: (Int64) -> Int64 = { (x: Int64) in x &* factor }
    return mul
}

func tripleOf(_ x: Int64) -> Int64 {
    let triple: (Int64) -> Int64 = makeMultiplier(3)
    return triple(x)
}

func sharedCounter() -> Int64 {
    var shared: Int64 = 0
    let bumpBy1: () -> Int64 = { () -> Int64 in
        shared = shared &+ 1
        return shared
    }
    let bumpBy10: () -> Int64 = { () -> Int64 in
        shared = shared &+ 10
        return shared
    }
    let a: Int64 = bumpBy1()
    let b: Int64 = bumpBy10()
    let c: Int64 = bumpBy1()
    return (a &* 10000 &+ b &* 100) &+ c
}
