#ifndef POLYIR_ATOMICS_H
#define POLYIR_ATOMICS_H

// A compare-and-swap that gives the value it found.
static int64_t compareSwap(int64_t* at, int64_t expected, int64_t value) {
  atomic_compare_exchange_strong((_Atomic int64_t*) at, &expected, value);

  return expected;
}

#endif
