#ifndef POLYIR_CLOSURE_H
#define POLYIR_CLOSURE_H

// A closure: its function and its environment, which the function takes first. The function is kept as any function pointer
// and called as its own type.
typedef void (*AnyFn)(void);

typedef struct Closure {
  AnyFn fn;
  void* env;
} Closure;
#endif
