#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Audio.h"
#import "../../audio/Biquad.h"
#import "../../audio/Sound.h"
#import "../../audio/Wave.h"
#import "../../gpu/examples/shared/picture.h"
#import "../../gpu/examples/shared/plot.h"
#import "../../gpu/examples/shared/print.h"

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

double log10(double x);

static Sound* stereoMix(void);
static int64_t checkFile(Sound* mix);
static int64_t checkPan(Sound* mix);
static int64_t checkResample(Sound* mix);
static int64_t checkAliasing(void);
static int64_t applyEqualizer(Sound* s);
static float equalizerGain(float hz);
static float measuredEqualizer(float hz);
static int64_t checkEqualizer(void);
static float frequencyAt(int64_t column);
static int64_t markDecades(float* pic, int64_t w, int64_t h);
static float decibels(float gain);
static int64_t drawEqualizer(void);
static int64_t checkHum(void);
static int64_t drawHum(void);
static float telephoneGain(float hz);
static int64_t checkTelephone(void);
static int64_t main_(void);

static Sound* stereoMix(void) {
  Sound* mix = soundNewChannels(4.0f, 44100, 2);
  Sound* melody = soundNew(4.0f, 44100);
  Sound_tone(melody, 0, 440.0f, 0.4f, 0.0f, 4.0f);
  Sound* bass = soundNew(4.0f, 44100);
  Sound_tone(bass, 2, 100.0f, 0.1f, 0.0f, 4.0f);
  Sound* chord = soundNew(4.0f, 44100);
  Sound_tone(chord, 0, 660.0f, 0.3f, 0.0f, 4.0f);
  Sound_add(mix, Sound_pan(melody, (float) (0.0 - 0.6)), 0.0f, 1.0f);
  Sound_add(mix, Sound_pan(bass, 0.0f), 0.0f, 1.0f);
  Sound_add(mix, Sound_pan(chord, 0.6f), 0.0f, 1.0f);
  return mix;
}

