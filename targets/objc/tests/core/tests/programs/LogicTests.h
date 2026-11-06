#pragma once

#import <stdbool.h>
#import <stdint.h>

#import "Check.h"

static int64_t calls = 0;
static int64_t noted = 0;

static bool tick(void);
static double checkAndSkipsRightSideWhenLeftIsFalse(void);
static double checkAndRunsRightSideWhenLeftIsTrue(void);
static double checkOrSkipsRightSideWhenLeftIsTrue(void);
static double checkOrRunsRightSideWhenLeftIsFalse(void);
static double checkNullGuardWithAnd(void);
static double checkNullGuardWithOr(void);
static double checkLoopConditionShortCircuits(void);
static double checkLetShortCircuits(void);
static double checkVarAndAssignShortCircuit(void);
static bool bothTrue(int64_t a, int64_t b);
static double checkReturnValue(void);
static double checkNotOfAnd(void);
static double checkAndBindsTighterThanOr(void);
static double checkAndBindsTighterThanOrOnTheLeft(void);
static double checkChain(void);
static int64_t gradeOf(int64_t n);
static double checkElseIfChain(void);
static void note(int64_t n);
static double checkBareReturn(void);

static bool tick(void) {
  calls = calls + 1;
  return true;
}

static double checkAndSkipsRightSideWhenLeftIsFalse(void) {
  calls = 0;
  if (1 == 2 && tick()) {
    return 1.0;
  }
  return offInt(calls, 0);
}

static double checkAndRunsRightSideWhenLeftIsTrue(void) {
  calls = 0;
  if (1 == 1 && tick()) {
    return offInt(calls, 1);
  }
  return 1.0;
}

static double checkOrSkipsRightSideWhenLeftIsTrue(void) {
  calls = 0;
  if (1 == 1 || tick()) {
    return offInt(calls, 0);
  }
  return 1.0;
}

static double checkOrRunsRightSideWhenLeftIsFalse(void) {
  calls = 0;
  if (1 == 2 || tick()) {
    return offInt(calls, 1);
  }
  return 1.0;
}

static double checkNullGuardWithAnd(void) {
  int64_t* p = (int8_t*) (intptr_t) 0;
  if ((int64_t) (intptr_t) p != 0 && p[0] == 5) {
    return 1.0;
  }
  return 0.0;
}

static double checkNullGuardWithOr(void) {
  int64_t* p = (int8_t*) (intptr_t) 0;
  if ((int64_t) (intptr_t) p == 0 || p[0] == 5) {
    return 0.0;
  }
  return 1.0;
}

static double checkLoopConditionShortCircuits(void) {
  calls = 0;
  int64_t rounds = 0;
  for (int64_t i = 0; i < 5 && tick(); i = i + 1) {
    rounds = rounds + 1;
  }
  return offInt(rounds, 5) + offInt(calls, 5);
}

static double checkLetShortCircuits(void) {
  calls = 0;
  bool ok = 1 == 2 && tick();
  if (ok) {
    return 1.0;
  }
  return offInt(calls, 0);
}

static double checkVarAndAssignShortCircuit(void) {
  calls = 0;
  bool ok = 1 == 2 && tick();
  ok = 1 == 1 || tick();
  if (ok) {
    return offInt(calls, 0);
  }
  return 1.0;
}

static bool bothTrue(int64_t a, int64_t b) {
  return a > 0 && b > 0;
}

static double checkReturnValue(void) {
  int64_t wrong = 0;
  if (bothTrue(1, 1) == false) {
    wrong = wrong + 1;
  }
  if (bothTrue(1, 0)) {
    wrong = wrong + 1;
  }
  if (bothTrue(0, 1)) {
    wrong = wrong + 1;
  }
  return offInt(wrong, 0);
}

static double checkNotOfAnd(void) {
  int64_t* p = (int8_t*) (intptr_t) 0;
  if (!((int64_t) (intptr_t) p != 0 && p[0] == 5)) {
    return 0.0;
  }
  return 1.0;
}

static double checkAndBindsTighterThanOr(void) {
  if (1 == 1 || 1 == 2 && 1 == 2) {
    return 0.0;
  }
  return 1.0;
}

static double checkAndBindsTighterThanOrOnTheLeft(void) {
  if (1 == 2 && 1 == 2 || 1 == 1) {
    return 0.0;
  }
  return 1.0;
}

static double checkChain(void) {
  int64_t wrong = 0;
  if (!(1 == 1 && 2 == 2 && 3 == 3)) {
    wrong = wrong + 1;
  }
  if (1 == 1 && 2 == 2 && 3 == 4) {
    wrong = wrong + 1;
  }
  if (1 == 2 || 2 == 3 || 3 == 4) {
    wrong = wrong + 1;
  }
  if (!(1 == 2 || 2 == 3 || 3 == 3)) {
    wrong = wrong + 1;
  }
  return offInt(wrong, 0);
}

static int64_t gradeOf(int64_t n) {
  return n > 90 ? 4 : n > 80 ? 3 : n > 70 ? 2 : 1;
}

static double checkElseIfChain(void) {
  return offInt(gradeOf(95) * 1000 + gradeOf(85) * 100 + gradeOf(75) * 10 + gradeOf(10), 4321);
}

static void note(int64_t n) {
  if (n < 0) {
    return;
  }
  noted = noted + n;
}

static double checkBareReturn(void) {
  noted = 0;
  note(5);
  note(0 - 3);
  note(7);
  return offInt(noted, 12);
}
