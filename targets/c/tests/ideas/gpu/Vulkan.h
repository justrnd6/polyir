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

int32_t vkCreateInstance(int64_t info, int64_t allocator, int64_t out);
int32_t vkEnumeratePhysicalDevices(int64_t instance, int64_t count, int64_t out);
int64_t vkGetPhysicalDeviceProperties(int64_t physical, int64_t out);
int64_t vkGetPhysicalDeviceMemoryProperties(int64_t physical, int64_t out);
int32_t vkCreateDevice(int64_t physical, int64_t info, int64_t allocator, int64_t out);
int64_t vkGetDeviceQueue(int64_t device, int64_t family, int64_t at, int64_t out);
int32_t vkCreateBuffer(int64_t device, int64_t info, int64_t allocator, int64_t out);
int64_t vkGetBufferMemoryRequirements(int64_t device, int64_t buf, int64_t out);
int32_t vkAllocateMemory(int64_t device, int64_t info, int64_t allocator, int64_t out);
int32_t vkBindBufferMemory(int64_t device, int64_t buf, int64_t memory, int64_t offset);
int32_t vkMapMemory(int64_t device, int64_t memory, int64_t offset, int64_t size, int64_t flags, int64_t out);
int32_t vkCreateShaderModule(int64_t device, int64_t info, int64_t allocator, int64_t out);
int32_t vkCreateDescriptorSetLayout(int64_t device, int64_t info, int64_t allocator, int64_t out);
int32_t vkCreatePipelineLayout(int64_t device, int64_t info, int64_t allocator, int64_t out);
int32_t vkCreateComputePipelines(int64_t device, int64_t cache, int64_t count, int64_t infos, int64_t allocator, int64_t out);
int32_t vkCreateDescriptorPool(int64_t device, int64_t info, int64_t allocator, int64_t out);
int32_t vkAllocateDescriptorSets(int64_t device, int64_t info, int64_t out);
int64_t vkUpdateDescriptorSets(int64_t device, int64_t writeCount, int64_t writes, int64_t copyCount, int64_t copies);
int32_t vkCreateCommandPool(int64_t device, int64_t info, int64_t allocator, int64_t out);
int32_t vkAllocateCommandBuffers(int64_t device, int64_t info, int64_t out);
int32_t vkBeginCommandBuffer(int64_t commands, int64_t info);
int64_t vkCmdBindPipeline(int64_t commands, int64_t bindPoint, int64_t pipeline);
int64_t vkCmdBindDescriptorSets(int64_t commands, int64_t bindPoint, int64_t layout, int64_t first, int64_t count, int64_t sets, int64_t dynamicCount, int64_t dynamic);
int64_t vkCmdPushConstants(int64_t commands, int64_t layout, int64_t stages, int64_t offset, int64_t size, int64_t data);
int64_t vkCmdDispatch(int64_t commands, int64_t x, int64_t y, int64_t z);
int32_t vkEndCommandBuffer(int64_t commands);
int32_t vkQueueSubmit(int64_t queue, int64_t count, int64_t submits, int64_t fence);
int32_t vkQueueWaitIdle(int64_t queue);

