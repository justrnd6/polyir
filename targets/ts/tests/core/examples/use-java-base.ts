declare const process: any;
export {};

interface Runnable {
  run(): bigint;
}

function main(): bigint {
  return 0n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
