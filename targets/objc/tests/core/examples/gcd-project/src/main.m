#import <stdint.h>

#import "euclid.h"

static int64_t main_(void);

static int64_t main_(void) {
  return gcd(48, 18) * 10 + lcm(4, 6);
}

int main(void) {
  return (int) main_();
}
