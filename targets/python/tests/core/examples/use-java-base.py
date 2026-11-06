from typing import Protocol

class Runnable(Protocol):
    def run_(self) -> int: ...

def main() -> int:
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
