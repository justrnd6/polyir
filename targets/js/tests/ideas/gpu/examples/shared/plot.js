"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function drawStems(pic, w, h, values, n, low, high) {
  for (let k = 0n; k < i64(w * h); k = i64(k + 1n)) {
    pic[Number(k)] = 0.96;
  }
  const zero = BigInt(Math.trunc(Math.fround(Math.fround(high / Math.fround(high - low)) * Math.fround(Number(h)))));
  for (let x = 0n; x < w; x = i64(x + 1n)) {
    pic[Number(i64(i64(zero * w) + x))] = 0.55;
  }
  for (let j = 0n; j < n; j = i64(j + 1n)) {
    const column = BigInt(Math.trunc(Math.fround(Math.fround(Math.fround(Math.fround(Number(j)) + 0.5) * Math.fround(Number(w))) / Math.fround(Number(n)))));
    let top = BigInt(Math.trunc(Math.fround(Math.fround(Math.fround(high - values[Number(j)]) / Math.fround(high - low)) * Math.fround(Number(i64(h - 1n))))));
    if (top < 0n) {
      top = 0n;
    }
    if (top > i64(h - 1n)) {
      top = i64(h - 1n);
    }
    const from = minOf(top, zero);
    const to = maxOf(top, zero);
    for (let y = from; y <= to; y = i64(y + 1n)) {
      pic[Number(i64(i64(y * w) + column))] = 0.1;
    }
    for (let dy = i64(0n - 1n); dy <= 1n; dy = i64(dy + 1n)) {
      for (let dx = i64(0n - 1n); dx <= 1n; dx = i64(dx + 1n)) {
        const yy = i64(top + dy);
        const xx = i64(column + dx);
        if (yy >= 0n && yy < h && xx >= 0n && xx < w) {
          pic[Number(i64(i64(yy * w) + xx))] = 0.1;
        }
      }
    }
  }
  return 0n;
}

function minOf(a, b) {
  if (a < b) {
    return a;
  }
  return b;
}

function maxOf(a, b) {
  if (a > b) {
    return a;
  }
  return b;
}

function drawWave(pic, w, h, values, n, low, high) {
  for (let k = 0n; k < i64(w * h); k = i64(k + 1n)) {
    pic[Number(k)] = 0.96;
  }
  const zero = BigInt(Math.trunc(Math.fround(Math.fround(high / Math.fround(high - low)) * Math.fround(Number(h)))));
  for (let x = 0n; x < w; x = i64(x + 1n)) {
    pic[Number(i64(i64(zero * w) + x))] = 0.55;
  }
  for (let x = 0n; x < w; x = i64(x + 1n)) {
    const from = i64(x * n) / w;
    let upto = i64(i64(x + 1n) * n) / w;
    if (upto <= from) {
      upto = i64(from + 1n);
    }
    let lowest = values[Number(from)];
    let highest = values[Number(from)];
    for (let j = from; j < upto; j = i64(j + 1n)) {
      if (values[Number(j)] < lowest) {
        lowest = values[Number(j)];
      }
      if (values[Number(j)] > highest) {
        highest = values[Number(j)];
      }
    }
    let top = BigInt(Math.trunc(Math.fround(Math.fround(Math.fround(high - highest) / Math.fround(high - low)) * Math.fround(Number(i64(h - 1n))))));
    let bottom = BigInt(Math.trunc(Math.fround(Math.fround(Math.fround(high - lowest) / Math.fround(high - low)) * Math.fround(Number(i64(h - 1n))))));
    if (top < 0n) {
      top = 0n;
    }
    if (bottom > i64(h - 1n)) {
      bottom = i64(h - 1n);
    }
    for (let y = top; y <= bottom; y = i64(y + 1n)) {
      pic[Number(i64(i64(y * w) + x))] = 0.1;
    }
  }
  return 0n;
}

function drawCurve(pic, w, h, values, n, low, high) {
  for (let k = 0n; k < i64(w * h); k = i64(k + 1n)) {
    pic[Number(k)] = 0.96;
  }
  const zero = BigInt(Math.trunc(Math.fround(Math.fround(high / Math.fround(high - low)) * Math.fround(Number(h)))));
  for (let x = 0n; x < w; x = i64(x + 1n)) {
    pic[Number(i64(i64(zero * w) + x))] = 0.55;
  }
  let previous = curveRow(values[0], h, low, high);
  for (let x = 0n; x < w; x = i64(x + 1n)) {
    const row = curveRow(values[Number(i64(x * n) / w)], h, low, high);
    for (let y = minOf(row, previous); y <= maxOf(row, previous); y = i64(y + 1n)) {
      pic[Number(i64(i64(y * w) + x))] = 0.1;
    }
    previous = row;
  }
  return 0n;
}

function curveRow(value, h, low, high) {
  let row = BigInt(Math.trunc(Math.fround(Math.fround(Math.fround(high - value) / Math.fround(high - low)) * Math.fround(Number(i64(h - 1n))))));
  if (row < 0n) {
    row = 0n;
  }
  if (row > i64(h - 1n)) {
    row = i64(h - 1n);
  }
  return row;
}

export { drawStems, minOf, maxOf, drawWave, drawCurve, curveRow };
