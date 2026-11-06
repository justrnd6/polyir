#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef POLYIR_CORE_H
#define POLYIR_CORE_H

// Memory: what polyir shares (a struct, a list, a map) is allocated here and never freed. `copyBytes` keeps a value on the
// heap, so an optional, a variant or a closure's environment outlives the code that made it.
static void* gcAlloc(size_t size) {
  void* memory = calloc(1, size > 0 ? size : 1);

  if (memory == NULL) {
    fputs("trap: out of memory\n", stderr);
    exit(1);
  }

  return memory;
}

static void* copyBytes(const void* from, size_t size) {
  void* to = gcAlloc(size);
  memcpy(to, from, size);

  return to;
}

// Stops the program with a message, as polyir's `fail` does.
_Noreturn static void fail(const char* message) {
  fprintf(stderr, "trap: %s\n", message);
  exit(1);
}

// `throw` and `try`: each `try` puts a handler on the stack, and a throw jumps to the latest one with its code. A check that
// fails throws polyir's code for it (-1 an index outside its container, -3 an empty optional), or stops the program when no
// `try` is there to catch it.
typedef struct Handler {
  jmp_buf jump;
  struct Handler* outer;
  int64_t code;
} Handler;

static Handler* handlers = NULL;

_Noreturn static void throwCode(int64_t code) {
  if (handlers == NULL) {
    fail("a value was thrown and nothing caught it");
  }

  handlers->code = code;
  longjmp(handlers->jump, 1);
}

_Noreturn static void checkFailed(int64_t code, const char* message) {
  if (handlers == NULL) {
    fail(message);
  }

  throwCode(code);
}

// The value an optional holds (a pointer to it).
static void* present(void* value) {
  if (value == NULL) {
    checkFailed(-3, "unwrap of an empty optional");
  }

  return value;
}

// How two values of a map's keys or a list's items compare and hash: by their bytes, unless their type has functions of its
// own (a string, a struct).
typedef bool (*SameFn)(const void* a, const void* b, size_t size);
typedef uint64_t (*HashFn)(const void* value, size_t size);

static bool sameBytes(const void* a, const void* b, size_t size) {
  return memcmp(a, b, size) == 0;
}

static uint64_t hashBytes(const void* value, size_t size) {
  uint64_t hash = 1469598103934665603u;

  for (size_t i = 0; i < size; i++) {
    hash = (hash ^ ((const unsigned char*) value)[i]) * 1099511628211u;
  }

  return hash;
}

static bool sameTextKey(const void* a, const void* b, size_t size) {
  (void) size;

  return strcmp(*(const char* const*) a, *(const char* const*) b) == 0;
}

static uint64_t hashTextKey(const void* value, size_t size) {
  (void) size;
  const char* text = *(const char* const*) value;

  return hashBytes(text, strlen(text));
}
#endif

#ifndef POLYIR_MEMORY_H
#define POLYIR_MEMORY_H

// The C library's functions polyir declares with numbers for their pointers or sizes (`extern func calloc(n: i64, size: i64)
// -> i64`), each called through a function of that shape. This file comes after the runtime, which calls the C library's own.
static int64_t polyCalloc(int64_t count, int64_t size) {
  return (int64_t) (intptr_t) calloc((size_t) count, (size_t) size);
}

static int64_t* polyMalloc(int64_t size) {
  return malloc((size_t) size);
}

static void polyFree(int64_t memory) {
  free((void*) (intptr_t) memory);
}

static int64_t polyFopen(const char* path, const char* mode) {
  return (int64_t) (intptr_t) fopen(path, mode);
}

static int64_t polyFclose(int64_t file) {
  return fclose((FILE*) (intptr_t) file);
}

static int64_t polyFread(int64_t data, int64_t size, int64_t count, int64_t file) {
  return (int64_t) fread((void*) (intptr_t) data, (size_t) size, (size_t) count, (FILE*) (intptr_t) file);
}

static int64_t polyFwrite(int64_t data, int64_t size, int64_t count, int64_t file) {
  return (int64_t) fwrite((const void*) (intptr_t) data, (size_t) size, (size_t) count, (FILE*) (intptr_t) file);
}

static int64_t polyFseek(int64_t file, int64_t offset, int64_t whence) {
  return fseek((FILE*) (intptr_t) file, (long) offset, (int) whence);
}

