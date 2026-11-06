"use strict";

import { Rect, area } from "./shapes.js";

function rectArea(w, h) {
  return area(new Rect(w, h));
}

export { rectArea };
