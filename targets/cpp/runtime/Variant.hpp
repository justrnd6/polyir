#ifndef POLYIR_VARIANT_HPP
#define POLYIR_VARIANT_HPP

// A variant: the number of its case and the value it holds, which it can compare and hash (`makeVariant` makes one).
struct Variant {
  int64_t tag = 0;
  std::any value;
  std::function<bool(const std::any&)> holds;
  std::function<std::size_t()> hasher;

  bool operator==(const Variant& other) const {
    return tag == other.tag && (holds ? holds(other.value) : !other.value.has_value());
  }

  std::size_t hash() const {
    return tag * 31 + (hasher ? hasher() : 0);
  }
};

template <typename T>
Variant makeVariant(int64_t tag, const T& value) {
  return Variant{tag, value, [value](const std::any& other) { return other.type() == typeid(T) && same(std::any_cast<T>(other), value); },
                 [value]() { return hashOf(value); }};
}
#endif
