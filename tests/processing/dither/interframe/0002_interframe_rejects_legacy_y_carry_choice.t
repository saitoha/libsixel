#!/bin/sh
# TAP test confirming legacy -Y carry is still rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d interframe:scan=raster -Y carry -p 16 \
    -o /dev/null "${input_image}" >/dev/null 2>&1
status=$?
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "legacy -Y carry unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "legacy -Y carry is rejected"
exit 0
