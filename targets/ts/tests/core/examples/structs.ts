class Point {
  constructor(public x: number, public y: number) {}
}

function distanceSquared(a: Point, b: Point): number {
  const dx: number = a.x - b.x;
  const dy: number = a.y - b.y;
  return dx * dx + dy * dy;
}

function distFromOrigin(bx: number, by: number): number {
  return distanceSquared(new Point(0.0, 0.0), new Point(bx, by));
}

export { Point, distanceSquared, distFromOrigin };
