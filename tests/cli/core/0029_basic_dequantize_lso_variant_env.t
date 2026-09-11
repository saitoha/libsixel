#!/bin/sh
# Verify the registered LSO variant environment value reaches dequantization.
# Policy: docs/functionality/dequantization.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

from_env=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_DEQUANTIZE_LSO_VARIANT=light -dlso_undither \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
from_cli=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -dlso_undither:Vlight \
    <"${TOP_SRCDIR}/images/map8.six" | cksum)
test "${from_env}" = "${from_cli}" || {
    echo "not ok" 1 - "LSO variant environment value changed output"
    exit 0
}

echo "ok" 1 - "LSO variant environment value is applied"
exit 0
