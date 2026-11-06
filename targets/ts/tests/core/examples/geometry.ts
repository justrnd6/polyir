import { Rect, area } from "./shapes.js";

function rectArea(w: bigint, h: bigint): bigint {
  return area(new Rect(w, h));
}

export { rectArea };
