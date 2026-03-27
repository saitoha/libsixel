#!/bin/sh
# TAP test confirming forced builtin loader rejects corrupted PNG input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_png}" >/dev/null && {
    echo "not ok" 1 - "forced builtin loader accepted corrupted PNG"
    exit 0
}

echo "ok" 1 - "forced builtin loader rejects corrupted PNG"
exit 0
