# A variant: the number of its case and the value it holds.
@dataclass
class Variant:
    tag: int
    value: object
