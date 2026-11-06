#import <stdint.h>

static int64_t mix(int64_t h, int64_t v);
static int64_t wide(void);
static int64_t thirtyTwo(void);
static int64_t unsigned32(void);
static int64_t narrow(void);
static int64_t casts(void);
static int64_t main_(void);

static int64_t mix(int64_t h, int64_t v) {
  return (h * 31 + v) % 1000003;
}

static int64_t wide(void) {
  int64_t big = 9223372036854775807;
  int64_t wrapped = big + 1;
  int64_t product = 3037000500 * 3037000500;
  int64_t neg = 0 - big;
  int64_t quotient = (0 - 7) / 2;
  int64_t rest = (0 - 7) % 3;
  int64_t h = 7;
  h = mix(h, (wrapped >> 40) % 1000);
  h = mix(h, (product >> 33) % 1000);
  h = mix(h, (neg >> 50) % 1000);
  h = mix(h, quotient + 100);
  h = mix(h, rest + 100);
  h = mix(h, big << 3 >> 60);
  h = mix(h, 0 - 1 >> 5 + 100);
  return h;
}

static int64_t thirtyTwo(void) {
  int32_t big = 2147483647;
  int32_t wrapped = big + 1;
  int32_t product = (int32_t) (65536 * 65536);
  int32_t other = (int32_t) (123456789 * 987);
  int32_t quotient = (int32_t) ((0 - 7) / 2);
  int32_t rest = (int32_t) ((0 - 7) % 3);
  int64_t h = 11;
  h = mix(h, (int64_t) wrapped % 100003);
  h = mix(h, (int64_t) product + 5);
  h = mix(h, (int64_t) other % 100003);
  h = mix(h, (int64_t) quotient + 100);
  h = mix(h, (int64_t) rest + 100);
  h = mix(h, (int64_t) (wrapped >> 7) % 100003);
  h = mix(h, (int64_t) (big << 5) % 100003);
  return h;
}

static int64_t unsigned32(void) {
  uint32_t top = 4294967295u;
  uint32_t wrapped = top + 2u;
  uint32_t product = (uint32_t) (4000000000 * 7);
  uint32_t under = (uint32_t) (3 - 5);
  uint32_t shifted = top << 4u;
  int64_t h = 13;
  h = mix(h, (int64_t) wrapped);
  h = mix(h, (int64_t) product % 100003);
  h = mix(h, (int64_t) under % 100003);
  h = mix(h, (int64_t) shifted % 100003);
  h = mix(h, (int64_t) (top >> 3u) % 100003);
  h = mix(h, (int64_t) (top / 3u) % 100003);
  h = mix(h, (int64_t) (top % 1000u));
  return h;
}

static int64_t narrow(void) {
  int8_t a = 127;
  int8_t b = (int8_t) (a + 1);
  int16_t c = 32767;
  int16_t d = (int16_t) (c + 1);
  uint8_t e = 250;
  uint8_t f = (uint8_t) (e + 10);
  uint16_t g = 65535;
  uint16_t hh = (uint16_t) (g + 3);
  int8_t m = (int8_t) (0 - 128);
  int8_t n = (int8_t) (m / (int8_t) (0 - 1));
  int64_t h = 17;
  h = mix(h, (int64_t) b + 200);
  h = mix(h, (int64_t) d + 40000);
  h = mix(h, (int64_t) f);
  h = mix(h, (int64_t) hh);
  h = mix(h, (int64_t) n + 200);
  h = mix(h, (int64_t) (int8_t) (b >> 2) + 200);
  h = mix(h, (int64_t) (uint8_t) (e << 3));
  return h;
}

static int64_t casts(void) {
  int64_t big = 5000000000;
  int32_t cut = (int32_t) big;
  int8_t tiny = (int8_t) 300;
  uint32_t uns = (uint32_t) (0 - 5);
  uint64_t up = (uint64_t) (0 - 1);
  int64_t back = (int64_t) up;
  double f = 3.99;
  double g = 0.0 - 3.99;
  int64_t h = 19;
  h = mix(h, (int64_t) cut % 100003);
  h = mix(h, (int64_t) tiny + 200);
  h = mix(h, (int64_t) uns % 100003);
  h = mix(h, (int64_t) (up >> 60u));
  h = mix(h, back + 5);
  h = mix(h, (int64_t) f);
  h = mix(h, (int64_t) g + 10);
  h = mix(h, (int64_t) (f * 2.5));
  return h;
}

static int64_t main_(void) {
  int64_t h = 1;
  h = mix(h, wide());
  h = mix(h, thirtyTwo());
  h = mix(h, unsigned32());
  h = mix(h, narrow());
  h = mix(h, casts());
  return h % 251;
}

int main(void) {
  return (int) main_();
}
