#!/bin/sh
# Verify accumulation-delta accepts only the documented byte range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
status=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --accumulation-delta=8 \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid accumulation-delta was rejected"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --accumulation-delta=256 \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status=$?
set -e
test "${status}" -ne 0 || {
    echo "not ok" 1 - "out-of-range accumulation-delta was accepted"
    exit 0
}

echo "ok" 1 - "accumulation-delta validates the byte range"
exit 0
