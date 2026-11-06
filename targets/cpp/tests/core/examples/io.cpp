#include <cstdint>
#include <cstdio>

int32_t writeLine();
int32_t echoOneByte();

inline int32_t writeLine() {
  putchar(104);
  putchar(108);
  putchar(108);
  putchar(118);
  putchar(109);
  putchar(32);
  putchar(105);
  putchar(111);
  return putchar(10);
}

inline int32_t echoOneByte() {
  const int32_t c = getchar();
  return putchar(c);
}

int main() {
  writeLine();
  echoOneByte();
  return 0;
}
