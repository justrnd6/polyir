// Shortens a list to at most `count` items, and gives it back.
function truncate(items, count) {
  items.length = Math.min(items.length, Number(count));
  return items;
}