static int64_t polyFtell(int64_t file) {
  return ftell((FILE*) (intptr_t) file);
}

static int64_t polyGetenv(const char* name) {
  return (int64_t) (intptr_t) getenv(name);
}

static int64_t polyStrlen(const char* text) {
  return (int64_t) strlen(text);
}

static int64_t polyPutchar(int64_t c) {
  return putchar((int) c);
}

static int64_t polyPuts(const char* text) {
  return puts(text);
}

#define calloc polyCalloc
#define malloc polyMalloc
#define free polyFree
#define fopen polyFopen
#define fclose polyFclose
#define fread polyFread
#define fwrite polyFwrite
#define fseek polyFseek
#define ftell polyFtell
#define getenv polyGetenv
#define strlen polyStrlen
#define putchar polyPutchar
#define puts polyPuts
#endif

#ifndef POLYIR_LIST_H
#define POLYIR_LIST_H

// A list: its items' size, how many it holds, and its items. `LIST_AT(list, i, T)` is item `i`, to read or to write.
typedef struct List {
  size_t size;
  int64_t count;
  int64_t capacity;
  char* items;
} List;

static List* listNew(size_t size) {
  List* list = gcAlloc(sizeof(List));
  list->size = size;

  return list;
}

static void* listAt(List* list, int64_t at) {
  if (at < 0 || at >= list->count) {
    checkFailed(-1, "index out of range");
  }

  return list->items + at * list->size;
}

static void listPush(List* list, const void* item) {
  if (list->count == list->capacity) {
    list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
    char* grown = gcAlloc(list->capacity * list->size);
    memcpy(grown, list->items, list->count * list->size);
    list->items = grown;
  }

  memcpy(list->items + list->count * list->size, item, list->size);
  list->count++;
}

static List* listOf(size_t size, int64_t count, const void* items) {
  List* list = listNew(size);

  for (int64_t i = 0; i < count; i++) {
    listPush(list, (const char*) items + i * size);
  }

  return list;
}

// A list of `count` items, each `item`.
static List* listFilled(size_t size, int64_t count, const void* item) {
  List* list = listNew(size);

  for (int64_t i = 0; i < count; i++) {
    listPush(list, item);
  }

  return list;
}

static List* listCopy(List* from) {
  return listOf(from->size, from->count, from->items);
}

static List* listPushed(List* list, const void* item) {
  listPush(list, item);

  return list;
}

static List* listWith(List* from, const void* item) {
  return listPushed(listCopy(from), item);
}

static List* listConcat(List* a, List* b) {
  List* out = listCopy(a);

  for (int64_t i = 0; i < b->count; i++) {
    listPush(out, b->items + i * b->size);
  }

  return out;
}

static List* listTruncate(List* list, int64_t count) {
  list->count = count < 0 ? 0 : count < list->count ? count : list->count;

  return list;
}

static bool listHas(List* list, const void* item, SameFn same) {
  for (int64_t i = 0; i < list->count; i++) {
    if ((same != NULL ? same : sameBytes)(list->items + i * list->size, item, list->size)) {
      return true;
    }
  }

  return false;
}

// Whether two lists hold the same items, compared by `same` (by their bytes when it is NULL).
static bool listSame(List* a, List* b, SameFn same) {
  if (a == b || a->count != b->count) {
    return a == b;
  }

  for (int64_t i = 0; i < a->count; i++) {
    if (!(same != NULL ? same : sameBytes)(a->items + i * a->size, b->items + i * b->size, a->size)) {
      return false;
    }
  }

  return true;
}

#define LIST_AT(list, at, T) (*(T*) listAt((list), (at)))
#define LIST_OF(T, ...) listOf(sizeof(T), sizeof((T[]){__VA_ARGS__}) / sizeof(T), (T[]){__VA_ARGS__})
#endif

int64_t MTLCreateSystemDefaultDevice();
int64_t objc_getClass(const char* name);
int64_t sel_registerName(const char* name);
int64_t objc_msgSend(int64_t target, int64_t selector, int64_t a, int64_t b, int64_t c, int64_t d);
static int64_t commandQueue = 0;

