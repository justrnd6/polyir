"use strict";

function putchar(c) {
  process.stdout.write(String.fromCharCode(Number(c) & 255));
  return c;
}

function getchar() {
  const byte = Buffer.alloc(1);
  return require("fs").readSync(0, byte, 0, 1, null) === 1 ? byte[0] : -1;
}

function writeLine() {
  putchar(104);
  putchar(108);
  putchar(108);
  putchar(118);
  putchar(109);
  putchar(32);
  putchar(105);
  putchar(111);
  return putchar(10);
}

function echoOneByte() {
  const c = getchar();
  return putchar(c);
}

function main() {
  writeLine();
  echoOneByte();
  return 0n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
