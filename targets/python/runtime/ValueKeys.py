# A dict whose keys, or a set whose members, may be lists, sets or dicts, compared by what they hold. Each is kept under a
# frozen stand-in that is equal exactly when the values are; the first one added stands for all values equal to it.
def frozen(value):
    if isinstance(value, list):
        return ("list", tuple(frozen(item) for item in value))
    if isinstance(value, (set, frozenset)):
        return ("set", frozenset(frozen(item) for item in value))
    if isinstance(value, dict):
        return ("dict", frozenset((frozen(key), frozen(value[key])) for key in value))
    return value


class ValueDict(dict):
    def __init__(self, entries=()):
        super().__init__()
        self.held = {}
        for key, value in ([(key, entries[key]) for key in entries] if isinstance(entries, dict) else entries):
            self[key] = value

    def __setitem__(self, key, value):
        self.held.setdefault(frozen(key), key)
        super().__setitem__(frozen(key), value)

    def __getitem__(self, key):
        return super().__getitem__(frozen(key))

    def get(self, key, default=None):
        return super().get(frozen(key), default)

    def __contains__(self, key):
        return super().__contains__(frozen(key))

    def __delitem__(self, key):
        super().__delitem__(frozen(key))
        del self.held[frozen(key)]

    def __iter__(self):
        return iter(list(self.held.values()))


class ValueSet(set):
    def __init__(self, items=()):
        super().__init__()
        self.held = {}
        for item in items:
            self.add(item)

    def add(self, item):
        self.held.setdefault(frozen(item), item)
        super().add(frozen(item))

    def __contains__(self, item):
        return super().__contains__(frozen(item))

    def discard(self, item):
        super().discard(frozen(item))
        self.held.pop(frozen(item), None)

    def __iter__(self):
        return iter(list(self.held.values()))
