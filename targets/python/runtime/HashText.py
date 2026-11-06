# A string's hash: djb2 over its UTF-8 bytes, wrapping at 64 bits, as the native code computes it.
def hash_text(text: str) -> int:
    value = 5381
    for byte in text.encode():
        value = (value * 33 + byte + 2**63) % 2**64 - 2**63
    return value