static int64_t checkFile(Sound* mix) {
  if (Sound_saveWav(mix, "studio-mix.wav") == 0) {
    return 11;
  }
  Sound* back = soundLoadWav("studio-mix.wav");
  if (back->count != mix->count || back->channels != 2 || back->rate != 44100) {
    return 11;
  }
  float worst = 0.0f;
  for (int64_t k = 0; k < mix->count * 2; k = k + 1) {
    float gap = (back->samples)[k] - (mix->samples)[k];
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  say("studio-mix.wav: saved and read back, largest difference ");
  sayDecimal(worst * 1000.0f);
  say(" thousandths");
  newline();
  if (worst > 0.001f) {
    return 11;
  }
  return 0;
}

static int64_t checkPan(Sound* mix) {
  float left = Sound_strength(Sound_channel(mix, 0), 440.0f, 1.0f, 2.0f);
  float right = Sound_strength(Sound_channel(mix, 1), 440.0f, 1.0f, 2.0f);
  say("the 440 Hz melody: left ");
  sayDecimal(left);
  say(", right ");
  sayDecimal(right);
  newline();
  if (left < 0.37f || left > 0.39f || right < 0.115f || right > 0.133f) {
    return 12;
  }
  float chordRight = Sound_strength(Sound_channel(mix, 1), 660.0f, 1.0f, 2.0f);
  if (chordRight < 0.27f || chordRight > 0.3f) {
    return 12;
  }
  return 0;
}

static int64_t checkResample(Sound* mix) {
  Sound* up = Sound_resample(mix, 48000);
  Sound* down = Sound_resample(mix, 22050);
  float before = Sound_strength(Sound_channel(mix, 0), 440.0f, 1.0f, 2.0f);
  float atUp = Sound_strength(Sound_channel(up, 0), 440.0f, 1.0f, 2.0f);
  float atDown = Sound_strength(Sound_channel(down, 0), 440.0f, 1.0f, 2.0f);
  say("resampled to 48,000 Hz: ");
  sayNumber(up->count);
  say(" frames, the 440 Hz melody still ");
  sayDecimal(atUp);
  say(" (was ");
  sayDecimal(before);
  say("); to 22,050 Hz: ");
  sayNumber(down->count);
  say(" frames, melody ");
  sayDecimal(atDown);
  newline();
  if (up->count != 192000 || down->count != 88200 || up->channels != 2) {
    return 13;
  }
  if (atUp < before - 0.005f || atUp > before + 0.005f || atDown < before - 0.005f || atDown > before + 0.005f) {
    return 13;
  }
  return 0;
}

static int64_t checkAliasing(void) {
  Sound* tone = soundNew(1.0f, 8000);
  for (int64_t i = 0; i < tone->count; i = i + 1) {
    (tone->samples)[i] = sine((float) (3000.0 * (double) (float) i / 8000.0));
  }
  Sound* naive = soundNew(1.0f, 4000);
  for (int64_t i = 0; i < naive->count; i = i + 1) {
    (naive->samples)[i] = (tone->samples)[i * 2];
  }
  Sound* careful = Sound_resample(tone, 4000);
  float alias = Sound_strength(naive, 1000.0f, 0.25f, 0.5f);
  float kept = Sound_strength(careful, 1000.0f, 0.25f, 0.5f);
  say("3,000 Hz tone at 8,000 Hz down to 4,000 Hz: dropping every second sample gives a false 1,000 Hz tone of ");
  sayDecimal(alias);
  say("; resample gives ");
  sayDecimal(kept);
  newline();
  if (alias < 0.99f || kept > 0.01f) {
    return 13;
  }
  return 0;
}

static int64_t applyEqualizer(Sound* s) {
  Sound_lowShelf(s, 200.0f, 6.0f);
  Sound_bell(s, 1000.0f, 1.0f, (float) (0.0 - 6.0));
  Sound_highShelf(s, 6000.0f, 3.0f);
  return 0;
}

static float equalizerGain(float hz) {
  Biquad* low = biquadLowShelf(44100, 200.0f, 6.0f);
  Biquad* middle = biquadBell(44100, 1000.0f, 1.0f, (float) (0.0 - 6.0));
  Biquad* high = biquadHighShelf(44100, 6000.0f, 3.0f);
  return Biquad_gain(low, hz, 44100) * Biquad_gain(middle, hz, 44100) * Biquad_gain(high, hz, 44100);
}

static float measuredEqualizer(float hz) {
  Sound* tone = soundNew(1.0f, 44100);
  for (int64_t i = 0; i < tone->count; i = i + 1) {
    (tone->samples)[i] = sine(hz * (float) i / 44100.0f);
  }
  applyEqualizer(tone);
  return Sound_strength(tone, hz, 0.5f, 0.5f);
}

static int64_t checkEqualizer(void) {
  float* tones = (int8_t*) (intptr_t) calloc(8, 4);
  tones[0] = 50.0f;
  tones[1] = 100.0f;
  tones[2] = 200.0f;
  tones[3] = 500.0f;
  tones[4] = 1000.0f;
  tones[5] = 2000.0f;
  tones[6] = 6000.0f;
  tones[7] = 12000.0f;
  say("equalizer, tone by tone: predicted gain / measured gain");
  newline();
  float worst = 0.0f;
  for (int64_t k = 0; k < 8; k = k + 1) {
    float predicted = equalizerGain(tones[k]);
    float measured = measuredEqualizer(tones[k]);
    sayRight((int64_t) tones[k], 7);
    say(" Hz   ");
    sayDecimal(predicted);
    say("   ");
    sayDecimal(measured);
    newline();
    float gap = (measured - predicted) / predicted;
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  if (worst > 0.01f) {
    return 14;
  }
  return drawEqualizer();
}

static float frequencyAt(int64_t column) {
  return (float) pow(10.0, 1.30103 + 3.0 * (double) column / 319.0);
}

static int64_t markDecades(float* pic, int64_t w, int64_t h) {
  for (int64_t d = 0; d < 3; d = d + 1) {
    int64_t column = (int64_t) ((0.69897 + (double) (float) d) / 3.0 * (double) (float) (w - 1));
    for (int64_t y = 0; y < h; y = y + 1) {
      if (pic[y * w + column] > 0.9f) {
        pic[y * w + column] = 0.8f;
      }
    }
  }
  return 0;
}

static float decibels(float gain) {
  return (float) (20.0 * log10((double) gain + 0.0000001));
}

static int64_t drawEqualizer(void) {
  float* curve = (int8_t*) (intptr_t) calloc(320, 4);
  for (int64_t x = 0; x < 320; x = x + 1) {
    curve[x] = decibels(equalizerGain(frequencyAt(x)));
  }
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawCurve(pic, 320, 96, curve, 320, (float) (0.0 - 12.0), 12.0f);
  markDecades(pic, 320, 96);
  savePicture("studio-eq.pgm", pic, 320, 96);
  return 0;
}

static int64_t checkHum(void) {
  Sound* s = soundNew(2.0f, 44100);
  Sound_tone(s, 0, 440.0f, 0.5f, 0.0f, 2.0f);
  for (int64_t i = 0; i < s->count; i = i + 1) {
    (s->samples)[i] = (s->samples)[i] + (float) (0.3 * (double) sine((float) (50.0 * (double) (float) i / 44100.0)));
  }
  float humBefore = Sound_strength(s, 50.0f, 1.0f, 1.0f);
  Sound_notch(s, 50.0f, 10.0f);
  float humAfter = Sound_strength(s, 50.0f, 1.0f, 1.0f);
  float toneAfter = Sound_strength(s, 440.0f, 1.0f, 1.0f);
  say("hum at 50 Hz: ");
  sayDecimal(humBefore);
  say(" before the notch, ");
  sayDecimal(humAfter);
  say(" after; the 440 Hz tone is ");
  sayDecimal(toneAfter);
  newline();
  if (humAfter > 0.01f || toneAfter < 0.47f || toneAfter > 0.51f) {
    return 15;
  }
  return drawHum();
}

static int64_t drawHum(void) {
  Biquad* notch = biquadNotch(44100, 50.0f, 10.0f);
  float* curve = (int8_t*) (intptr_t) calloc(320, 4);
  for (int64_t x = 0; x < 320; x = x + 1) {
    curve[x] = decibels(Biquad_gain(notch, frequencyAt(x), 44100));
  }
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawCurve(pic, 320, 96, curve, 320, (float) (0.0 - 40.0), 3.0f);
  markDecades(pic, 320, 96);
  savePicture("studio-hum.pgm", pic, 320, 96);
  return 0;
}

static float telephoneGain(float hz) {
  Biquad* high = biquadHighPass(44100, 300.0f, 0.7071f);
  Biquad* low = biquadLowPass(44100, 3400.0f, 0.7071f);
  return Biquad_gain(high, hz, 44100) * Biquad_gain(low, hz, 44100);
}

static int64_t checkTelephone(void) {
  say("telephone gain at 100 Hz, 1,000 Hz and 8,000 Hz: ");
  sayDecimal(telephoneGain(100.0f));
  say(", ");
  sayDecimal(telephoneGain(1000.0f));
  say(", ");
  sayDecimal(telephoneGain(8000.0f));
  newline();
  if (telephoneGain(100.0f) > 0.15f || telephoneGain(1000.0f) < 0.95f || telephoneGain(8000.0f) > 0.25f) {
    return 16;
  }
  float* curve = (int8_t*) (intptr_t) calloc(320, 4);
  for (int64_t x = 0; x < 320; x = x + 1) {
    curve[x] = decibels(telephoneGain(frequencyAt(x)));
  }
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawCurve(pic, 320, 96, curve, 320, (float) (0.0 - 40.0), 3.0f);
  markDecades(pic, 320, 96);
  savePicture("studio-phone.pgm", pic, 320, 96);
  return 0;
}

static int64_t main_(void) {
  Sound* mix = stereoMix();
  int64_t code = checkFile(mix);
  if (code == 0) {
    code = checkPan(mix);
  }
  if (code == 0) {
    code = checkResample(mix);
  }
  if (code == 0) {
    code = checkAliasing();
  }
  if (code == 0) {
    code = checkEqualizer();
  }
  if (code == 0) {
    code = checkHum();
  }
  if (code == 0) {
    code = checkTelephone();
  }
  if (code == 0 && getenv("POLYIR_PLAY") != 0) {
    if (soundPlay(mix) != 0) {
      code = 17;
    }
  }
  return code;
}

int main(void) {
  return (int) main_();
}
