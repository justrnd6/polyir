"use strict";

class Job {
  constructor(id) {
    this.id = id;
  }

  run() {
    return this.id;
  }
}

function main() {
  return 0n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
