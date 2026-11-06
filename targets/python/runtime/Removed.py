# Takes a key out of a dict, or a member out of a set, and says whether it was there.
def removed(items, key) -> bool:
    if key not in items:
        return False
    if isinstance(items, dict):
        del items[key]
    else:
        items.discard(key)
    return True
