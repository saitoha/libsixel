#!/bin/sh
# Verify the compact Vlight alias matches the full spelling.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

vlight_short=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dl:Vl \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
vlight_full=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
        -dlso_undither:Vlight \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
test "${vlight_short}" = "${vlight_full}" || {
    echo "not ok" 1 - "compact Vlight alias changed output"
    exit 0
}

echo "ok" 1 - "compact Vlight alias matches full spelling"
exit 0
