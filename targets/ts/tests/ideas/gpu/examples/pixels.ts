declare const process: any;
export {};

import { metalDevice, metalReadFile } from "./../Metal.js";
import { bitsDemo, channelsDemo, enlargeDemo, facePicture, printTable, resizeDemo, scanDemo } from "./shared/pixels.js";

function main(): bigint {
  const device: bigint = metalDevice();
  if (device === 0n) {
    return 10n;
  }
  const source: string = metalReadFile("pixels.metal");
  if (source === "") {
    return 11n;
  }
  printTable(facePicture());
  let code: bigint = scanDemo(device, source);
  if (code !== 0n) {
    return code;
  }
  code = channelsDemo(device, source);
  if (code !== 0n) {
    return code;
  }
  code = bitsDemo(device, source);
  if (code !== 0n) {
    return code;
  }
  code = resizeDemo(device, source);
  if (code !== 0n) {
    return code;
  }
  return enlargeDemo(device, source);
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
