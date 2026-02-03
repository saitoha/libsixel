#!/usr/bin/env python3
"""Generate deterministic fuzz input bytes from a numeric seed."""

from __future__ import annotations

import random
import sys


def main(argv: list[str]) -> int:
    """Write deterministic random bytes to the requested path."""
    if len(argv) != 3:
        print("usage: fuzz_seed_input.py <seed> <output_path>", file=sys.stderr)
        return 2

    try:
        seed = int(argv[1])
    except ValueError:
        print("seed must be an integer", file=sys.stderr)
        return 2

    output_path = argv[2]
    random.seed(seed)
    size = random.randint(1, 4096)
    payload = bytearray(random.getrandbits(8) for _ in range(size))
    with open(output_path, "wb") as handle:
        handle.write(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
