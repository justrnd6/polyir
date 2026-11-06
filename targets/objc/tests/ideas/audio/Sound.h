#pragma once

#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../../core/stdlib/Math.h"
#import "Biquad.h"
#import "Resample.h"
#import "Wave.h"

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

typedef struct Sound Sound;
typedef struct WavInfo WavInfo;

struct Sound {
  float* samples;
  int64_t count;
  int64_t rate;
  int64_t channels;
};

struct WavInfo {
  int64_t format;
  int64_t channels;
  int64_t rate;
  int64_t bits;
  int64_t dataStart;
  int64_t dataBytes;
};

static Sound* Sound_new(float* samples, int64_t count, int64_t rate, int64_t channels);
static bool Sound_equal(Sound* a, Sound* b);
static uint64_t Sound_hash(Sound* a);
static float Sound_seconds(Sound* self);
static int64_t Sound_scale(Sound* self, float gain);
static int64_t Sound_add(Sound* self, Sound* other, float start, float gain);
static int64_t Sound_tone(Sound* self, int64_t shape, float frequency, float amplitude, float start, float length);
static int64_t Sound_noise(Sound* self, float amplitude, int64_t seed);
static float Sound_peak(Sound* self);
static int64_t Sound_normalize(Sound* self, float level);
static float Sound_strength(Sound* self, float hz, float start, float length);
static float* Sound_takeChannel(Sound* self, int64_t c);
static int64_t Sound_putChannel(Sound* self, int64_t c, float* one);
static Sound* Sound_channel(Sound* self, int64_t c);
static Sound* Sound_toMono(Sound* self);
static Sound* Sound_pan(Sound* self, float position);
static Sound* Sound_toStereo(Sound* self);
static int64_t Sound_lowPass(Sound* self, float cutoff);
static int64_t Sound_highPass(Sound* self, float cutoff);
static int64_t Sound_apply(Sound* self, Biquad* filter);
static int64_t Sound_lowPassSteep(Sound* self, float cutoff, float q);
static int64_t Sound_highPassSteep(Sound* self, float cutoff, float q);
static int64_t Sound_bandPass(Sound* self, float center, float q);
static int64_t Sound_notch(Sound* self, float center, float q);
static int64_t Sound_bell(Sound* self, float center, float q, float gainDb);
static int64_t Sound_lowShelf(Sound* self, float corner, float gainDb);
static int64_t Sound_highShelf(Sound* self, float corner, float gainDb);
static int64_t Sound_echo(Sound* self, float delay, float gain);
static int64_t Sound_reverb(Sound* self, float wet);
static Sound* Sound_convolve(Sound* self, Sound* response);
static Sound* Sound_resample(Sound* self, int64_t newRate);
static int64_t Sound_saveWav(Sound* self, const char* path);

static Sound* soundNew(float seconds, int64_t rate);
static Sound* soundNewChannels(float seconds, int64_t rate, int64_t channels);
static Sound* soundNewFrames(int64_t frames, int64_t rate, int64_t channels);
static Sound* soundStereo(Sound* left, Sound* right);
static float strengthOfArray(float* data, int64_t start, int64_t count, int64_t rate, float hz);
static int64_t lowPassArray(float* data, int64_t count, int64_t rate, float cutoff);
static int64_t highPassArray(float* data, int64_t count, int64_t rate, float cutoff);
static int64_t reverbArray(float* data, int64_t count, int64_t rate, float wet);
static float* convolveArrays(float* x, int64_t nx, float* h, int64_t nh);
static float combDelay(int64_t c);
static float combFeedback(int64_t c);
static float oscillator(int64_t shape, float phase);
static Sound* soundLoadWav(const char* path);

static WavInfo* WavInfo_new(int64_t format, int64_t channels, int64_t rate, int64_t bits, int64_t dataStart, int64_t dataBytes);
static bool WavInfo_equal(WavInfo* a, WavInfo* b);
static uint64_t WavInfo_hash(WavInfo* a);

