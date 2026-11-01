#!/bin/sh
# TAP test checking oversized DCS geometry is tolerated.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

oversized="${TOP_SRCDIR}/tests/data/inputs/snake_64-oversized.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${oversized}" >/dev/null || {
    echo "not ok" 1 - "oversized DCS geometry rejected"
    exit 0
}

echo "ok" 1 - "oversized DCS geometry tolerated"
exit 0
