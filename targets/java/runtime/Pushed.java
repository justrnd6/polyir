// A push used as a value gives the list, as in polyir (Java's add gives whether it changed the list).
static <T> ArrayList<T> pushed(ArrayList<T> items, T item) {
    items.add(item);
    return items;
}
