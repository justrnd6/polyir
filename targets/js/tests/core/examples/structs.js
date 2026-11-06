"use strict";

class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

function distanceSquared(a, b) {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return dx * dx + dy * dy;
}

function distFromOrigin(bx, by) {
  return distanceSquared(new Point(0.0, 0.0), new Point(bx, by));
}

export { Point, distanceSquared, distFromOrigin };
