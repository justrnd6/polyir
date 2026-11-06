declare const process: any;
export {};

import { vulkanStart } from "./../Vulkan.js";
import { runAll } from "./shared/handed.js";

function main(): bigint {
  const gpu: bigint = vulkanStart();
  if (gpu === 0n) {
    return 10n;
  }
  return runAll(gpu);
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