static int64_t words(int64_t count);
static int64_t put(int64_t block, int64_t at, int64_t value);
static int64_t word(int64_t block, int64_t at);
static int64_t half(int64_t block, int64_t at);
static int64_t pair(int64_t lo, int64_t hi);
static int64_t slot(void);
static int64_t first(int64_t block);
static int64_t vulkanStart(void);
static int64_t vulkanStartOn(int64_t instance, int64_t gpuNumber);
static int64_t vulkanGpuCount(int64_t instance);
static const char* vulkanGpuName(int64_t ctx);
static int64_t vulkanInstance(void);
static int64_t vulkanPhysicalAt(int64_t instance, int64_t gpuNumber);
static int64_t vulkanDevice(int64_t physical);
static int64_t vulkanDeviceWith(int64_t physical, int64_t subset, int64_t int64);
static int64_t vulkanCommandPool(int64_t device);
static int64_t vulkanBuffer(int64_t ctx, int64_t address, int64_t byteCount);
static int64_t copyBytes_(int64_t from, int64_t to, int64_t byteCount);
static int64_t vulkanEmptyBuffer(int64_t ctx, int64_t byteCount);
static int64_t vulkanBackBuffer(int64_t ctx, int64_t buf, int64_t byteCount);
static int64_t vulkanMapped(int64_t device, int64_t buf, int64_t memory, int64_t byteCount);
static int64_t vulkanMemoryType(int64_t physical, int64_t typeBits);
static int64_t vulkanContents(int64_t buf);
static int64_t vulkanPipeline(int64_t ctx, const char* spvPath, int64_t bufferCount, int64_t pushBytes);
static int64_t vulkanShaderModule(int64_t device, const char* path);
static int64_t shaderBytes(int64_t code, int64_t size);
static int64_t hexDigit(int64_t c);
static int64_t vulkanSetLayout(int64_t device, int64_t bufferCount);
static int64_t vulkanPipelineLayout(int64_t device, int64_t setLayout, int64_t pushBytes);
static int64_t vulkanCompute(int64_t device, int64_t module, int64_t layout, int64_t setLayout, int64_t pushBytes);
static int64_t vulkanDispatch(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t b0, int64_t b1, int64_t b2, int64_t b3);
static int64_t vulkanDispatchBuffers(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t* buffers, int64_t bufferCount);
static int64_t vulkanDispatch2D(int64_t ctx, int64_t pipe, int64_t width, int64_t height, int64_t push, int64_t* buffers, int64_t bufferCount);
static int64_t vulkanDispatchGroups(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t* buffers, int64_t bufferCount);
static int64_t vulkanLaunch(int64_t ctx, int64_t pipe, int64_t groupsX, int64_t groupsY, int64_t push, int64_t* buffers, int64_t bufferCount);
static int64_t vulkanLaunchAsync(int64_t ctx, int64_t pipe, int64_t groupsX, int64_t groupsY, int64_t push, int64_t* buffers, int64_t bufferCount);
static int64_t vulkanDispatchBuffersAsync(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t* buffers, int64_t bufferCount);
static int64_t vulkanFinish(int64_t ctx);
static int64_t vulkanDescriptors(int64_t device, int64_t pipe, int64_t* buffers, int64_t bufferCount);
static int64_t bindAll(int64_t device, int64_t dset, int64_t* buffers, int64_t bufferCount);
static int64_t writeBinding(int64_t device, int64_t dset, int64_t binding, int64_t buf);
static int64_t vulkanCommands(int64_t ctx, int64_t pipe, int64_t dset, int64_t groupsX, int64_t groupsY, int64_t push);
static int64_t record(int64_t commands, int64_t pipe, int64_t dset, int64_t groupsX, int64_t groupsY, int64_t push);
static int64_t vulkanSubmitOnly(int64_t queue, int64_t commands);
static int64_t vulkanSubmit(int64_t queue, int64_t commands);

static int64_t words(int64_t count) {
  return calloc(count, 8);
}

static int64_t put(int64_t block, int64_t at, int64_t value) {
  int64_t* w = (int8_t*) (intptr_t) block;
  w[at] = value;
  return 0;
}

static int64_t word(int64_t block, int64_t at) {
  int64_t* w = (int8_t*) (intptr_t) block;
  return w[at];
}

static int64_t half(int64_t block, int64_t at) {
  int32_t* h = (int8_t*) (intptr_t) block;
  return (int64_t) h[at];
}

static int64_t pair(int64_t lo, int64_t hi) {
  return lo + hi * 4294967296;
}

static int64_t slot(void) {
  return words(1);
}

static int64_t first(int64_t block) {
  return word(block, 0);
}

static int64_t vulkanStart(void) {
  int64_t instance = vulkanInstance();
  if (instance == 0) {
    return 0;
  }
  return vulkanStartOn(instance, 0);
}

static int64_t vulkanStartOn(int64_t instance, int64_t gpuNumber) {
  int64_t physical = vulkanPhysicalAt(instance, gpuNumber);
  if (physical == 0) {
    return 0;
  }
  int64_t device = vulkanDevice(physical);
  if (device == 0) {
    return 0;
  }
  int64_t ctx = words(4);
  put(ctx, 0, physical);
  put(ctx, 1, device);
  int64_t queue = slot();
  vkGetDeviceQueue(device, 0, 0, queue);
  put(ctx, 2, first(queue));
  put(ctx, 3, vulkanCommandPool(device));
  return ctx;
}

