"use strict";

function main() {
  return 0n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
