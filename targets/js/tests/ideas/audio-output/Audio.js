"use strict";

import { Stream, soundStream, streamNew } from "./Stream.js";
import { audioStreamRun } from "./macos.js";
import { Sound } from "./../audio/Sound.js";

function audioStreamAt(fill, state, channels, rate, seconds, volume) {
  return audioStreamRun(streamNew(fill, state, channels, rate, seconds, volume));
}

function audioStream(fill, state, channels, rate, seconds) {
  return audioStreamAt(fill, state, channels, rate, seconds, 1.0);
}

function soundPlay(s) {
  return audioStreamRun(soundStream(s, 1.0));
}

function audioPlay(samples, count, rate) {
  return soundPlay(new Sound(samples, count, rate, 1n));
}

export { audioStreamAt, audioStream, soundPlay, audioPlay };
