#!/bin/sh
# Verify the historical -dk_ prefix still selects k_undither.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_ \
        <"${TOP_SRCDIR}/images/map8.six" >/dev/null || {
    echo "not ok" 1 - "legacy k_undither prefix rejected"
    exit 0
}

echo "ok" 1 - "accepts legacy k_undither prefix"
exit 0