static int64_t soundReadFile(const char* path, int64_t* out);
static WavInfo* wavParse(int8_t* bytes, int64_t size);
static int64_t wavDataBytes(int64_t claimed, int64_t available);
static int64_t soundTag(int64_t a, int64_t b, int64_t c, int64_t d);
static WavInfo* wavFormat(int8_t* bytes, int64_t at, int64_t length);
static Sound* wavDecode(int8_t* bytes, WavInfo* info);
static float wavSample(int8_t* bytes, int64_t at, int64_t format, int64_t bits, int8_t* scratch);
static float wavFloat(int8_t* bytes, int64_t at, int64_t bits, int8_t* scratch);
static int64_t soundByte(int8_t* bytes, int64_t at);
static int64_t soundSigned(int8_t* bytes, int64_t at, int64_t width);
static int64_t soundWord(int8_t* bytes, int64_t at);
static int64_t soundLetters(int8_t* bytes, int64_t at, int64_t a, int64_t b, int64_t c, int64_t d);
static int64_t soundNumber32(int8_t* bytes, int64_t at, int64_t value);
static int64_t soundNumber16(int8_t* bytes, int64_t at, int64_t value);

static Sound* Sound_new(float* samples, int64_t count, int64_t rate, int64_t channels) {
  Sound* made = gcAlloc(sizeof(Sound));
  made->samples = samples;
  made->count = count;
  made->rate = rate;
  made->channels = channels;
  return made;
}

static bool Sound_equal(Sound* a, Sound* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->samples == b->samples;
  same = same && a->count == b->count;
  same = same && a->rate == b->rate;
  same = same && a->channels == b->channels;
  return same;
}

static uint64_t Sound_hash(Sound* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->samples, sizeof a->samples);
  hash = hash * 31 + (uint64_t) a->count;
  hash = hash * 31 + (uint64_t) a->rate;
  hash = hash * 31 + (uint64_t) a->channels;
  return hash;
}

static float Sound_seconds(Sound* self) {
  return (float) (self->count) / (float) (self->rate);
}

static int64_t Sound_scale(Sound* self, float gain) {
  float* data = self->samples;
  for (int64_t i = 0; i < self->count * self->channels; i = i + 1) {
    data[i] = data[i] * gain;
  }
  return 0;
}

static int64_t Sound_add(Sound* self, Sound* other, float start, float gain) {
  float* data = self->samples;
  float* source = other->samples;
  int64_t first = (int64_t) (start * (float) (self->rate));
  for (int64_t i = 0; i < other->count; i = i + 1) {
    int64_t at = first + i;
    if (at >= 0 && at < self->count) {
      for (int64_t c = 0; c < self->channels; c = c + 1) {
        data[at * self->channels + c] = data[at * self->channels + c] + gain * source[i * other->channels + c % other->channels];
      }
    }
  }
  return 0;
}

static int64_t Sound_tone(Sound* self, int64_t shape, float frequency, float amplitude, float start, float length) {
  float* data = self->samples;
  int64_t first = (int64_t) (start * (float) (self->rate));
  int64_t n = (int64_t) (length * (float) (self->rate));
  for (int64_t i = 0; i < n; i = i + 1) {
    int64_t at = first + i;
    if (at >= 0 && at < self->count) {
      float t = (float) i / (float) (self->rate);
      float value = amplitude * fade(t, 0.02f, 0.05f, length) * oscillator(shape, frequency * t);
      for (int64_t c = 0; c < self->channels; c = c + 1) {
        data[at * self->channels + c] = data[at * self->channels + c] + value;
      }
    }
  }
  return 0;
}

static int64_t Sound_noise(Sound* self, float amplitude, int64_t seed) {
  float* data = self->samples;
  for (int64_t i = 0; i < self->count * self->channels; i = i + 1) {
    data[i] = data[i] + amplitude * noise((int32_t) (seed + i));
  }
  return 0;
}

static float Sound_peak(Sound* self) {
  float* data = self->samples;
  float loudest = 0.0f;
  for (int64_t i = 0; i < self->count * self->channels; i = i + 1) {
    float level = data[i];
    if (level < 0.0f) {
      level = (float) (0.0 - (double) level);
    }
    if (level > loudest) {
      loudest = level;
    }
  }
  return loudest;
}

