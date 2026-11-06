var counter: Int64 = 0

func bump() -> Int64 {
    counter = counter &+ 1
    return counter
}
