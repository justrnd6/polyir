# A new dict with one more entry, or a new set with one more member.
def with_entry(table, key, value):
    out = type(table)(table)
    out[key] = value
    return out


def with_item(items, item):
    out = type(items)(items)
    out.add(item)
    return out
