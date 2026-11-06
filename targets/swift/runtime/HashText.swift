// A string's hash: djb2 over its UTF-8 bytes, wrapping at 64 bits, as the native code computes it.
fileprivate func hashText(_ text: String) -> Int64 {
    var hash: Int64 = 5381
    for byte in text.utf8 {
        hash = hash &* 33 &+ Int64(byte)
    }
    return hash
}
