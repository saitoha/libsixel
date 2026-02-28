#!/bin/sh
# TAP test: quicklook does not hang on invalid APNG fdat sequence input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

test "${HAVE_PYTHON-}" = 1 || {
    printf "1..0 # SKIP python runtime is unavailable\n"
    exit 0
}

echo "1..1"
set -v

python3 -c 'import os
import subprocess
import sys

cmd = [
    os.environ["IMG2SIXEL_PATH"],
    "-L",
    "quicklook!",
    os.path.join(
        os.environ["TOP_SRCDIR"],
        "tests/data/inputs/formats/apng_invalid_libpng_fdat_sequence_gap.png",
    ),
]
env = dict(os.environ)
env["SIXEL_THUMBNAILER_HINT_SIZE"] = "64"
subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
               timeout=5, check=False)
sys.exit(0)
' || {
    echo "not ok" 1 "quicklook hang guard failed for invalid APNG fdat sequence"
    exit 0
}

echo "ok" 1 "quicklook does not hang on invalid APNG fdat sequence"
exit 0
