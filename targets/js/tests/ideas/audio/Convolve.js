"use strict";

function convolveSample(i, x, h, p) {
  const nx = p[0];
  const nh = p[1];
  let first = (i - ((nx - 1) | 0)) | 0;
  if (first < 0) {
    first = 0;
  }
  let last = i;
  if (last > ((nh - 1) | 0)) {
    last = (nh - 1) | 0;
  }
  let sum = 0.0;
  for (let j = first; j <= last; j = (j + 1) | 0) {
    sum = Math.fround(sum + Math.fround(x[(i - j) | 0] * h[j]));
  }
  return sum;
}

export { convolveSample };
