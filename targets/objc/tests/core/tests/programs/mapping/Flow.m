#import <stdint.h>

static int64_t sumTo(int64_t n);
static int64_t gcd(int64_t a0, int64_t b0);
static int64_t collatz(int64_t n);
static int64_t swapped(int64_t a, int64_t b);
static int64_t weight(int64_t day);
static int64_t zigzag(int64_t a);
static int64_t main_(void);

static int64_t sumTo(int64_t n) {
  int64_t label = 0;
  int64_t total = 0;
  int64_t i = 0;
  int64_t result = 0;
  for (;;) {
    switch (label) {
      case 0: {
        total = 0;
        i = 1;
        label = 1;
        continue;
      }
      case 1: {
        if (i > n) {
          result = total;
          label = 3;
          continue;
        } else {
          label = 2;
          continue;
        }
      }
      case 2: {
        total = total + i;
        i = i + 1;
        label = 1;
        continue;
      }
      case 3: {
        return result;
      }
    }
  }
}

static int64_t gcd(int64_t a0, int64_t b0) {
  int64_t label = 0;
  int64_t a = 0;
  int64_t b = 0;
  int64_t result = 0;
  for (;;) {
    switch (label) {
      case 0: {
        a = a0;
        b = b0;
        label = 1;
        continue;
      }
      case 1: {
        if (b == 0) {
          result = a;
          label = 3;
          continue;
        } else {
          label = 2;
          continue;
        }
      }
      case 2: {
        int64_t r = a % b;
        a = b;
        b = r;
        label = 1;
        continue;
      }
      case 3: {
        return result;
      }
    }
  }
}

static int64_t collatz(int64_t n) {
  int64_t label = 0;
  int64_t x = 0;
  int64_t steps = 0;
  int64_t count = 0;
  for (;;) {
    switch (label) {
      case 0: {
        x = n;
        steps = 0;
        label = 1;
        continue;
      }
      case 1: {
        if (x == 1) {
          count = steps;
          label = 5;
          continue;
        } else {
          label = 2;
          continue;
        }
      }
      case 2: {
        if (x % 2 == 0) {
          label = 3;
          continue;
        } else {
          label = 4;
          continue;
        }
      }
      case 3: {
        x = x / 2;
        steps = steps + 1;
        label = 1;
        continue;
      }
      case 4: {
        x = 3 * x + 1;
        steps = steps + 1;
        label = 1;
        continue;
      }
      case 5: {
        return count;
      }
    }
  }
}

static int64_t swapped(int64_t a, int64_t b) {
  int64_t label = 0;
  int64_t x = 0;
  int64_t y = 0;
  int64_t k = 0;
  for (;;) {
    switch (label) {
      case 0: {
        x = a;
        y = b;
        k = 0;
        label = 1;
        continue;
      }
      case 1: {
        if (k == 3) {
          label = 3;
          continue;
        } else {
          label = 2;
          continue;
        }
      }
      case 2: {
        int64_t next_x = y;
        int64_t next_y = x;
        int64_t next_k = k + 1;
        x = next_x;
        y = next_y;
        k = next_k;
        label = 1;
        continue;
      }
      case 3: {
        return x * 10 + y;
      }
    }
  }
}

static int64_t weight(int64_t day) {
  int64_t label = 0;
  int64_t w = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (day == 0) {
          label = 1;
          continue;
        } else if (day == 6) {
          label = 1;
          continue;
        } else if (day == 1) {
          label = 2;
          continue;
        } else if (day == 2) {
          label = 2;
          continue;
        } else {
          label = 3;
          continue;
        }
      }
      case 1: {
        w = 1;
        label = 4;
        continue;
      }
      case 2: {
        w = 2;
        label = 4;
        continue;
      }
      case 3: {
        w = 3;
        label = 4;
        continue;
      }
      case 4: {
        return w * 100;
      }
    }
  }
}

static int64_t zigzag(int64_t a) {
  int64_t label = 0;
  int64_t p = 0;
  int64_t q = 0;
  int64_t r = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (a == 0) {
          p = a;
          label = 1;
          continue;
        } else {
          q = a;
          label = 2;
          continue;
        }
      }
      case 1: {
        int64_t next = p + 1;
        if (next > 5) {
          r = next;
          label = 3;
          continue;
        } else {
          q = next;
          label = 2;
          continue;
        }
      }
      case 2: {
        if (q > 5) {
          r = q;
          label = 3;
          continue;
        } else {
          p = q + 1;
          label = 1;
          continue;
        }
      }
      case 3: {
        return r * 2;
      }
    }
  }
}

static int64_t main_(void) {
  int64_t score = sumTo(10);
  score = score + gcd(48, 18);
  score = score + collatz(6);
  score = score + swapped(4, 7);
  score = score + weight(0) + weight(2) + weight(9);
  score = score + zigzag(0) + zigzag(3);
  return score;
}

int main(void) {
  return (int) main_();
}
