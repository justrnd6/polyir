#pragma once

#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "texture.h"

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

static float hitSphere(float ox, float oy, float oz, float dx, float dy, float dz, float cx, float cy, float cz, float r);
static float hitFloor(float oy, float dy, float floorY);
static float trace(int32_t i, float* cam, float* tex);
static int64_t normalize3(float* v);
static int64_t buildCamera(float* cam, int64_t w, int64_t h);
static int64_t poseCamera(float* cam, float ex, float ey, float ez, float yaw, float pitch, float roll, int64_t w, int64_t h);
static int64_t setScene(float* cam, int64_t w, int64_t h);

static float hitSphere(float ox, float oy, float oz, float dx, float dy, float dz, float cx, float cy, float cz, float r) {
  float lx = ox - cx;
  float ly = oy - cy;
  float lz = oz - cz;
  float b = lx * dx + ly * dy + lz * dz;
  float disc = b * b - (lx * lx + ly * ly + lz * lz - r * r);
  if (disc < 0.0f) {
    return (float) (0.0 - 1.0);
  }
  float t = (float) (0.0 - (double) b - (double) sqrtf(disc));
  if (t > 0.001f) {
    return t;
  }
  return (float) (0.0 - 1.0);
}

static float hitFloor(float oy, float dy, float floorY) {
  if (dy >= (float) (0.0 - 0.0001)) {
    return (float) (0.0 - 1.0);
  }
  float t = (floorY - oy) / dy;
  if (t > 0.001f) {
    return t;
  }
  return (float) (0.0 - 1.0);
}

static float trace(int32_t i, float* cam, float* tex) {
  int32_t w = (int32_t) cam[14];
  float u = (float) ((2.0 * (double) ((float) (i % w) + 0.5f) / (double) cam[14] - 1.0) * (double) cam[13] * (double) cam[12]);
  float v = (float) ((1.0 - 2.0 * (double) ((float) (i / w) + 0.5f) / (double) cam[15]) * (double) cam[12]);
  float rx = cam[9] + cam[3] * u + cam[6] * v;
  float ry = cam[10] + cam[4] * u + cam[7] * v;
  float rz = cam[11] + cam[5] * u + cam[8] * v;
  float len = sqrtf(rx * rx + ry * ry + rz * rz);
  float dx = rx / len;
  float dy = ry / len;
  float dz = rz / len;
  float ts = hitSphere(cam[0], cam[1], cam[2], dx, dy, dz, cam[16], cam[17], cam[18], cam[19]);
  float tf = hitFloor(cam[1], dy, cam[23]);
  if (ts > 0.0f && (tf < 0.0f || ts < tf)) {
    float nx = (cam[0] + dx * ts - cam[16]) / cam[19];
    float ny = (cam[1] + dy * ts - cam[17]) / cam[19];
    float nz = (cam[2] + dz * ts - cam[18]) / cam[19];
    float facing = nx * cam[20] + ny * cam[21] + nz * cam[22];
    if (facing < 0.0f) {
      return 0.15f;
    }
    return (float) (0.15 + 0.85 * (double) facing);
  }
  if (tf > 0.0f) {
    float hx = cam[0] + dx * tf;
    float hz = cam[2] + dz * tf;
    float tone = 0.85f;
    if (cam[24] < 0.5f) {
      if (((int32_t) floorf(hx) + (int32_t) floorf(hz)) % 2 != 0) {
        tone = 0.45f;
      }
    } else {
      float u2 = hx / cam[27];
      float v2 = hz / cam[27];
      int32_t base = (int32_t) cam[25];
      if (cam[24] < 1.5f) {
        tone = nearest(tex, 0, base, u2, v2, 1);
      }
      if (cam[24] > 1.5f && cam[24] < 2.5f) {
        tone = bilinear(tex, 0, base, u2, v2, 1);
      }
      if (cam[24] > 2.5f) {
        float worldPerPixel = tf * 2.0f * cam[12] / cam[15] / (float) (0.0 - (double) dy);
        float texels = worldPerPixel * cam[25] / cam[27];
        float level = texels > 1.0f ? logf(texels) * 1.442695f : 0.0f;
        tone = mipSample(tex, base, (int32_t) cam[26], u2, v2, level, 1);
      }
    }
    float blocked = hitSphere(hx, cam[23], hz, cam[20], cam[21], cam[22], cam[16], cam[17], cam[18], cam[19]);
    if (blocked > 0.0f) {
      return tone * 0.35f;
    }
    return tone;
  }
  return (float) (0.12 + 0.1 * (double) v);
}

static int64_t normalize3(float* v) {
  float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] = v[0] / len;
  v[1] = v[1] / len;
  v[2] = v[2] / len;
  return 0;
}

