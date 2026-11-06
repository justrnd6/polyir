# A push used as a value gives the list, as in polyir (Python's append gives nothing).
def pushed(items: list, item) -> list:
    items.append(item)
    return items