static int64_t send(int64_t target, const char* method, int64_t a, int64_t b, int64_t c);
static int64_t nsString(const char* text);
static const char* metalReadFile(const char* path);
static int64_t metalDevice(void);
static int64_t metalPipeline(int64_t device, const char* source, const char* kernelName);
static int64_t metalBuffer(int64_t device, int64_t address, int64_t byteCount);
static int64_t metalEmptyBuffer(int64_t device, int64_t byteCount);
static int64_t metalContents(int64_t buf);
static int64_t metalDispatch(int64_t device, int64_t pipeline, int64_t count, int64_t b0, int64_t b1, int64_t b2, int64_t b3);
static int64_t metalDispatchBuffers(int64_t device, int64_t pipeline, int64_t count, int64_t* buffers, int64_t bufferCount);
static int64_t metalDispatch2D(int64_t device, int64_t pipeline, int64_t width, int64_t height, int64_t* buffers, int64_t bufferCount);
static int64_t metalDispatchGroups(int64_t device, int64_t pipeline, int64_t count, int64_t* buffers, int64_t bufferCount);
static int64_t metalPadded(int64_t count);
static int64_t metalRun(int64_t device, int64_t pipeline, int64_t* buffers, int64_t bufferCount, const char* launchMethod, int64_t sizeA, int64_t sizeB);
static int64_t metalQueue(int64_t device);
static int64_t bindBuffer(int64_t encoder, int64_t buf, int64_t slot);
static int64_t smaller(int64_t a, int64_t b);
static int64_t groupWidth(int64_t pipeline, int64_t count);

static int64_t send(int64_t target, const char* method, int64_t a, int64_t b, int64_t c) {
  return objc_msgSend(target, sel_registerName(method), a, b, c, 0);
}

static int64_t nsString(const char* text) {
  return send(objc_getClass("NSString"), "stringWithUTF8String:", (int64_t) text, 0, 0);
}

static const char* metalReadFile(const char* path) {
  int64_t size = 1048576;
  int64_t buf = calloc(size + 1, 1);
  int64_t file = fopen(path, "r");
  if (file == 0) {
    return "";
  }
  fread(buf, 1, size, file);
  fclose(file);
  return (int8_t*) (intptr_t) buf;
}

static int64_t metalDevice(void) {
  return MTLCreateSystemDefaultDevice();
}

static int64_t metalPipeline(int64_t device, const char* source, const char* kernelName) {
  List* errorSlot = listFilled(sizeof(int64_t), 1, (int64_t[]){0});
  LIST_AT(errorSlot, 0, int64_t) = 0;
  int64_t library = send(device, "newLibraryWithSource:options:error:", nsString(source), 0, (int64_t) errorSlot);
  if (library == 0) {
    return 0;
  }
  int64_t function = send(library, "newFunctionWithName:", nsString(kernelName), 0, 0);
  if (function == 0) {
    return 0;
  }
  return send(device, "newComputePipelineStateWithFunction:error:", function, (int64_t) errorSlot, 0);
}

static int64_t metalBuffer(int64_t device, int64_t address, int64_t byteCount) {
  return send(device, "newBufferWithBytes:length:options:", address, byteCount, 0);
}

static int64_t metalEmptyBuffer(int64_t device, int64_t byteCount) {
  return send(device, "newBufferWithLength:options:", byteCount, 0, 0);
}

static int64_t metalContents(int64_t buf) {
  return send(buf, "contents", 0, 0, 0);
}

static int64_t metalDispatch(int64_t device, int64_t pipeline, int64_t count, int64_t b0, int64_t b1, int64_t b2, int64_t b3) {
  int64_t* buffers = (int8_t*) (intptr_t) calloc(4, 8);
  int64_t used = 0;
  if (b0 != 0) {
    buffers[used] = b0;
    used = used + 1;
  }
  if (b1 != 0) {
    buffers[used] = b1;
    used = used + 1;
  }
  if (b2 != 0) {
    buffers[used] = b2;
    used = used + 1;
  }
  if (b3 != 0) {
    buffers[used] = b3;
    used = used + 1;
  }
  return metalDispatchBuffers(device, pipeline, count, buffers, used);
}

