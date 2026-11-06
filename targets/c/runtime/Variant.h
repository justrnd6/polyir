#ifndef POLYIR_VARIANT_H
#define POLYIR_VARIANT_H

// A variant: the number of its case and the value it holds, kept on the heap with its size.
typedef struct Variant {
  int64_t tag;
  void* value;
  size_t size;
} Variant;

static Variant makeVariant(int64_t tag, const void* value, size_t size) {
  return (Variant){tag, size > 0 ? copyBytes(value, size) : NULL, size};
}

// Two variants are the same when they are of one case and hold the same bytes.
static bool variantSame(Variant a, Variant b) {
  return a.tag == b.tag && a.size == b.size && (a.size == 0 || memcmp(a.value, b.value, a.size) == 0);
}
#endif
