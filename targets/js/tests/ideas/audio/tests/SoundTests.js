"use strict";

import { off, offInt } from "./../../../core/tests/programs/Check.js";
import { Sound, soundNew } from "./../Sound.js";
import { sine } from "./../Wave.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function quietTone() {
  const s = soundNew(0.1, 1000n);
  s.tone(0n, 125.0, 0.5, 0.0, 0.1);
  return s;
}

function checkNewSoundIsMono() {
  return offInt(soundNew(1.0, 8000n).channels, 1n);
}

function checkNewSoundLength() {
  return offInt(soundNew(2.5, 8000n).count, 20000n);
}

function checkSeconds() {
  return off(soundNew(2.5, 8000n).seconds(), 2.5);
}

function checkTonePeak() {
  return off(quietTone().peak(), 0.5);
}

function checkSquareTonePeak() {
  const s = soundNew(0.1, 1000n);
  s.tone(1n, 125.0, 0.4, 0.0, 0.1);
  return off(s.peak(), 0.4);
}

function checkScale() {
  const s = quietTone();
  s.scale(0.5);
  return off(s.peak(), 0.25);
}

function checkMixAdds() {
  const a = quietTone();
  a.add(quietTone(), 0.0, 1.0);
  return off(a.peak(), 1.0);
}

function checkNormalize() {
  const s = soundNew(0.1, 1000n);
  s.tone(0n, 125.0, 0.3, 0.0, 0.1);
  s.normalize(0.9);
  return off(s.peak(), 0.9);
}

function checkDelayedMixKeepsLength() {
  const a = soundNew(1.0, 1000n);
  a.add(quietTone(), 0.5, 1.0);
  return offInt(a.count, 1000n);
}

function checkDelayedMixIsSilentBefore() {
  const a = soundNew(1.0, 1000n);
  a.add(quietTone(), 0.5, 1.0);
  return off(a.samples[499], 0.0);
}

function checkDelayedMixStartsLater() {
  const a = soundNew(1.0, 1000n);
  a.add(quietTone(), 0.5, 1.0);
  return off(a.samples[525], 0.35355339);
}

function checkToneFadesIn() {
  const s = soundNew(0.5, 8000n);
  s.tone(0n, 440.0, 1.0, 0.0, 0.5);
  return off(s.samples[0], 0.0);
}

function checkNoiseAmplitude() {
  const s = soundNew(0.1, 8000n);
  s.noise(0.25, 7n);
  const loudest = s.peak();
  if (loudest > 0.25) {
    return 1.0;
  }
  if (loudest < 0.2) {
    return 1.0;
  }
  return 0.0;
}

function filterGain(kind, hz, cutoff) {
  const s = soundNew(1.0, 8000n);
  const data = s.samples;
  for (let i = 0n; i < 8000n; i = i64(i + 1n)) {
    data[Number(i)] = sine(Math.fround(Math.fround(hz * Math.fround(Number(i))) / 8000.0));
  }
  if (kind === 0n) {
    s.lowPass(cutoff);
  } else {
    s.highPass(cutoff);
  }
  let loudest = 0.0;
  for (let i = 6000n; i < 8000n; i = i64(i + 1n)) {
    let level = data[Number(i)];
    if (level < 0.0) {
      level = Math.fround(0.0 - level);
    }
    if (level > loudest) {
      loudest = level;
    }
  }
  return loudest;
}

function checkLowPassCutsHighTone() {
  return off(filterGain(0n, 1000.0, 500.0), 0.3987);
}

function checkLowPassKeepsLowTone() {
  return off(filterGain(0n, 200.0, 500.0), 0.9031);
}

function checkHighPassKeepsHighTone() {
  return off(filterGain(1n, 1000.0, 500.0), 0.7225);
}

function checkHighPassCutsLowTone() {
  return off(filterGain(1n, 200.0, 500.0), 0.3613);
}

function checkEchoFirstRepeat() {
  const s = soundNew(0.5, 1000n);
  s.samples[0] = 1.0;
  s.echo(0.1, 0.5);
  return off(s.samples[100], 0.5);
}

function checkEchoSecondRepeat() {
  const s = soundNew(0.5, 1000n);
  s.samples[0] = 1.0;
  s.echo(0.1, 0.5);
  return off(s.samples[200], 0.25);
}

function convolved() {
  const a = soundNew(0.002, 1000n);
  const r = soundNew(0.003, 1000n);
  a.samples[0] = 1.0;
  a.samples[1] = 2.0;
  r.samples[0] = 1.0;
  r.samples[2] = 3.0;
  return a.convolve(r);
}

function checkConvolutionLength() {
  return offInt(convolved().count, 4n);
}

function checkConvolutionValues() {
  const y = convolved().samples;
  return off(y[0], 1.0) + off(y[1], 2.0) + off(y[2], 3.0) + off(y[3], 6.0);
}

function reverbOfImpulse() {
  const s = soundNew(1.0, 10000n);
  s.samples[0] = 1.0;
  s.reverb(1.0);
  return s;
}

function checkReverbStart() {
  return off(reverbOfImpulse().samples[0], 0.20525);
}

function checkReverbFirstEcho() {
  return off(reverbOfImpulse().samples[297], 0.03924);
}

export { quietTone, checkNewSoundIsMono, checkNewSoundLength, checkSeconds, checkTonePeak, checkSquareTonePeak, checkScale, checkMixAdds, checkNormalize, checkDelayedMixKeepsLength, checkDelayedMixIsSilentBefore, checkDelayedMixStartsLater, checkToneFadesIn, checkNoiseAmplitude, filterGain, checkLowPassCutsHighTone, checkLowPassKeepsLowTone, checkHighPassKeepsHighTone, checkHighPassCutsLowTone, checkEchoFirstRepeat, checkEchoSecondRepeat, convolved, checkConvolutionLength, checkConvolutionValues, reverbOfImpulse, checkReverbStart, checkReverbFirstEcho };