static int64_t vulkanGpuCount(int64_t instance) {
  int64_t count = words(1);
  put(count, 0, 4);
  int64_t found = words(4);
  if (vkEnumeratePhysicalDevices(instance, count, found) < 0) {
    return 0;
  }
  return half(count, 0);
}

static const char* vulkanGpuName(int64_t ctx) {
  int64_t properties = words(128);
  vkGetPhysicalDeviceProperties(word(ctx, 0), properties);
  const char* name = (int8_t*) (intptr_t) (properties + 20);
  return name;
}

static int64_t vulkanInstance(void) {
  int64_t extensions = words(1);
  put(extensions, 0, (int64_t) "VK_KHR_portability_enumeration");
  int64_t info = words(8);
  put(info, 0, 1);
  put(info, 2, 1);
  put(info, 6, 1);
  put(info, 7, extensions);
  int64_t out = slot();
  if (vkCreateInstance(info, 0, out) != 0) {
    return 0;
  }
  return first(out);
}

static int64_t vulkanPhysicalAt(int64_t instance, int64_t gpuNumber) {
  int64_t count = words(1);
  put(count, 0, 4);
  int64_t found = words(4);
  if (vkEnumeratePhysicalDevices(instance, count, found) < 0) {
    return 0;
  }
  if (gpuNumber >= half(count, 0)) {
    return 0;
  }
  return word(found, gpuNumber);
}

static int64_t vulkanDevice(int64_t physical) {
  int64_t device = vulkanDeviceWith(physical, 1, 1);
  if (device == 0) {
    device = vulkanDeviceWith(physical, 1, 0);
  }
  if (device == 0) {
    device = vulkanDeviceWith(physical, 0, 1);
  }
  if (device == 0) {
    device = vulkanDeviceWith(physical, 0, 0);
  }
  return device;
}

static int64_t vulkanDeviceWith(int64_t physical, int64_t subset, int64_t int64) {
  float* priority = (int8_t*) (intptr_t) words(1);
  priority[0] = 1.0f;
  int64_t queueInfo = words(5);
  put(queueInfo, 0, 2);
  put(queueInfo, 3, 1);
  put(queueInfo, 4, (int64_t) (intptr_t) priority);
  int64_t extensions = words(1);
  put(extensions, 0, (int64_t) "VK_KHR_portability_subset");
  int32_t* features = (int8_t*) (intptr_t) words(28);
  features[40] = 1;
  int64_t info = words(10);
  put(info, 0, 3);
  put(info, 2, pair(0, 1));
  put(info, 3, queueInfo);
  put(info, 6, subset);
  put(info, 7, extensions);
  if (int64 == 1) {
    put(info, 8, (int64_t) (intptr_t) features);
  }
  int64_t out = slot();
  if (vkCreateDevice(physical, info, 0, out) != 0) {
    return 0;
  }
  return first(out);
}

static int64_t vulkanCommandPool(int64_t device) {
  int64_t info = words(3);
  put(info, 0, 39);
  int64_t out = slot();
  if (vkCreateCommandPool(device, info, 0, out) != 0) {
    return 0;
  }
  return first(out);
}

static int64_t vulkanBuffer(int64_t ctx, int64_t address, int64_t byteCount) {
  int64_t buf = vulkanEmptyBuffer(ctx, byteCount);
  if (buf == 0) {
    return 0;
  }
  copyBytes_(address, word(buf, 2), byteCount);
  return buf;
}

static int64_t copyBytes_(int64_t from, int64_t to, int64_t byteCount) {
  int8_t* src = (int8_t*) (intptr_t) from;
  int8_t* dst = (int8_t*) (intptr_t) to;
  for (int64_t i = 0; i < byteCount; i = i + 1) {
    dst[i] = src[i];
  }
  return 0;
}

static int64_t vulkanEmptyBuffer(int64_t ctx, int64_t byteCount) {
  int64_t device = word(ctx, 1);
  int64_t info = words(7);
  put(info, 0, 12);
  put(info, 3, byteCount);
  put(info, 4, 32);
  int64_t out = slot();
  if (vkCreateBuffer(device, info, 0, out) != 0) {
    return 0;
  }
  return vulkanBackBuffer(ctx, first(out), byteCount);
}

