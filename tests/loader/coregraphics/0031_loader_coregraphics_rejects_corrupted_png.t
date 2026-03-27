#!/bin/sh
# TAP test: coregraphics loader rejects corrupted PNG stream safely.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/corrupted/truncated.png" >/dev/null && {
    echo "not ok" 1 - "corrupted PNG unexpectedly decoded by coregraphics"
    exit 0
}

echo "ok" 1 - "coregraphics rejects corrupted PNG without crashing"
exit 0
