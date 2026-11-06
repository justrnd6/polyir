# A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
# finished; `give` is a yield that hears whether the coroutine was closed.
class Coroutine:
    def __init__(self, body, *args):
        self.finished = False
        self.started = False
        self.waiting = False
        self.source = body(self, *args)

    def resume(self) -> Variant:
        if self.finished:
            return Variant(1, None)
        self.waiting = False
        try:
            value = self.source.send(False) if self.started else next(self.source)
        except StopIteration:
            self.finished = True
            return Variant(1, None)
        self.started = True
        return Variant(0, value)

    def done(self) -> bool:
        return self.finished

    def give(self, value):
        self.waiting = True
        return (yield value) is True

    def close(self):
        if not self.finished and self.waiting:
            try:
                self.source.send(True)
            except StopIteration:
                pass
        self.finished = True