static int64_t Sound_normalize(Sound* self, float level) {
  float loudest = Sound_peak(self);
  if (loudest > 0.0f) {
    Sound_scale(self, level / loudest);
  }
  return 0;
}

static float Sound_strength(Sound* self, float hz, float start, float length) {
  int64_t first = (int64_t) (start * (float) (self->rate));
  int64_t n = (int64_t) (length * (float) (self->rate));
  return strengthOfArray(Sound_takeChannel(self, 0), first, n, self->rate, hz);
}

static float* Sound_takeChannel(Sound* self, int64_t c) {
  float* one = (int8_t*) (intptr_t) calloc(self->count + 1, 4);
  float* data = self->samples;
  for (int64_t i = 0; i < self->count; i = i + 1) {
    one[i] = data[i * self->channels + c];
  }
  return one;
}

static int64_t Sound_putChannel(Sound* self, int64_t c, float* one) {
  float* data = self->samples;
  for (int64_t i = 0; i < self->count; i = i + 1) {
    data[i * self->channels + c] = one[i];
  }
  return 0;
}

static Sound* Sound_channel(Sound* self, int64_t c) {
  return Sound_new(Sound_takeChannel(self, c), self->count, self->rate, 1);
}

static Sound* Sound_toMono(Sound* self) {
  Sound* result = soundNewFrames(self->count, self->rate, 1);
  float* out = result->samples;
  float* data = self->samples;
  for (int64_t i = 0; i < self->count; i = i + 1) {
    for (int64_t c = 0; c < self->channels; c = c + 1) {
      out[i] = out[i] + data[i * self->channels + c] / (float) (self->channels);
    }
  }
  return result;
}

static Sound* Sound_pan(Sound* self, float position) {
  Sound* mono = Sound_toMono(self);
  Sound* result = soundNewFrames(self->count, self->rate, 2);
  float* out = result->samples;
  float angle = (position + 1.0f) * 0.7853982f;
  for (int64_t i = 0; i < self->count; i = i + 1) {
    out[i * 2] = cosf(angle) * (mono->samples)[i];
    out[i * 2 + 1] = sinf(angle) * (mono->samples)[i];
  }
  return result;
}

static Sound* Sound_toStereo(Sound* self) {
  Sound* result = soundNewFrames(self->count, self->rate, 2);
  float* out = result->samples;
  float* data = self->samples;
  for (int64_t i = 0; i < self->count; i = i + 1) {
    out[i * 2] = data[i * self->channels];
    out[i * 2 + 1] = data[i * self->channels + self->channels - 1];
  }
  return result;
}

static int64_t Sound_lowPass(Sound* self, float cutoff) {
  for (int64_t c = 0; c < self->channels; c = c + 1) {
    float* one = Sound_takeChannel(self, c);
    lowPassArray(one, self->count, self->rate, cutoff);
    Sound_putChannel(self, c, one);
  }
  return 0;
}

static int64_t Sound_highPass(Sound* self, float cutoff) {
  for (int64_t c = 0; c < self->channels; c = c + 1) {
    float* one = Sound_takeChannel(self, c);
    highPassArray(one, self->count, self->rate, cutoff);
    Sound_putChannel(self, c, one);
  }
  return 0;
}

static int64_t Sound_apply(Sound* self, Biquad* filter) {
  for (int64_t c = 0; c < self->channels; c = c + 1) {
    float* one = Sound_takeChannel(self, c);
    Biquad_run(filter, one, self->count);
    Sound_putChannel(self, c, one);
  }
  return 0;
}

static int64_t Sound_lowPassSteep(Sound* self, float cutoff, float q) {
  return Sound_apply(self, biquadLowPass(self->rate, cutoff, q));
}

static int64_t Sound_highPassSteep(Sound* self, float cutoff, float q) {
  return Sound_apply(self, biquadHighPass(self->rate, cutoff, q));
}

static int64_t Sound_bandPass(Sound* self, float center, float q) {
  return Sound_apply(self, biquadBandPass(self->rate, center, q));
}

