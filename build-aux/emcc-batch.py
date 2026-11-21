# SPDX-License-Identifier: MIT
"""Run the Windows SDK's emcc.bat with the arguments supplied by Meson."""

from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def main():
    # Ninja passes Windows argv quoting, which cmd.exe /c can reinterpret.
    # Keep the arguments in an emcc response file and pass a relative,
    # unquoted filename through cmd, within its command length limit.
    with tempfile.TemporaryDirectory(dir=".", prefix="emcc-") as directory:
        response = Path(Path(directory).name) / "args.rsp"
        response.write_text(shlex.join(sys.argv[1:]) + "\n", encoding="utf-8")
        return subprocess.call(["cmd.exe", "/d", "/c", "emcc.bat", "@" + str(response)])


if __name__ == "__main__":
    sys.exit(main())
