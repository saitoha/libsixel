# SPDX-License-Identifier: MIT
"""Check Python build headers against their Autotools counterparts."""

import os
import subprocess
import sys


def check_generators(source_root, awk):
    env = dict(os.environ, LC_ALL="C")

    def generate(*args):
        output = subprocess.check_output(args, cwd=source_root, env=env)
        return output.replace(b"\r\n", b"\n")

    idl = "include/6cells.idl"
    actual = generate(sys.executable, "tools/gen_6cells_h.py", idl)
    expected = generate(awk, "-f", "tools/gen_6cells_h.awk", idl)
    # The header documents the generator to use for each build system.
    if actual.replace(b"tools/gen_6cells_h.py from", b"tools/gen_6cells_h.awk from", 1) != expected:
        print(f"{idl}: Python and AWK generated different declarations", file=sys.stderr)
        return 1

    for shader in ("src/lookup-policy-blockcull-gpu.metal", "src/dither-policy-sierra2-gpu.glsl"):
        # Skip the license and provenance lines before comparing the literals.
        actual = generate(sys.executable, "tools/embed-shader.py", shader, "-").split(b"\n", 2)[2]
        expected = generate(awk, "-f", "tools/embed-metal.awk", shader).split(b"\n", 2)[2]
        if actual != expected:
            print(f"{shader}: Python and AWK generated different literals", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(check_generators(sys.argv[1], sys.argv[2]))