static int64_t Sound_notch(Sound* self, float center, float q) {
  return Sound_apply(self, biquadNotch(self->rate, center, q));
}

static int64_t Sound_bell(Sound* self, float center, float q, float gainDb) {
  return Sound_apply(self, biquadBell(self->rate, center, q, gainDb));
}

static int64_t Sound_lowShelf(Sound* self, float corner, float gainDb) {
  return Sound_apply(self, biquadLowShelf(self->rate, corner, gainDb));
}

static int64_t Sound_highShelf(Sound* self, float corner, float gainDb) {
  return Sound_apply(self, biquadHighShelf(self->rate, corner, gainDb));
}

static int64_t Sound_echo(Sound* self, float delay, float gain) {
  float* data = self->samples;
  int64_t gap = (int64_t) (delay * (float) (self->rate)) * self->channels;
  for (int64_t i = gap; i < self->count * self->channels; i = i + 1) {
    data[i] = data[i] + gain * data[i - gap];
  }
  return 0;
}

static int64_t Sound_reverb(Sound* self, float wet) {
  for (int64_t c = 0; c < self->channels; c = c + 1) {
    float* one = Sound_takeChannel(self, c);
    reverbArray(one, self->count, self->rate, wet);
    Sound_putChannel(self, c, one);
  }
  return 0;
}

static Sound* Sound_convolve(Sound* self, Sound* response) {
  Sound* result = soundNewFrames(self->count + response->count - 1, self->rate, self->channels);
  for (int64_t c = 0; c < self->channels; c = c + 1) {
    float* one = Sound_takeChannel(self, c);
    float* weights = Sound_takeChannel(response, c % response->channels);
    Sound_putChannel(result, c, convolveArrays(one, self->count, weights, response->count));
  }
  return result;
}

static Sound* Sound_resample(Sound* self, int64_t newRate) {
  Sound* result = soundNewFrames(resampledCount(self->count, self->rate, newRate), newRate, self->channels);
  for (int64_t c = 0; c < self->channels; c = c + 1) {
    float* one = Sound_takeChannel(self, c);
    Sound_putChannel(result, c, resampleArray(one, self->count, self->rate, newRate));
  }
  return result;
}

static int64_t Sound_saveWav(Sound* self, const char* path) {
  int64_t values = self->count * self->channels;
  int64_t total = 44 + values * 2;
  int8_t* bytes = (int8_t*) (intptr_t) calloc(total, 1);
  int64_t at = soundLetters(bytes, 0, 82, 73, 70, 70);
  at = soundNumber32(bytes, at, total - 8);
  at = soundLetters(bytes, at, 87, 65, 86, 69);
  at = soundLetters(bytes, at, 102, 109, 116, 32);
  at = soundNumber32(bytes, at, 16);
  at = soundNumber16(bytes, at, 1);
  at = soundNumber16(bytes, at, self->channels);
  at = soundNumber32(bytes, at, self->rate);
  at = soundNumber32(bytes, at, self->rate * 2 * self->channels);
  at = soundNumber16(bytes, at, 2 * self->channels);
  at = soundNumber16(bytes, at, 16);
  at = soundLetters(bytes, at, 100, 97, 116, 97);
  at = soundNumber32(bytes, at, values * 2);
  float* data = self->samples;
  for (int64_t k = 0; k < values; k = k + 1) {
    float level = clamp(data[k], (float) (0.0 - 1.0), 1.0f);
    int64_t whole = (int64_t) (level * 32767.0f);
    if (whole < 0) {
      whole = whole + 65536;
    }
    soundNumber16(bytes, 44 + 2 * k, whole);
  }
  int64_t file = fopen(path, "wb");
  if (file == 0) {
    return 0;
  }
  fwrite((int64_t) (intptr_t) bytes, 1, total, file);
  fclose(file);
  return 1;
}

static Sound* soundNew(float seconds, int64_t rate) {
  return soundNewFrames((int64_t) (seconds * (float) rate), rate, 1);
}

