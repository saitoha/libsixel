#!/bin/sh
# Verify 6delta-threshold accepts only the documented byte range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
status_256=0
status_negative=0
status_text=0
status_error_mode=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --6delta-threshold=8 \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid 6delta-threshold was rejected"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --6delta-threshold=256 \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_256=$?
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --6delta-threshold=-1 \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_negative=$?
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --6delta-threshold=fast \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_text=$?
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep --6delta-threshold=8 --6delta-error=skip \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_error_mode=$?
set -e
test "${status_256}" -ne 0 || {
    echo "not ok" 1 - "out-of-range 6delta-threshold was accepted"
    exit 0
}
test "${status_negative}" -ne 0 || {
    echo "not ok" 1 - "negative 6delta-threshold was accepted"
    exit 0
}
test "${status_text}" -ne 0 || {
    echo "not ok" 1 - "non-numeric 6delta-threshold was accepted"
    exit 0
}
test "${status_error_mode}" -eq 0 || {
    echo "not ok" 1 - "valid 6delta-error was rejected"
    exit 0
}

echo "ok" 1 - "6delta-threshold validates the byte range"
exit 0