static int64_t vulkanBackBuffer(int64_t ctx, int64_t buf, int64_t byteCount) {
  int64_t device = word(ctx, 1);
  int64_t need = words(3);
  vkGetBufferMemoryRequirements(device, buf, need);
  int64_t kind_ = vulkanMemoryType(word(ctx, 0), half(need, 4));
  if (kind_ < 0) {
    return 0;
  }
  int64_t alloc = words(4);
  put(alloc, 0, 5);
  put(alloc, 2, word(need, 0));
  put(alloc, 3, kind_);
  int64_t memory = slot();
  if (vkAllocateMemory(device, alloc, 0, memory) != 0) {
    return 0;
  }
  vkBindBufferMemory(device, buf, first(memory), 0);
  return vulkanMapped(device, buf, first(memory), byteCount);
}

static int64_t vulkanMapped(int64_t device, int64_t buf, int64_t memory, int64_t byteCount) {
  int64_t where = slot();
  if (vkMapMemory(device, memory, 0, 0 - 1, 0, where) != 0) {
    return 0;
  }
  int64_t block = words(4);
  put(block, 0, buf);
  put(block, 1, memory);
  put(block, 2, first(where));
  put(block, 3, byteCount);
  return block;
}

static int64_t vulkanMemoryType(int64_t physical, int64_t typeBits) {
  int64_t props = words(65);
  vkGetPhysicalDeviceMemoryProperties(physical, props);
  for (int64_t i = 0; i < half(props, 0); i = i + 1) {
    int64_t usable = (typeBits >> i) % 2;
    int64_t flags = half(props, 1 + 2 * i);
    if (usable == 1 && flags % 8 >= 6) {
      return i;
    }
  }
  return 0 - 1;
}

static int64_t vulkanContents(int64_t buf) {
  return word(buf, 2);
}

static int64_t vulkanPipeline(int64_t ctx, const char* spvPath, int64_t bufferCount, int64_t pushBytes) {
  int64_t device = word(ctx, 1);
  int64_t module = vulkanShaderModule(device, spvPath);
  if (module == 0) {
    return 0;
  }
  int64_t setLayout = vulkanSetLayout(device, bufferCount);
  if (setLayout == 0) {
    return 0;
  }
  int64_t layout = vulkanPipelineLayout(device, setLayout, pushBytes);
  if (layout == 0) {
    return 0;
  }
  return vulkanCompute(device, module, layout, setLayout, pushBytes);
}

static int64_t vulkanShaderModule(int64_t device, const char* path) {
  int64_t code = calloc(1048576, 1);
  int64_t file = fopen(path, "rb");
  if (file == 0) {
    return 0;
  }
  int64_t read = fread(code, 1, 1048576, file);
  fclose(file);
  int64_t size = shaderBytes(code, read);
  if (size == 0) {
    return 0;
  }
  int64_t info = words(5);
  put(info, 0, 16);
  put(info, 3, size);
  put(info, 4, code);
  int64_t out = slot();
  if (vkCreateShaderModule(device, info, 0, out) != 0) {
    return 0;
  }
  return first(out);
}

static int64_t shaderBytes(int64_t code, int64_t size) {
  int8_t* bytes = (int8_t*) (intptr_t) code;
  if (size == 0 || bytes[0] == 3) {
    return size;
  }
  int64_t count = 0;
  int64_t pending = 0 - 1;
  for (int64_t i = 0; i < size; i = i + 1) {
    int64_t digit = hexDigit((int64_t) bytes[i]);
    if (digit >= 0 && pending < 0) {
      pending = digit;
    } else if (digit >= 0) {
      bytes[count] = (int8_t) (pending * 16 + digit);
      count = count + 1;
      pending = 0 - 1;
    }
  }
  return count;
}

static int64_t hexDigit(int64_t c) {
  if (c >= 48 && c <= 57) {
    return c - 48;
  }
  if (c >= 97 && c <= 102) {
    return c - 87;
  }
  if (c >= 65 && c <= 70) {
    return c - 55;
  }
  return 0 - 1;
}

