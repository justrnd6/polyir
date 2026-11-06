# Shortens a list to at most `count` items, and gives it back.
def truncate(items: list, count: int) -> list:
    del items[max(count, 0):]
    return items
