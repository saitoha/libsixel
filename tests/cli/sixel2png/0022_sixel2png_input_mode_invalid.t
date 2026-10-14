#!/bin/sh
# Policy: docs/decoder/multi-pass.md
set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}
test -n "${LSQA_PATH-}" && test -x "${LSQA_PATH}" || {
    echo "1..0 # SKIP lsqa is disabled"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M "" \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - invalid input mode was not rejected: "
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M "Single" \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - invalid input mode was not rejected: Single"
    exit 0
}

status=0
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M "6delta-progressive-extra" \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" >/dev/null 2>&1 || status=$?
test "${status}" -eq 2 || {
    echo "not ok 1 - invalid input mode was not rejected: 6delta-progressive-extra"
    exit 0
}

echo "ok 1 - invalid input modes remain usage errors"
exit 0
