#!/bin/sh
# Verify update delta threshold accepts only the documented byte range.
# Policy: docs/functionality/delta-encoding.md

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
    --alpha-policy=keep --update-policy=delta:threshold=8 \
    -L builtin -e -o - "${input_image}" >/dev/null || {
    echo "not ok" 1 - "valid update delta threshold was rejected"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep --update-policy=delta:threshold=256 \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_256=$?
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep --update-policy=delta:threshold=-1 \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_negative=$?
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep --update-policy=delta:threshold=fast \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_text=$?
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=keep --update-policy=delta:threshold=8:error=skip \
    -L builtin -e -o - "${input_image}" >/dev/null 2>/dev/null
status_error_mode=$?
set -e
test "${status_256}" -ne 0 || {
    echo "not ok" 1 - "out-of-range update delta threshold was accepted"
    exit 0
}
test "${status_negative}" -ne 0 || {
    echo "not ok" 1 - "negative update delta threshold was accepted"
    exit 0
}
test "${status_text}" -ne 0 || {
    echo "not ok" 1 - "non-numeric update delta threshold was accepted"
    exit 0
}
test "${status_error_mode}" -eq 0 || {
    echo "not ok" 1 - "valid update delta error was rejected"
    exit 0
}

echo "ok" 1 - "update delta threshold validates the byte range"
exit 0