static int64_t vulkanSetLayout(int64_t device, int64_t bufferCount) {
  int64_t bindings = words(3 * bufferCount);
  for (int64_t i = 0; i < bufferCount; i = i + 1) {
    put(bindings, 3 * i, pair(i, 7));
    put(bindings, 3 * i + 1, pair(1, 32));
  }
  int64_t info = words(4);
  put(info, 0, 32);
  put(info, 2, pair(0, bufferCount));
  put(info, 3, bindings);
  int64_t out = slot();
  if (vkCreateDescriptorSetLayout(device, info, 0, out) != 0) {
    return 0;
  }
  return first(out);
}

static int64_t vulkanPipelineLayout(int64_t device, int64_t setLayout, int64_t pushBytes) {
  int64_t sets = words(1);
  put(sets, 0, setLayout);
  int64_t range = words(2);
  put(range, 0, pair(32, 0));
  put(range, 1, pushBytes);
  int64_t info = words(6);
  put(info, 0, 30);
  put(info, 2, pair(0, 1));
  put(info, 3, sets);
  put(info, 4, 1);
  put(info, 5, range);
  int64_t out = slot();
  if (vkCreatePipelineLayout(device, info, 0, out) != 0) {
    return 0;
  }
  return first(out);
}

static int64_t vulkanCompute(int64_t device, int64_t module, int64_t layout, int64_t setLayout, int64_t pushBytes) {
  int64_t info = words(12);
  put(info, 0, 29);
  put(info, 3, 18);
  put(info, 5, pair(0, 32));
  put(info, 6, module);
  put(info, 7, (int64_t) "main");
  put(info, 9, layout);
  int64_t out = slot();
  if (vkCreateComputePipelines(device, 0, 1, info, 0, out) != 0) {
    return 0;
  }
  int64_t pipe = words(4);
  put(pipe, 0, first(out));
  put(pipe, 1, layout);
  put(pipe, 2, setLayout);
  put(pipe, 3, pushBytes);
  return pipe;
}

static int64_t vulkanDispatch(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t b0, int64_t b1, int64_t b2, int64_t b3) {
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
  return vulkanDispatchBuffers(ctx, pipe, count, push, buffers, used);
}

static int64_t vulkanDispatchBuffers(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t* buffers, int64_t bufferCount) {
  return vulkanLaunch(ctx, pipe, (count + 63) / 64, 1, push, buffers, bufferCount);
}

static int64_t vulkanDispatch2D(int64_t ctx, int64_t pipe, int64_t width, int64_t height, int64_t push, int64_t* buffers, int64_t bufferCount) {
  return vulkanLaunch(ctx, pipe, (width + 7) / 8, (height + 7) / 8, push, buffers, bufferCount);
}

static int64_t vulkanDispatchGroups(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t* buffers, int64_t bufferCount) {
  return vulkanLaunch(ctx, pipe, (count + 255) / 256, 1, push, buffers, bufferCount);
}

static int64_t vulkanLaunch(int64_t ctx, int64_t pipe, int64_t groupsX, int64_t groupsY, int64_t push, int64_t* buffers, int64_t bufferCount) {
  if (vulkanLaunchAsync(ctx, pipe, groupsX, groupsY, push, buffers, bufferCount) == 0) {
    return 0;
  }
  return vulkanFinish(ctx);
}

static int64_t vulkanLaunchAsync(int64_t ctx, int64_t pipe, int64_t groupsX, int64_t groupsY, int64_t push, int64_t* buffers, int64_t bufferCount) {
  int64_t device = word(ctx, 1);
  int64_t dset = vulkanDescriptors(device, pipe, buffers, bufferCount);
  if (dset == 0) {
    return 0;
  }
  int64_t commands = vulkanCommands(ctx, pipe, dset, groupsX, groupsY, push);
  if (commands == 0) {
    return 0;
  }
  return vulkanSubmitOnly(word(ctx, 2), commands);
}

static int64_t vulkanDispatchBuffersAsync(int64_t ctx, int64_t pipe, int64_t count, int64_t push, int64_t* buffers, int64_t bufferCount) {
  return vulkanLaunchAsync(ctx, pipe, (count + 63) / 64, 1, push, buffers, bufferCount);
}

