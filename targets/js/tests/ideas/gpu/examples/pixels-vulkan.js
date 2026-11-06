"use strict";

import { vulkanStart } from "./../Vulkan.js";
import { bitsDemo, channelsDemo, enlargeDemo, facePicture, printTable, resizeDemo, scanDemo } from "./shared/pixels.js";

function main() {
  const gpu = vulkanStart();
  if (gpu === 0n) {
    return 10n;
  }
  printTable(facePicture());
  let code = scanDemo(gpu, "");
  if (code !== 0n) {
    return code;
  }
  code = channelsDemo(gpu, "");
  if (code !== 0n) {
    return code;
  }
  code = bitsDemo(gpu, "");
  if (code !== 0n) {
    return code;
  }
  code = resizeDemo(gpu, "");
  if (code !== 0n) {
    return code;
  }
  return enlargeDemo(gpu, "");
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
