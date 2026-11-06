// Shortens a list to at most `count` items, and gives it back.
function truncate<T>(items: T[], count: bigint): T[] {
  items.length = Math.min(items.length, Number(count));
  return items;
}
