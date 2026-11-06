import { Stream, soundStream, streamNew } from "./Stream.js";
import { audioStreamRun } from "./macos.js";
import { Sound } from "./../audio/Sound.js";

function audioStreamAt(fill: Array<(...args: [bigint, Array<number>, bigint, bigint]) => bigint>, state: bigint, channels: bigint, rate: bigint, seconds: number, volume: number): bigint {
  return audioStreamRun(streamNew(fill, state, channels, rate, seconds, volume));
}

function audioStream(fill: Array<(...args: [bigint, Array<number>, bigint, bigint]) => bigint>, state: bigint, channels: bigint, rate: bigint, seconds: number): bigint {
  return audioStreamAt(fill, state, channels, rate, seconds, 1.0);
}

function soundPlay(s: Sound): bigint {
  return audioStreamRun(soundStream(s, 1.0));
}

function audioPlay(samples: Array<number>, count: bigint, rate: bigint): bigint {
  return soundPlay(new Sound(samples, count, rate, 1n));
}

export { audioStreamAt, audioStream, soundPlay, audioPlay };