static int64_t buildCamera(float* cam, int64_t w, int64_t h) {
  cam[0] = 0.0f;
  cam[1] = 1.2f;
  cam[2] = 2.5f;
  List* f = listFilled(sizeof(float), 3, (float[]){0.0f});
  LIST_AT(f, 0, float) = (float) (0.0 - (double) cam[0]);
  LIST_AT(f, 1, float) = (float) (0.0 - 0.2 - (double) cam[1]);
  LIST_AT(f, 2, float) = (float) (0.0 - 3.0 - (double) cam[2]);
  float* forward = (int8_t*) (intptr_t) (int64_t) f;
  normalize3(forward);
  List* r = listFilled(sizeof(float), 3, (float[]){0.0f});
  LIST_AT(r, 0, float) = (float) (0.0 - (double) forward[2]);
  LIST_AT(r, 1, float) = 0.0f;
  LIST_AT(r, 2, float) = forward[0];
  float* right = (int8_t*) (intptr_t) (int64_t) r;
  normalize3(right);
  cam[3] = right[0];
  cam[4] = right[1];
  cam[5] = right[2];
  cam[6] = right[1] * forward[2] - right[2] * forward[1];
  cam[7] = right[2] * forward[0] - right[0] * forward[2];
  cam[8] = right[0] * forward[1] - right[1] * forward[0];
  cam[9] = forward[0];
  cam[10] = forward[1];
  cam[11] = forward[2];
  cam[12] = 0.6f;
  cam[13] = (float) w / (float) h;
  cam[14] = (float) w;
  cam[15] = (float) h;
  cam[16] = 0.0f;
  cam[17] = 0.0f;
  cam[18] = (float) (0.0 - 3.0);
  cam[19] = 1.0f;
  List* l = listFilled(sizeof(float), 3, (float[]){0.0f});
  LIST_AT(l, 0, float) = (float) (0.0 - 0.5);
  LIST_AT(l, 1, float) = 0.8f;
  LIST_AT(l, 2, float) = 0.4f;
  float* light = (int8_t*) (intptr_t) (int64_t) l;
  normalize3(light);
  cam[20] = light[0];
  cam[21] = light[1];
  cam[22] = light[2];
  cam[23] = (float) (0.0 - 1.0);
  cam[24] = 0.0f;
  cam[25] = 32.0f;
  cam[26] = 6.0f;
  cam[27] = 4.0f;
  return 0;
}

static int64_t poseCamera(float* cam, float ex, float ey, float ez, float yaw, float pitch, float roll, int64_t w, int64_t h) {
  setScene(cam, w, h);
  float toRadians = 0.0174533f;
  float cy = cosf(yaw * toRadians);
  float sy = sinf(yaw * toRadians);
  float cp = cosf(pitch * toRadians);
  float sp = sinf(pitch * toRadians);
  float cr = cosf(roll * toRadians);
  float sr = sinf(roll * toRadians);
  float fx = cp * sy;
  float fy = sp;
  float fz = (float) (0.0 - (double) (cp * cy));
  float rx = cy;
  float rz = sy;
  float ux = (float) (0.0 - (double) (rz * fy));
  float uy = rz * fx - rx * fz;
  float uz = rx * fy;
  cam[0] = ex;
  cam[1] = ey;
  cam[2] = ez;
  cam[3] = rx * cr + ux * sr;
  cam[4] = uy * sr;
  cam[5] = rz * cr + uz * sr;
  cam[6] = ux * cr - rx * sr;
  cam[7] = uy * cr;
  cam[8] = uz * cr - rz * sr;
  cam[9] = fx;
  cam[10] = fy;
  cam[11] = fz;
  return 0;
}

static int64_t setScene(float* cam, int64_t w, int64_t h) {
  cam[12] = 0.6f;
  cam[13] = (float) w / (float) h;
  cam[14] = (float) w;
  cam[15] = (float) h;
  cam[16] = 0.0f;
  cam[17] = 0.0f;
  cam[18] = (float) (0.0 - 3.0);
  cam[19] = 1.0f;
  List* l = listFilled(sizeof(float), 3, (float[]){0.0f});
  LIST_AT(l, 0, float) = (float) (0.0 - 0.5);
  LIST_AT(l, 1, float) = 0.8f;
  LIST_AT(l, 2, float) = 0.4f;
  float* light = (int8_t*) (intptr_t) (int64_t) l;
  normalize3(light);
  cam[20] = light[0];
  cam[21] = light[1];
  cam[22] = light[2];
  cam[23] = (float) (0.0 - 1.0);
  cam[24] = 0.0f;
  cam[25] = 32.0f;
  cam[26] = 6.0f;
  cam[27] = 4.0f;
  return 0;
}