static Sound* soundNewChannels(float seconds, int64_t rate, int64_t channels) {
  return soundNewFrames((int64_t) (seconds * (float) rate), rate, channels);
}

static Sound* soundNewFrames(int64_t frames, int64_t rate, int64_t channels) {
  return Sound_new((int8_t*) (intptr_t) calloc(frames * channels + 1, 4), frames, rate, channels);
}

static Sound* soundStereo(Sound* left, Sound* right) {
  int64_t frames = left->count;
  if (right->count > frames) {
    frames = right->count;
  }
  Sound* result = soundNewFrames(frames, left->rate, 2);
  float* out = result->samples;
  for (int64_t i = 0; i < left->count; i = i + 1) {
    out[i * 2] = (left->samples)[i * left->channels];
  }
  for (int64_t i = 0; i < right->count; i = i + 1) {
    out[i * 2 + 1] = (right->samples)[i * right->channels];
  }
  return result;
}

static float strengthOfArray(float* data, int64_t start, int64_t count, int64_t rate, float hz) {
  double re = 0.0;
  double im = 0.0;
  for (int64_t k = 0; k < count; k = k + 1) {
    double angle = 6.283185307179586 * (double) hz * (double) (start + k) / (double) rate;
    re = re + (double) data[start + k] * cos(angle);
    im = im + (double) data[start + k] * sin(angle);
  }
  return (float) (2.0 * sqrt(re * re + im * im) / (double) count);
}

static int64_t lowPassArray(float* data, int64_t count, int64_t rate, float cutoff) {
  float dt = (float) (1.0 / (double) (float) rate);
  float rc = (float) (1.0 / (6.2831853 * (double) cutoff));
  float alpha = dt / (rc + dt);
  float previous = 0.0f;
  for (int64_t i = 0; i < count; i = i + 1) {
    previous = previous + alpha * (data[i] - previous);
    data[i] = previous;
  }
  return 0;
}

static int64_t highPassArray(float* data, int64_t count, int64_t rate, float cutoff) {
  float dt = (float) (1.0 / (double) (float) rate);
  float rc = (float) (1.0 / (6.2831853 * (double) cutoff));
  float alpha = rc / (rc + dt);
  float lastIn = 0.0f;
  float lastOut = 0.0f;
  for (int64_t i = 0; i < count; i = i + 1) {
    float x = data[i];
    lastOut = alpha * (lastOut + x - lastIn);
    lastIn = x;
    data[i] = lastOut;
  }
  return 0;
}

static int64_t reverbArray(float* data, int64_t count, int64_t rate, float wet) {
  float* sum = (int8_t*) (intptr_t) calloc(count + 1, 4);
  for (int64_t c = 0; c < 4; c = c + 1) {
    int64_t gap = (int64_t) (combDelay(c) * (float) rate);
    float feedback = combFeedback(c);
    float* line = (int8_t*) (intptr_t) calloc(count + 1, 4);
    for (int64_t i = 0; i < count; i = i + 1) {
      float v = data[i];
      if (i >= gap) {
        v = v + feedback * line[i - gap];
      }
      line[i] = v;
      sum[i] = sum[i] + (float) (0.25 * (1.0 - (double) feedback) * (double) v);
    }
  }
  for (int64_t i = 0; i < count; i = i + 1) {
    data[i] = (float) ((1.0 - (double) wet) * (double) data[i] + (double) (wet * sum[i]));
  }
  return 0;
}

static float* convolveArrays(float* x, int64_t nx, float* h, int64_t nh) {
  float* out = (int8_t*) (intptr_t) calloc(nx + nh, 4);
  for (int64_t i = 0; i < nx; i = i + 1) {
    for (int64_t j = 0; j < nh; j = j + 1) {
      out[i + j] = out[i + j] + x[i] * h[j];
    }
  }
  return out;
}

static float combDelay(int64_t c) {
  if (c == 0) {
    return 0.0297f;
  }
  if (c == 1) {
    return 0.0371f;
  }
  if (c == 2) {
    return 0.0411f;
  }
  return 0.0437f;
}

