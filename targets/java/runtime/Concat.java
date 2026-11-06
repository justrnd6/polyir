// Two lists joined into a new one.
static <T> ArrayList<T> concat(ArrayList<T> a, ArrayList<T> b) {
    final ArrayList<T> out = new ArrayList<>(a);
    out.addAll(b);
    return out;
}
