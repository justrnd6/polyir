function convolveSample(i: number, x: Array<number>, h: Array<number>, p: Array<number>): number {
  const nx: number = p[0];
  const nh: number = p[1];
  let first: number = (i - ((nx - 1) | 0)) | 0;
  if (first < 0) {
    first = 0;
  }
  let last: number = i;
  if (last > ((nh - 1) | 0)) {
    last = (nh - 1) | 0;
  }
  let sum: number = 0.0;
  for (let j = first; j <= last; j = (j + 1) | 0) {
    sum = Math.fround(sum + Math.fround(x[(i - j) | 0] * h[j]));
  }
  return sum;
}

export { convolveSample };
