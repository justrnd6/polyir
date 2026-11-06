from dataclasses import dataclass
from typing import Protocol

class Runnable(Protocol):
    def run_(self) -> int: ...

@dataclass
class Job:
    id: int

    def run_(self) -> int:
        return self.id

def main() -> int:
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
