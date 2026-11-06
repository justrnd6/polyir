"use strict";

import { metalDevice, metalReadFile } from "./../Metal.js";
import { runAll } from "./shared/classics.js";

function main() {
  const device = metalDevice();
  if (device === 0n) {
    return 10n;
  }
  const source = metalReadFile("classics.metal");
  if (source === "") {
    return 11n;
  }
  return runAll(device, source);
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
