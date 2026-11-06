#ifndef POLYIR_HASH_TEXT_HPP
#define POLYIR_HASH_TEXT_HPP

// A string's hash: djb2 over its bytes, wrapping at 64 bits, as the native code computes it.
inline int64_t hashText(const std::string& text) {
  uint64_t hash = 5381;
  for (const char c : text) {
    hash = hash * 33 + (uint8_t) c;
  }
  return (int64_t) hash;
}
#endif
