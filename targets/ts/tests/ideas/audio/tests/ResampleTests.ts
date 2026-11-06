import { amplitudeAt, off, offInt } from "./../../../core/tests/programs/Check.js";
import { resampledCount } from "./../Resample.js";
import { Sound, soundNew, soundStereo } from "./../Sound.js";

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const sin = Math.sin;

function sineSound(hz: number, seconds: number, rate: bigint): Sound {
  const s: Sound = soundNew(seconds, rate);
  for (let i = 0n; i < s.count; i = i64(i + 1n)) {
    s.samples[Number(i)] = Math.fround(sin(6.283185307179586 * hz * Number(i) / Number(rate)));
  }
  return s;
}

function checkCountGoingUp(): number {
  return offInt(resampledCount(44100n, 44100n, 48000n), 48000n);
}

function checkCountGoingDown(): number {
  return offInt(resampledCount(1000n, 8000n, 4000n), 500n);
}

function checkCountRounds(): number {
  return offInt(resampledCount(10n, 3n, 4n), 13n);
}

function checkSoundKeepsItsLength(): number {
  const s: Sound = sineSound(440.0, 0.5, 8000n).resample(12000n);
  return offInt(s.rate, 12000n) + offInt(s.count, 6000n) + offInt(s.channels, 1n) + off(s.seconds(), 0.5);
}

function checkUpsamplingFollowsTheWave(): number {
  const s: Sound = sineSound(440.0, 1.0, 8000n).resample(12000n);
  let worst: number = 0.0;
  for (let j = 1000n; j < 11000n; j = i64(j + 7n)) {
    const expected: number = sin(6.283185307179586 * 440.0 * Number(j) / 12000.0);
    let gap: number = s.samples[Number(j)] - expected;
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst;
}

function checkDownsamplingRemovesAliases(): number {
  const s: Sound = sineSound(3000.0, 1.0, 8000n).resample(4000n);
  return amplitudeAt(s.samples, 500n, 2000n, 4000n, 1000.0);
}

function checkDownsamplingKeepsLowTones(): number {
  const s: Sound = sineSound(1000.0, 1.0, 8000n).resample(4000n);
  return off(amplitudeAt(s.samples, 500n, 2000n, 4000n, 1000.0), 1.0);
}

function checkRoundTrip(): number {
  const original: Sound = sineSound(300.0, 1.0, 8000n);
  const back: Sound = original.resample(12000n).resample(8000n);
  let worst: number = 0.0;
  for (let j = 1000n; j < 7000n; j = i64(j + 11n)) {
    let gap: number = Math.fround(back.samples[Number(j)] - original.samples[Number(j)]);
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst;
}

function checkConstantStaysConstant(): number {
  const s: Sound = soundNew(0.5, 8000n);
  for (let i = 0n; i < s.count; i = i64(i + 1n)) {
    s.samples[Number(i)] = 1.0;
  }
  const up: Sound = s.resample(22050n);
  const down: Sound = s.resample(3000n);
  return off(up.samples[Number(up.count / 2n)], 1.0) + off(down.samples[Number(down.count / 2n)], 1.0);
}

function checkStereoResampling(): number {
  const left: Sound = sineSound(440.0, 0.5, 8000n);
  const right: Sound = sineSound(880.0, 0.5, 8000n);
  const out: Sound = soundStereo(left, right).resample(16000n);
  return offInt(out.channels, 2n) + offInt(out.count, 8000n) + off(amplitudeAt(out.channel(0n).samples, 2000n, 4000n, 16000n, 440.0), 1.0) + off(amplitudeAt(out.channel(1n).samples, 2000n, 4000n, 16000n, 880.0), 1.0);
}

function checkSameRateIsACopy(): number {
  const s: Sound = sineSound(440.0, 0.1, 8000n);
  const copy: Sound = s.resample(8000n);
  let worst: number = 0.0;
  for (let i = 100n; i < 700n; i = i64(i + 1n)) {
    let gap: number = Math.fround(copy.samples[Number(i)] - s.samples[Number(i)]);
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst + offInt(copy.count, s.count);
}

export { sineSound, checkCountGoingUp, checkCountGoingDown, checkCountRounds, checkSoundKeepsItsLength, checkUpsamplingFollowsTheWave, checkDownsamplingRemovesAliases, checkDownsamplingKeepsLowTones, checkRoundTrip, checkConstantStaysConstant, checkStereoResampling, checkSameRateIsACopy };
