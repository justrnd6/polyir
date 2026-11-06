# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object


@dataclass
class Err:
    value: object