static int64_t metalDispatchBuffers(int64_t device, int64_t pipeline, int64_t count, int64_t* buffers, int64_t bufferCount) {
  List* total = listFilled(sizeof(int64_t), 3, (int64_t[]){0});
  LIST_AT(total, 0, int64_t) = count;
  LIST_AT(total, 1, int64_t) = 1;
  LIST_AT(total, 2, int64_t) = 1;
  List* perGroup = listFilled(sizeof(int64_t), 3, (int64_t[]){0});
  LIST_AT(perGroup, 0, int64_t) = groupWidth(pipeline, count);
  LIST_AT(perGroup, 1, int64_t) = 1;
  LIST_AT(perGroup, 2, int64_t) = 1;
  return metalRun(device, pipeline, buffers, bufferCount, "dispatchThreads:threadsPerThreadgroup:", (int64_t) total, (int64_t) perGroup);
}

static int64_t metalDispatch2D(int64_t device, int64_t pipeline, int64_t width, int64_t height, int64_t* buffers, int64_t bufferCount) {
  List* total = listFilled(sizeof(int64_t), 3, (int64_t[]){0});
  LIST_AT(total, 0, int64_t) = width;
  LIST_AT(total, 1, int64_t) = height;
  LIST_AT(total, 2, int64_t) = 1;
  List* perGroup = listFilled(sizeof(int64_t), 3, (int64_t[]){0});
  LIST_AT(perGroup, 0, int64_t) = smaller(16, width);
  LIST_AT(perGroup, 1, int64_t) = smaller(16, height);
  LIST_AT(perGroup, 2, int64_t) = 1;
  return metalRun(device, pipeline, buffers, bufferCount, "dispatchThreads:threadsPerThreadgroup:", (int64_t) total, (int64_t) perGroup);
}

static int64_t metalDispatchGroups(int64_t device, int64_t pipeline, int64_t count, int64_t* buffers, int64_t bufferCount) {
  List* groups = listFilled(sizeof(int64_t), 3, (int64_t[]){0});
  LIST_AT(groups, 0, int64_t) = (count + 255) / 256;
  LIST_AT(groups, 1, int64_t) = 1;
  LIST_AT(groups, 2, int64_t) = 1;
  List* perGroup = listFilled(sizeof(int64_t), 3, (int64_t[]){0});
  LIST_AT(perGroup, 0, int64_t) = 256;
  LIST_AT(perGroup, 1, int64_t) = 1;
  LIST_AT(perGroup, 2, int64_t) = 1;
  return metalRun(device, pipeline, buffers, bufferCount, "dispatchThreadgroups:threadsPerThreadgroup:", (int64_t) groups, (int64_t) perGroup);
}

static int64_t metalPadded(int64_t count) {
  return (count + 255) / 256 * 256;
}

static int64_t metalRun(int64_t device, int64_t pipeline, int64_t* buffers, int64_t bufferCount, const char* launchMethod, int64_t sizeA, int64_t sizeB) {
  int64_t queue = metalQueue(device);
  int64_t commands = send(queue, "commandBuffer", 0, 0, 0);
  int64_t encoder = send(commands, "computeCommandEncoder", 0, 0, 0);
  if (encoder == 0) {
    return 0;
  }
  send(encoder, "setComputePipelineState:", pipeline, 0, 0);
  for (int64_t slot = 0; slot < bufferCount; slot = slot + 1) {
    bindBuffer(encoder, buffers[slot], slot);
  }
  send(encoder, launchMethod, sizeA, sizeB, 0);
  send(encoder, "endEncoding", 0, 0, 0);
  send(commands, "commit", 0, 0, 0);
  send(commands, "waitUntilCompleted", 0, 0, 0);
  return 1;
}

static int64_t metalQueue(int64_t device) {
  if (commandQueue == 0) {
    commandQueue = send(device, "newCommandQueue", 0, 0, 0);
  }
  return commandQueue;
}

static int64_t bindBuffer(int64_t encoder, int64_t buf, int64_t slot) {
  if (buf == 0) {
    return 0;
  }
  return send(encoder, "setBuffer:offset:atIndex:", buf, 0, slot);
}

static int64_t smaller(int64_t a, int64_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static int64_t groupWidth(int64_t pipeline, int64_t count) {
  int64_t most = send(pipeline, "maxTotalThreadsPerThreadgroup", 0, 0, 0);
  if (count < most) {
    return count;
  }
  return most;
}
