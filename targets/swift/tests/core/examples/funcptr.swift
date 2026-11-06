func doubleIt(_ x: Int64) -> Int64 {
    return x &* 2
}

func applyTwice(_ f: (Int64) -> Int64, _ x: Int64) -> Int64 {
    let once: Int64 = f(x)
    return f(once)
}

func applyDoubleTwice(_ x: Int64) -> Int64 {
    return applyTwice(doubleIt, x)
}

func directDoubleCheck(_ x: Int64) -> Int64 {
    return doubleIt(x)
}

func addOne(_ x: Int64) -> Int64 {
    return x &+ 1
}

func pickAndApply(_ x: Int64, _ useAddOne: Int64) -> Int64 {
    var fn: (Int64) -> Int64 = doubleIt
    if useAddOne != 0 {
        fn = addOne
    }
    return fn(x)
}

func applyNTimes(_ f: (Int64) -> Int64, _ x: Int64, _ n: Int64) -> Int64 {
    var acc: Int64 = x
    do {
        var i: Int64 = 0
        while i < n {
            acc = f(acc)
            i = i &+ 1
        }
    }
    return acc
}

func doubleFourTimes(_ x: Int64) -> Int64 {
    return applyNTimes(doubleIt, x, 4)
}
