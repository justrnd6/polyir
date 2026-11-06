// A push used as a value gives the list, as in polyir (JavaScript's push gives the length).
function pushed<T>(items: T[], item: T): T[] {
  items.push(item);
  return items;
}