static float combFeedback(int64_t c) {
  if (c == 0) {
    return 0.805f;
  }
  if (c == 1) {
    return 0.827f;
  }
  if (c == 2) {
    return 0.783f;
  }
  return 0.764f;
}

static float oscillator(int64_t shape, float phase) {
  if (shape == 1) {
    return square(phase);
  }
  if (shape == 2) {
    return sawtooth(phase);
  }
  if (shape == 3) {
    return triangle(phase);
  }
  return sine(phase);
}

static Sound* soundLoadWav(const char* path) {
  int64_t* slot = (int8_t*) (intptr_t) calloc(1, 8);
  int64_t size = soundReadFile(path, slot);
  int8_t* bytes = (int8_t*) (intptr_t) slot[0];
  WavInfo* info = wavParse(bytes, size);
  if (info->format == 0) {
    return soundNewFrames(0, 0, 1);
  }
  return wavDecode(bytes, info);
}

static WavInfo* WavInfo_new(int64_t format, int64_t channels, int64_t rate, int64_t bits, int64_t dataStart, int64_t dataBytes) {
  WavInfo* made = gcAlloc(sizeof(WavInfo));
  made->format = format;
  made->channels = channels;
  made->rate = rate;
  made->bits = bits;
  made->dataStart = dataStart;
  made->dataBytes = dataBytes;
  return made;
}

static bool WavInfo_equal(WavInfo* a, WavInfo* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->format == b->format;
  same = same && a->channels == b->channels;
  same = same && a->rate == b->rate;
  same = same && a->bits == b->bits;
  same = same && a->dataStart == b->dataStart;
  same = same && a->dataBytes == b->dataBytes;
  return same;
}

static uint64_t WavInfo_hash(WavInfo* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->format;
  hash = hash * 31 + (uint64_t) a->channels;
  hash = hash * 31 + (uint64_t) a->rate;
  hash = hash * 31 + (uint64_t) a->bits;
  hash = hash * 31 + (uint64_t) a->dataStart;
  hash = hash * 31 + (uint64_t) a->dataBytes;
  return hash;
}

static int64_t soundReadFile(const char* path, int64_t* out) {
  int64_t file = fopen(path, "rb");
  if (file == 0) {
    return 0;
  }
  fseek(file, 0, 2);
  int64_t size = ftell(file);
  fseek(file, 0, 0);
  int64_t memory = calloc(size + 8, 1);
  int64_t got = fread(memory, 1, size, file);
  fclose(file);
  out[0] = memory;
  return got;
}

static WavInfo* wavParse(int8_t* bytes, int64_t size) {
  WavInfo* none = WavInfo_new(0, 0, 0, 0, 0, 0);
  if (size < 12 || soundWord(bytes, 0) != soundTag(82, 73, 70, 70) || soundWord(bytes, 8) != soundTag(87, 65, 86, 69)) {
    return none;
  }
  WavInfo* found = none;
  int64_t at = 12;
  for (int64_t step = 0; step < 64; step = step + 1) {
    if (at + 8 <= size) {
      int64_t name = soundWord(bytes, at);
      int64_t length = soundWord(bytes, at + 4);
      if (name == soundTag(102, 109, 116, 32) && at + 24 <= size) {
        found = wavFormat(bytes, at + 8, length);
      }
      if (name == soundTag(100, 97, 116, 97) && found->format != 0) {
        return WavInfo_new(found->format, found->channels, found->rate, found->bits, at + 8, wavDataBytes(length, size - at - 8));
      }
      at = at + 8 + length + length % 2;
    }
  }
  return none;
}

static int64_t wavDataBytes(int64_t claimed, int64_t available) {
  if (claimed > available) {
    return available;
  }
  return claimed;
}

static int64_t soundTag(int64_t a, int64_t b, int64_t c, int64_t d) {
  return a + 256 * b + 65536 * c + 16777216 * d;
}

