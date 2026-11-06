"use strict";

import { vulkanStart } from "./../Vulkan.js";
import { runAll } from "./shared/handed.js";

function main() {
  const gpu = vulkanStart();
  if (gpu === 0n) {
    return 10n;
  }
  return runAll(gpu);
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