static int64_t vulkanFinish(int64_t ctx) {
  if (vkQueueWaitIdle(word(ctx, 2)) != 0) {
    return 0;
  }
  return 1;
}

static int64_t vulkanDescriptors(int64_t device, int64_t pipe, int64_t* buffers, int64_t bufferCount) {
  int64_t sizes = words(1);
  put(sizes, 0, pair(7, bufferCount));
  int64_t poolInfo = words(5);
  put(poolInfo, 0, 33);
  put(poolInfo, 2, pair(0, 1));
  put(poolInfo, 3, 1);
  put(poolInfo, 4, sizes);
  int64_t pool = slot();
  if (vkCreateDescriptorPool(device, poolInfo, 0, pool) != 0) {
    return 0;
  }
  int64_t layouts = words(1);
  put(layouts, 0, word(pipe, 2));
  int64_t allocInfo = words(5);
  put(allocInfo, 0, 34);
  put(allocInfo, 2, first(pool));
  put(allocInfo, 3, 1);
  put(allocInfo, 4, layouts);
  int64_t dset = slot();
  if (vkAllocateDescriptorSets(device, allocInfo, dset) != 0) {
    return 0;
  }
  bindAll(device, first(dset), buffers, bufferCount);
  return first(dset);
}

static int64_t bindAll(int64_t device, int64_t dset, int64_t* buffers, int64_t bufferCount) {
  for (int64_t binding = 0; binding < bufferCount; binding = binding + 1) {
    writeBinding(device, dset, binding, buffers[binding]);
  }
  return 0;
}

static int64_t writeBinding(int64_t device, int64_t dset, int64_t binding, int64_t buf) {
  int64_t info = words(3);
  put(info, 0, word(buf, 0));
  put(info, 2, 0 - 1);
  int64_t write = words(8);
  put(write, 0, 35);
  put(write, 2, dset);
  put(write, 3, pair(binding, 0));
  put(write, 4, pair(1, 7));
  put(write, 6, info);
  vkUpdateDescriptorSets(device, 1, write, 0, 0);
  return 0;
}

static int64_t vulkanCommands(int64_t ctx, int64_t pipe, int64_t dset, int64_t groupsX, int64_t groupsY, int64_t push) {
  int64_t alloc = words(4);
  put(alloc, 0, 40);
  put(alloc, 2, word(ctx, 3));
  put(alloc, 3, pair(0, 1));
  int64_t out = slot();
  if (vkAllocateCommandBuffers(word(ctx, 1), alloc, out) != 0) {
    return 0;
  }
  int64_t commands = first(out);
  int64_t begin = words(4);
  put(begin, 0, 42);
  put(begin, 2, 1);
  if (vkBeginCommandBuffer(commands, begin) != 0) {
    return 0;
  }
  record(commands, pipe, dset, groupsX, groupsY, push);
  if (vkEndCommandBuffer(commands) != 0) {
    return 0;
  }
  return commands;
}

static int64_t record(int64_t commands, int64_t pipe, int64_t dset, int64_t groupsX, int64_t groupsY, int64_t push) {
  int64_t sets = words(1);
  put(sets, 0, dset);
  vkCmdBindPipeline(commands, 1, word(pipe, 0));
  vkCmdBindDescriptorSets(commands, 1, word(pipe, 1), 0, 1, sets, 0, 0);
  vkCmdPushConstants(commands, word(pipe, 1), 32, 0, word(pipe, 3), push);
  vkCmdDispatch(commands, groupsX, groupsY, 1);
  return 0;
}

static int64_t vulkanSubmitOnly(int64_t queue, int64_t commands) {
  int64_t list = words(1);
  put(list, 0, commands);
  int64_t submit = words(9);
  put(submit, 0, 4);
  put(submit, 5, 1);
  put(submit, 6, list);
  if (vkQueueSubmit(queue, 1, submit, 0) != 0) {
    return 0;
  }
  return 1;
}

static int64_t vulkanSubmit(int64_t queue, int64_t commands) {
  if (vulkanSubmitOnly(queue, commands) == 0) {
    return 0;
  }
  if (vkQueueWaitIdle(queue) != 0) {
    return 0;
  }
  return 1;
}