static WavInfo* wavFormat(int8_t* bytes, int64_t at, int64_t length) {
  int64_t tag = soundSigned(bytes, at, 2);
  if (tag < 0) {
    tag = tag + 65536;
  }
  if (tag == 65534 && length >= 40) {
    tag = soundByte(bytes, at + 24) + 256 * soundByte(bytes, at + 25);
  }
  int64_t bits = soundByte(bytes, at + 14) + 256 * soundByte(bytes, at + 15);
  int64_t format = tag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32) ? 1 : 0;
  if (tag == 3 && (bits == 32 || bits == 64)) {
    format = 3;
  }
  return WavInfo_new(format, soundByte(bytes, at + 2) + 256 * soundByte(bytes, at + 3), soundWord(bytes, at + 4), bits, 0, 0);
}

static Sound* wavDecode(int8_t* bytes, WavInfo* info) {
  int64_t width = info->bits / 8;
  int64_t frames = info->dataBytes / (width * info->channels);
  Sound* result = soundNewFrames(frames, info->rate, info->channels);
  float* out = result->samples;
  int8_t* scratch = (int8_t*) (intptr_t) calloc(8, 1);
  for (int64_t k = 0; k < frames * info->channels; k = k + 1) {
    out[k] = wavSample(bytes, info->dataStart + k * width, info->format, info->bits, scratch);
  }
  return result;
}

static float wavSample(int8_t* bytes, int64_t at, int64_t format, int64_t bits, int8_t* scratch) {
  if (format == 3) {
    return wavFloat(bytes, at, bits, scratch);
  }
  if (bits == 8) {
    return (float) (soundByte(bytes, at) - 128) / 128.0f;
  }
  if (bits == 16) {
    return (float) soundSigned(bytes, at, 2) / 32768.0f;
  }
  if (bits == 24) {
    return (float) soundSigned(bytes, at, 3) / 8388608.0f;
  }
  return (float) soundSigned(bytes, at, 4) / 2147483648.0f;
}

static float wavFloat(int8_t* bytes, int64_t at, int64_t bits, int8_t* scratch) {
  for (int64_t k = 0; k < bits / 8; k = k + 1) {
    scratch[k] = bytes[at + k];
  }
  if (bits == 64) {
    double* wide = (int8_t*) (intptr_t) (int64_t) (intptr_t) scratch;
    return (float) wide[0];
  }
  float* narrow = (int8_t*) (intptr_t) (int64_t) (intptr_t) scratch;
  return narrow[0];
}

static int64_t soundByte(int8_t* bytes, int64_t at) {
  int64_t value = (int64_t) bytes[at];
  if (value < 0) {
    value = value + 256;
  }
  return value;
}

static int64_t soundSigned(int8_t* bytes, int64_t at, int64_t width) {
  int64_t value = 0;
  int64_t weight = 1;
  for (int64_t k = 0; k < width; k = k + 1) {
    value = value + soundByte(bytes, at + k) * weight;
    weight = weight * 256;
  }
  if (value >= weight / 2) {
    value = value - weight;
  }
  return value;
}

static int64_t soundWord(int8_t* bytes, int64_t at) {
  return soundByte(bytes, at) + 256 * soundByte(bytes, at + 1) + 65536 * soundByte(bytes, at + 2) + 16777216 * soundByte(bytes, at + 3);
}

static int64_t soundLetters(int8_t* bytes, int64_t at, int64_t a, int64_t b, int64_t c, int64_t d) {
  bytes[at] = (int8_t) a;
  bytes[at + 1] = (int8_t) b;
  bytes[at + 2] = (int8_t) c;
  bytes[at + 3] = (int8_t) d;
  return at + 4;
}

static int64_t soundNumber32(int8_t* bytes, int64_t at, int64_t value) {
  bytes[at] = (int8_t) (value % 256);
  bytes[at + 1] = (int8_t) (value / 256 % 256);
  bytes[at + 2] = (int8_t) (value / 65536 % 256);
  bytes[at + 3] = (int8_t) (value / 16777216 % 256);
  return at + 4;
}

static int64_t soundNumber16(int8_t* bytes, int64_t at, int64_t value) {
  bytes[at] = (int8_t) (value % 256);
  bytes[at + 1] = (int8_t) (value / 256 % 256);
  return at + 2;
}
