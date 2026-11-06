// A string's hash: djb2 over its bytes, wrapping at 64 bits, as the native code computes it.
pub fn hash_text(text: &str) -> i64 {
    let mut hash: u64 = 5381;

    for byte in text.bytes() {
        hash = hash.wrapping_mul(33).wrapping_add(byte as u64);
    }

    hash as i64
}
