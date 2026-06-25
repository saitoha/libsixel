#!/bin/sh
# TAP test verifying 6delta-error accepts only documented choices.

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
    --transparent-policy=keep --6delta-error=diffuse \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid 6delta-error=diffuse was rejected"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep -Y skip \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid -Y skip was rejected"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --6delta-error=carry \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_invalid=$?
set -e

test "${status_invalid}" -ne 0 || {
    echo "not ok" 1 - "invalid 6delta-error choice was accepted"
    exit 0
}

echo "ok" 1 - "6delta-error validates documented choices"
exit 0
