# What `throw` raises: the value thrown.
class Thrown(Exception):
    def __init__(self, value):
        super().__init__(value)
        self.value = value
