#!/bin/sh
# TAP test verifying update delta error accepts only documented choices.
# Policy: docs/functionality/delta-encoding.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
status_invalid=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep --update-policy=delta:error=diffuse \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid update delta error=diffuse was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep -Z delta:error=skip \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid -Z delta:error=skip was rejected"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep --update-policy=delta:error=carry \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_invalid=$?
set -e

test "${status_invalid}" -ne 0 || {
    echo "not ok" 1 - "invalid update delta error choice was accepted"
    exit 0
}

echo "ok" 1 - "update delta error validates documented choices"
exit 0
