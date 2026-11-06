declare const process: any;
export {};

interface Runnable {
  run(): bigint;
}

class Job {
  constructor(public id: bigint) {}

  run(): bigint {
    return this.id;
  }
}

function main(): bigint {
  return 0n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
