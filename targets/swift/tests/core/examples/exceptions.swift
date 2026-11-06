// What `throw` throws: the code thrown.
struct Thrown: Error {
    let value: Int64
}

func riskyDouble(_ x: Int64) throws -> Int64 {
    if x < 0 {
        throw Thrown(value: 99)
    }
    return x &* 2
}

func safeDouble(_ x: Int64) -> Int64 {
    var result: Int64 = 0
    do {
        result = try riskyDouble(x)
    } catch {
        let code = (error as! Thrown).value
        result = code
    }
    return result
}

func nestedTryCheck(_ x: Int64) -> Int64 {
    var result: Int64 = 0
    do {
        do {
            result = try riskyDouble(x)
        } catch {
            let innerCode = (error as! Thrown).value
            result = innerCode &+ 1
            throw Thrown(value: result)
        }
    } catch {
        let outerCode = (error as! Thrown).value
        result = outerCode &* 10
    }
    return result
}
